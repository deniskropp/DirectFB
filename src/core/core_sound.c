/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <pthread.h>

#include <sys/soundcard.h>

#include <direct/build.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <fusion/arena.h>
#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/object.h>

#include <misc/conf.h>   /* FIXME */

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

#define DIRECTFB_CORE_ABI     22   /* FIXME */

/******************************************************************************/

typedef struct {
     DirectLink           link;

     CorePlayback        *playback;
} CorePlaylistEntry;

struct __FS_CoreSoundShared {
     FusionObjectPool    *buffer_pool;
     FusionObjectPool    *playback_pool;

     struct {
          DirectLink     *entries;
          FusionSkirmish  lock;
     } playlist;

     struct {
          int             fmt;               /* hack */
          int             bits;              /* hack */
          long            rate;              /* hack */
          int             channels;          /* hack */
          int             block_size;        /* hack */
          int             samples_per_block; /* hack */
     } config;

     int                  output_delay;      /* output buffer size in ms */
};

struct __FS_CoreSound {
     int              refs;

     int              fusion_id;

     FusionArena     *arena;

     CoreSoundShared *shared;

     bool             master;
     int              fd;
     DirectThread    *sound_thread;
};

/******************************************************************************/

static int fs_core_arena_initialize( FusionArena *arena,
                                     void        *ctx );
static int fs_core_arena_join      ( FusionArena *arena,
                                     void        *ctx );
static int fs_core_arena_leave     ( FusionArena *arena,
                                     void        *ctx,
                                     bool         emergency);
static int fs_core_arena_shutdown  ( FusionArena *arena,
                                     void        *ctx,
                                     bool         emergency);

/******************************************************************************/

static CoreSound       *core_sound      = NULL;
static pthread_mutex_t  core_sound_lock = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;

DFBResult
fs_core_create( CoreSound **ret_core )
{
     int        ret;
     int        world;
#if FUSION_BUILD_MULTI
     char       buf[16];
#endif
     CoreSound *core;

     D_ASSERT( ret_core != NULL );

     D_DEBUG( "FusionSound/Core: %s...\n", __FUNCTION__ );

     /* Lock the core singleton mutex. */
     pthread_mutex_lock( &core_sound_lock );

     /* Core already created? */
     if (core_sound) {
          /* Increase its references. */
          core_sound->refs++;

          /* Return the core. */
          *ret_core = core_sound;

          /* Unlock the core singleton mutex. */
          pthread_mutex_unlock( &core_sound_lock );

          return DFB_OK;
     }

     /* Allocate local core structure. */
     core = D_CALLOC( 1, sizeof(CoreSound) );
     if (!core) {
          pthread_mutex_unlock( &core_sound_lock );
          return DFB_NOSYSTEMMEMORY;
     }

     core->fusion_id = fusion_init( dfb_config->session,
#if DIRECT_BUILD_DEBUG
                                    -DIRECTFB_CORE_ABI,
#else
                                    DIRECTFB_CORE_ABI,
#endif
                                    &world );
     if (core->fusion_id < 0) {
          D_FREE( core );
          pthread_mutex_unlock( &core_sound_lock );
          return DFB_FUSION;
     }

#if FUSION_BUILD_MULTI
     D_DEBUG( "FusionSound/Core: world %d, fusion id %d\n", world, core->fusion_id );

     snprintf( buf, sizeof(buf), "%d", world );

     setenv( "DIRECTFB_SESSION", buf, true );
#endif

     /* Initialize the references. */
     core->refs = 1;

     /* Enter the FusionSound core arena. */
     if (fusion_arena_enter( "FusionSound/Core",
                             fs_core_arena_initialize, fs_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          D_FREE( core );
          pthread_mutex_unlock( &core_sound_lock );
          return ret ? ret : DFB_FUSION;
     }

     /* Return the core and store the singleton. */
     *ret_core = core_sound = core;

     /* Unlock the core singleton mutex. */
     pthread_mutex_unlock( &core_sound_lock );

     return DFB_OK;
}

DFBResult
fs_core_destroy( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core == core_sound );

     D_DEBUG( "FusionSound/Core: %s...\n", __FUNCTION__ );

     /* Lock the core singleton mutex. */
     pthread_mutex_lock( &core_sound_lock );

     /* Decrement and check references. */
     if (--core->refs) {
          /* Unlock the core singleton mutex. */
          pthread_mutex_unlock( &core_sound_lock );

          return DFB_OK;
     }

     /* Exit the FusionSound core arena. */
     if (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                            core->master ? NULL : fs_core_arena_leave,
                            core, false, NULL ) == DFB_BUSY)
     {
          D_WARN( "forking to wait until all slaves terminated" );

          /* FIXME: quick hack to solve the dfb-slave-but-fs-master problem. */
          switch (fork()) {
               case -1:
                    D_PERROR( "FusionSound/Core: fork() failed!\n" );

                    while (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                                              core->master ? NULL : fs_core_arena_leave,
                                              core, false, NULL ) == DFB_BUSY)
                         usleep( 100000 );

                    break;

               case 0:
                    while (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                                              core->master ? NULL : fs_core_arena_leave,
                                              core, false, NULL ) == DFB_BUSY)
                         usleep( 100000 );

                    _exit(0);
          }
     }

     fusion_exit( false );

     /* Deallocate local core structure. */
     D_FREE( core );

     /* Clear the singleton. */
     core_sound = NULL;

     /* Unlock the core singleton mutex. */
     pthread_mutex_unlock( &core_sound_lock );

     return DFB_OK;
}

CoreSoundBuffer *
fs_core_create_buffer( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->buffer_pool != NULL );

     /* Create a new object in the buffer pool. */
     return (CoreSoundBuffer*) fusion_object_create( core->shared->buffer_pool );
}

CorePlayback *
fs_core_create_playback( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->playback_pool != NULL );

     /* Create a new object in the playback pool. */
     return (CorePlayback*) fusion_object_create( core->shared->playback_pool );
}

DirectResult
fs_core_playlist_lock( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     return fusion_skirmish_prevail( &core->shared->playlist.lock );
}

DirectResult
fs_core_playlist_unlock( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     return fusion_skirmish_dismiss( &core->shared->playlist.lock );
}

DFBResult
fs_core_add_playback( CoreSound    *core,
                      CorePlayback *playback )
{
     CorePlaylistEntry *entry;
     CoreSoundShared   *shared;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( playback != NULL );

     D_DEBUG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, playback );

     shared = core->shared;

     /* Allocate playlist entry. */
     entry = SHCALLOC( 1, sizeof(CorePlaylistEntry) );
     if (!entry)
          return DFB_NOSYSTEMMEMORY;

     /* Link playback to playlist entry. */
     if (fs_playback_link( &entry->playback, playback )) {
          SHFREE( entry );
          return DFB_FUSION;
     }

     /* Add it to the playback list. */
     direct_list_prepend( &shared->playlist.entries, &entry->link );

     return DFB_OK;
}

DFBResult
fs_core_remove_playback( CoreSound    *core,
                         CorePlayback *playback )
{
     DirectLink      *l, *next;
     CoreSoundShared *shared;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( playback != NULL );

     D_DEBUG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, playback );

     shared = core->shared;

     /* Lookup playback in the list. */
     direct_list_foreach_safe (l, next, shared->playlist.entries) {
          CorePlaylistEntry *entry = (CorePlaylistEntry*) l;

          /* Remove any matches. */
          if (entry->playback == playback) {
               direct_list_remove( &shared->playlist.entries, l );

               fs_playback_unlink( &entry->playback );

               SHFREE( entry );
          }
     }

     return DFB_OK;
}

int
fs_core_output_delay( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     /* Return the delay produced by the device buffer. */
     return core->shared->output_delay;
}

/******************************************************************************/

static void *
sound_thread( DirectThread *thread, void *arg )
{
     CoreSound       *core    = arg;
     CoreSoundShared *shared  = core->shared;
     int              samples = shared->config.samples_per_block;

     __s16            output[samples];
     int              mixing[samples];

     int              byte_rate = (shared->config.rate *
                                   shared->config.channels *
                                   (shared->config.bits >> 3));

     bool             empty = true;


     while (true) {
          int             i;
          audio_buf_info  info;
          DirectLink     *next, *l;

          direct_thread_testcancel( thread );

          if (! ioctl( core->fd, SNDCTL_DSP_GETOSPACE, &info )) {
               int buffered = info.fragsize * info.fragstotal - info.bytes;

               if (!buffered && !empty)
                    D_WARN( "device buffer underrun?" );

               /* calculate output delay (ms) */
               shared->output_delay = buffered * 1000 / byte_rate;

               /* do not buffer more than 80 ms */
               if (buffered > byte_rate * 80 / 1000) {
                    D_DEBUG( "FusionSound/Core: %s sleeping...\n", __FUNCTION__ );
                    usleep( 10000 );
                    continue;
               }
          }
          else
               D_ONCE( "SNDCTL_DSP_GETOSPACE failed!" );

          empty = !shared->playlist.entries;
          if (empty) {
               usleep( 20000 );
               continue;
          }

          /* Clear mixing buffer. */
          memset( mixing, 0, sizeof(int) * samples );

          /* Iterate through running playbacks, mixing them together. */
          fusion_skirmish_prevail( &shared->playlist.lock );

          direct_list_foreach_safe (l, next, shared->playlist.entries) {
               DFBResult          ret;
               CorePlaylistEntry *entry    = (CorePlaylistEntry *) l;
               CorePlayback      *playback = entry->playback;

               ret = fs_playback_mixto( playback, mixing,
                                        shared->config.rate, samples );
               if (ret) {
                    direct_list_remove( &shared->playlist.entries, l );

                    fs_playback_unlink( &entry->playback );

                    SHFREE( entry );
               }
          }

          fusion_skirmish_dismiss( &shared->playlist.lock );

          /* Convert mixing buffer to output format, clipping each sample. */
          for (i=0; i<samples; i++) {
               register int sample = mixing[i];

               if (sample > 32767)
                    sample = 32767;
               else if (sample < -32767)
                    sample = -32767;

               output[i] = sample;
          }

          write( core->fd, output, shared->config.block_size );
     }

     return NULL;
}

/******************************************************************************/

static DFBResult
fs_core_initialize( CoreSound *core )
{
     int              fd;
#ifdef APF_NORMAL
     int              prof   = APF_NORMAL;
#endif
     CoreSoundShared *shared = core->shared;
     int              fmt    = shared->config.fmt;
     int              bits   = shared->config.bits;
     int              bytes  = (bits + 7) / 8;
     int              stereo = (shared->config.channels > 1) ? 1 : 0;
     int              rate   = shared->config.rate;

     /* open sound device */
     fd = direct_try_open( "/dev/dsp", "/dev/sound/dsp", O_WRONLY );
     if (fd < 0)
          return DFB_INIT;

     /* set application profile */
#ifdef SNDCTL_DSP_PROFILE
     ioctl( fd, SNDCTL_DSP_PROFILE, &prof );
#endif
     /* set bits per sample */
     ioctl( fd, SNDCTL_DSP_SETFMT, &fmt );
     if (fmt != shared->config.fmt) {
          D_ERROR( "FusionSound/Core: "
                   "Unable to set bits to '%d'!\n", shared->config.bits );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set mono/stereo */
     if (ioctl( fd, SNDCTL_DSP_STEREO, &stereo ) == -1) {
          D_ERROR( "FusionSound/Core: Unable to set '%s' mode!\n",
                   (shared->config.channels > 1) ? "stereo" : "mono");
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set sample rate */
     if (ioctl( fd, SNDCTL_DSP_SPEED, &rate ) == -1) {
          D_ERROR( "FusionSound/Core: "
                   "Unable to set rate to '%ld'!\n", shared->config.rate );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     shared->config.rate = rate;

     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &shared->config.block_size );
     if (shared->config.block_size < 1) {
          D_ERROR( "FusionSound/Core: "
                   "Unable to query block size of '/dev/dsp'!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     D_DEBUG( "FusionSound/Core: got block size %d\n", shared->config.block_size );

     if (shared->config.block_size < 4096)
          shared->config.block_size = 4096;
     else if (shared->config.block_size > 8192)
          shared->config.block_size = 8192;

     D_DEBUG( "FusionSound/Core: using block size %d\n", shared->config.block_size );

     /* calculate number of samples fitting into one block */
     shared->config.samples_per_block = shared->config.block_size / bytes;

     /* store file descriptor */
     core->fd = fd;

     /* initialize playback list lock */
     fusion_skirmish_init( &shared->playlist.lock );

     /* create a pool for sound buffer objects */
     shared->buffer_pool = fs_buffer_pool_create();

     /* create a pool for playback objects */
     shared->playback_pool = fs_playback_pool_create();

     /* create sound thread */
     core->sound_thread = direct_thread_create( DTT_CRITICAL, sound_thread, core, "Sound Mixer" );

     return DFB_OK;
}

static DFBResult
fs_core_join( CoreSound *core )
{
     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
fs_core_leave( CoreSound *core )
{
     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
fs_core_shutdown( CoreSound *core )
{
     DirectLink      *l, *next;
     CoreSoundShared *shared;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     shared = core->shared;

     D_ASSERT( shared->buffer_pool != NULL );
     D_ASSERT( shared->playback_pool != NULL );

     /* stop sound thread */
     direct_thread_cancel( core->sound_thread );
     direct_thread_join( core->sound_thread );
     direct_thread_destroy( core->sound_thread );

     /* flush pending sound data so we don't have to wait */
     ioctl( core->fd, SNDCTL_DSP_RESET, 0 );

     /* close sound device */
     close( core->fd );

     /* clear playback list */
     fusion_skirmish_prevail( &shared->playlist.lock );

     direct_list_foreach_safe (l, next, shared->playlist.entries) {
          CorePlaylistEntry *entry = (CorePlaylistEntry*) l;

          fs_playback_unlink( &entry->playback );

          SHFREE( entry );
     }

     /* destroy playback object pool */
     fusion_object_pool_destroy( shared->playback_pool );

     /* destroy buffer object pool */
     fusion_object_pool_destroy( shared->buffer_pool );

     /* destroy playlist lock */
     fusion_skirmish_destroy( &shared->playlist.lock );

     return DFB_OK;
}

/******************************************************************************/

static int
fs_core_arena_initialize( FusionArena *arena,
                          void        *ctx )
{
     DFBResult        ret;
     CoreSound       *core   = ctx;
     CoreSoundShared *shared;

     D_DEBUG( "FusionSound/Core: Initializing...\n" );

/*     if (!dfb_core_is_master()) {
          ERRORMSG( "FusionSound/Core: Only master can initialize for now!\n" );
          return DFB_INIT;
     }*/

     /* Allocate shared structure. */
     shared = SHCALLOC( 1, sizeof(CoreSoundShared) );
     if (!shared) {
          D_ERROR( "FusionSound/Core: Could not allocate (shared) memory!\n" );
          return DFB_NOSYSTEMMEMORY;
     }

     core->shared = shared;
     core->master = true;

     /* FIXME: add live configuration */
#ifdef WORDS_BIGENDIAN
     shared->config.fmt      = AFMT_S16_BE;
#else
     shared->config.fmt      = AFMT_S16_LE;
#endif
     shared->config.bits     = 16;
     shared->config.channels = 2;
     shared->config.rate     = 48000;

     /* Initialize. */
     ret = fs_core_initialize( core );
     if (ret) {
          SHFREE( shared );
          return ret;
     }

     /* Register shared data. */
     fusion_arena_add_shared_field( arena, "Core/Shared", shared );

     return DFB_OK;
}

static int
fs_core_arena_join( FusionArena *arena,
                    void        *ctx )
{
     DFBResult        ret;
     CoreSound       *core   = ctx;
     CoreSoundShared *shared;

     D_DEBUG( "FusionSound/Core: Joining...\n" );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", (void**)&shared ))
          return DFB_FUSION;

     core->shared = shared;

     /* Join. */
     ret = fs_core_join( core );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
fs_core_arena_leave( FusionArena *arena,
                     void        *ctx,
                     bool         emergency)
{
     DFBResult  ret;
     CoreSound *core = ctx;

     D_DEBUG( "FusionSound/Core: Leaving...\n" );

     /* Leave. */
     ret = fs_core_leave( core );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
fs_core_arena_shutdown( FusionArena *arena,
                        void        *ctx,
                        bool         emergency)
{
     DFBResult  ret;
     CoreSound *core = ctx;

     D_DEBUG( "FusionSound/Core: Shutting down...\n" );

     if (!core->master) {
          D_DEBUG( "FusionSound/Core: Refusing shutdown in slave.\n" );
          return fs_core_leave( core );
     }

     /* Shutdown. */
     ret = fs_core_shutdown( core );
     if (ret)
          return ret;

     return DFB_OK;
}

