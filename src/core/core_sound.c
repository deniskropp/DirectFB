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

#include <linux/soundcard.h>

#include <core/coredefs.h>
#include <core/core.h>
#include <core/fusion/arena.h>
#include <core/fusion/list.h>
#include <core/fusion/shmalloc.h>
#include <core/fusion/object.h>
#include <core/thread.h>
#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/util.h>

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

/******************************************************************************/

typedef struct {
     FusionLink           link;

     CorePlayback        *playback;
} CorePlaylistEntry;

struct __FS_CoreSoundShared {
     FusionObjectPool    *buffer_pool;
     FusionObjectPool    *playback_pool;
     
     struct {
          FusionLink     *entries;
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
};

struct __FS_CoreSound {
     int              refs;

     FusionArena     *arena;

     CoreSoundShared *shared;

     bool             master;
     int              fd;
     CoreThread      *sound_thread;
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
static pthread_mutex_t  core_sound_lock = PTHREAD_MUTEX_INITIALIZER;

DFBResult
fs_core_create( CoreSound **ret_core )
{
     int        ret;
     CoreSound *core;

     DFB_ASSERT( ret_core != NULL );

     DEBUGMSG( "FusionSound/core: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &core_sound_lock );
     
     if (core_sound) {
          core_sound->refs++;

          *ret_core = core_sound;

          pthread_mutex_unlock( &core_sound_lock );
          
          return DFB_OK;
     }

     /* Allocate local core structure. */
     core = DFBCALLOC( 1, sizeof(CoreSound) );
     if (!core) {
          pthread_mutex_unlock( &core_sound_lock );

          return DFB_NOSYSTEMMEMORY;
     }

     core->refs = 1;

     if (fusion_arena_enter( "FusionSound/Core",
                             fs_core_arena_initialize, fs_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          DFBFREE( core );
          
          pthread_mutex_unlock( &core_sound_lock );
          
          return ret ? ret : DFB_FUSION;
     }

     *ret_core = core_sound = core;

     pthread_mutex_unlock( &core_sound_lock );
     
     return DFB_OK;
}

DFBResult
fs_core_destroy( CoreSound *core )
{
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core == core_sound );

     DEBUGMSG( "FusionSound/Core: %s...\n", __FUNCTION__ );
     
     pthread_mutex_lock( &core_sound_lock );
     
     if (--core->refs) {
          pthread_mutex_unlock( &core_sound_lock );

          return DFB_OK;
     }

     while (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                               core->master ? NULL : fs_core_arena_leave,
                               core, false, NULL ) == FUSION_INUSE)
     {
          /* FIXME: quick hack to solve the dfb-slave-but-fs-master problem. */
          
          ONCE( "waiting for sound slaves to terminate" );

          usleep( 100000 );
     }
     
     DFBFREE( core );

     core_sound = NULL;

     pthread_mutex_unlock( &core_sound_lock );
     
     return DFB_OK;
}

CoreSoundBuffer *
fs_core_create_buffer( CoreSound *core )
{
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->buffer_pool != NULL );

     return (CoreSoundBuffer*)fusion_object_create( core->shared->buffer_pool );
}

CorePlayback *
fs_core_create_playback( CoreSound *core )
{
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->playback_pool != NULL );

     return (CorePlayback*) fusion_object_create( core->shared->playback_pool );
}

DFBResult
fs_core_add_playback( CoreSound    *core,
                      CorePlayback *playback )
{
     CorePlaylistEntry *entry;
     CoreSoundShared   *shared;
     
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( playback != NULL );
     
     DEBUGMSG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, playback );

     /* Allocate playlist entry. */
     entry = SHCALLOC( 1, sizeof(CorePlaylistEntry) );
     if (!entry)
          return DFB_NOSYSTEMMEMORY;

     /* Link playback to playlist entry. */
     if (fs_playback_link( &entry->playback, playback )) {
          SHFREE( entry );
          return DFB_FUSION;
     }

     /* Add it to the list. */
     shared = core->shared;
     
     fusion_skirmish_prevail( &shared->playlist.lock );
     fusion_list_prepend( &shared->playlist.entries, &entry->link );
     fusion_skirmish_dismiss( &shared->playlist.lock );

     return DFB_OK;
}

DFBResult
fs_core_remove_playback( CoreSound    *core,
                         CorePlayback *playback )
{
     FusionLink      *l, *next;
     CoreSoundShared *shared;
     
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( playback != NULL );
     
     DEBUGMSG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, playback );

     shared = core->shared;
     
     fusion_skirmish_prevail( &shared->playlist.lock );
     
     /* Lookup playback in the list. */
     fusion_list_foreach_safe (l, next, shared->playlist.entries) {
          CorePlaylistEntry *entry = (CorePlaylistEntry*) l;

          /* Remove any matches. */
          if (entry->playback == playback) {
               fs_playback_unlink( playback );

               fusion_list_remove( &shared->playlist.entries, l );

               SHFREE( entry );
          }
     }
     
     fusion_skirmish_dismiss( &shared->playlist.lock );

     return DFB_OK;
}

/******************************************************************************/

static void *
sound_thread( CoreThread *thread, void *arg )
{
     CoreSound       *core    = arg;
     CoreSoundShared *shared  = core->shared;
     int              samples = shared->config.samples_per_block;
     
     __s16            output[samples];
     int              mixing[samples];

     int              written    = 0;
     long long        last_time  = 0;

     int              total_rate = shared->config.rate *
                                   shared->config.channels;
     
     while (true) {
          int         i;
          FusionLink *l, *next;

          dfb_thread_testcancel( thread );
          
          if (last_time) {
               int       played;
               long long this_time = dfb_get_micros();
               long long diff_time = this_time - last_time;

               played = (int)(total_rate * diff_time / 1000000LL);
               
               written -= played;

               if (written < 0) {
                    CAUTION( "buffer underrun" );
                    written = 0;
               }

               last_time = this_time;

               /* do not buffer more than 60 ms */
               if (written > total_rate * 60 / 1000)
                    usleep( 20000 );
               
               dfb_thread_testcancel( thread );
          }

          /* Clear mixing buffer. */
          memset( mixing, 0, sizeof(int) * samples );

          /* Iterate through running playbacks mixing them together. */
          fusion_skirmish_prevail( &shared->playlist.lock );

          fusion_list_foreach_safe (l, next, shared->playlist.entries) {
               DFBResult          ret;
               CorePlaylistEntry *entry    = (CorePlaylistEntry *) l;
               CorePlayback      *playback = entry->playback;
               
               ret = fs_playback_mixto( playback, mixing,
                                        shared->config.rate, samples );
               if (ret) {
                    fs_playback_unlink( playback );

                    fusion_list_remove( &shared->playlist.entries, l );

                    SHFREE( entry );
               }
          }
          
          fusion_skirmish_dismiss( &shared->playlist.lock );
          
          /* Convert mixing buffer to output format clipping each sample. */
          for (i=0; i<samples; i++) {
               register int sample = mixing[i];

               if (sample > 32767)
                    sample = 32767;
               else if (sample < -32767)
                    sample = -32767;

               output[i] = sample;
          }
          
          if (!last_time)
               last_time = dfb_get_micros();
          
          write( core->fd, output, shared->config.block_size );

          written += samples;
     }

     return NULL;
}

/******************************************************************************/

static DFBResult
fs_core_initialize( CoreSound *core )
{
     int              fd;
     int              prof   = APF_NORMAL;
     CoreSoundShared *shared = core->shared;
     int              fmt    = shared->config.fmt;
     int              bits   = shared->config.bits;
     int              bytes  = (bits + 7) / 8;
     int              stereo = (shared->config.channels > 1) ? 1 : 0;
     int              rate   = shared->config.rate;

     /* open sound device */
     fd = dfb_try_open( "/dev/dsp", "/dev/sound/dsp", O_WRONLY );
     if (fd < 0)
          return DFB_INIT;

     /* set application profile */
     ioctl( fd, SNDCTL_DSP_PROFILE, &prof );

     /* set bits per sample */
     ioctl( fd, SNDCTL_DSP_SETFMT, &fmt );
     if (fmt != shared->config.fmt) {
          ERRORMSG( "FusionSound/Core: "
                    "Unable to set bits to '%d'!\n", shared->config.bits );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set mono/stereo */
     if (ioctl( fd, SNDCTL_DSP_STEREO, &stereo ) == -1) {
          ERRORMSG( "FusionSound/Core: Unable to set '%s' mode!\n",
                    (shared->config.channels > 1) ? "stereo" : "mono");
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set sample rate */
     if (ioctl( fd, SNDCTL_DSP_SPEED, &rate ) == -1) {
          ERRORMSG( "FusionSound/Core: "
                    "Unable to set rate to '%ld'!\n", shared->config.rate );
          close( fd );
          return DFB_UNSUPPORTED;
     }
     
     shared->config.rate = rate;

     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &shared->config.block_size );
     if (shared->config.block_size < 1) {
          ERRORMSG( "FusionSound/Core: "
                    "Unable to query block size of '/dev/dsp'!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     if (shared->config.block_size > 4096)
          shared->config.block_size = 4096;

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
     core->sound_thread = dfb_thread_create( CTT_CRITICAL, sound_thread, core );
     
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
     FusionLink      *l, *next;
     CoreSoundShared *shared;
     
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     
     shared = core->shared;
     
     DFB_ASSERT( shared->buffer_pool != NULL );
     DFB_ASSERT( shared->playback_pool != NULL );
     
     /* stop sound thread */
     dfb_thread_cancel( core->sound_thread );
     dfb_thread_join( core->sound_thread );
     dfb_thread_destroy( core->sound_thread );

     /* flush pending sound data so we don't have to wait */
     ioctl( core->fd, SNDCTL_DSP_RESET, 0 );
     
     /* close sound device */
     close( core->fd );

     /* clear playback list */
     fusion_skirmish_prevail( &shared->playlist.lock );

     fusion_list_foreach_safe (l, next, shared->playlist.entries) {
          CorePlaylistEntry *entry = (CorePlaylistEntry*) l;

          fs_playback_unlink( entry->playback );

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

     DEBUGMSG( "FusionSound/Core: Initializing...\n" );

/*     if (!dfb_core_is_master()) {
          ERRORMSG( "FusionSound/Core: Only master can initialize for now!\n" );
          return DFB_INIT;
     }*/

     /* Allocate shared structure. */
     shared = SHCALLOC( 1, sizeof(CoreSoundShared) );
     if (!shared) {
          ERRORMSG( "FusionSound/Core: Could not allocate (shared) memory!\n" );
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

     DEBUGMSG( "FusionSound/Core: Joining...\n" );

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

     DEBUGMSG( "FusionSound/Core: Leaving...\n" );

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

     DEBUGMSG( "FusionSound/Core: Shutting down...\n" );

     if (!core->master) {
          DEBUGMSG( "FusionSound/Core: Refusing shutdown in slave.\n" );
          return fs_core_leave( core );
     }

     /* Shutdown. */
     ret = fs_core_shutdown( core );
     if (ret)
          return ret;

     return DFB_OK;
}

