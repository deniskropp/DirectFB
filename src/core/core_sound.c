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
#include <core/sound_buffer.h>

/******************************************************************************/

typedef struct {
     FusionLink       link;

     CoreSoundBuffer *buffer;

     int              pos;
     __u16            pan;
     bool             loop;
} CoreSoundPlayback;

struct __FS_CoreSoundShared {
     FusionObjectPool    *buffer_pool;
     
     struct {
          FusionLink     *list;
          FusionSkirmish  lock;
     } playbacks;
     
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
          /* FIXME: quick hack to solve the dfb-slave-but-da-master problem. */
          
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

DFBResult
fs_core_add_playback( CoreSound       *core,
                      CoreSoundBuffer *buffer,
                      int              pos,
                      __u16            pan,
                      bool             loop )
{
     CoreSoundShared   *shared;
     CoreSoundPlayback *playback;
     
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( pos >= 0 );
     
     DEBUGMSG( "FusionSound/Core: %s...\n", __FUNCTION__ );

     shared = core->shared;

     playback = shcalloc( 1, sizeof(CoreSoundPlayback) );
     if (!playback)
          return DFB_NOSYSTEMMEMORY;

     playback->pos  = pos;
     playback->pan  = pan;
     playback->loop = loop;

     if (fs_buffer_link( &playback->buffer, buffer )) {
          shfree( playback );
          return DFB_FUSION;
     }

     fusion_skirmish_prevail( &shared->playbacks.lock );
     fusion_list_prepend( &shared->playbacks.list, &playback->link );
     fusion_skirmish_dismiss( &shared->playbacks.lock );

     return DFB_OK;
}

DFBResult
fs_core_remove_playbacks( CoreSound       *core,
                          CoreSoundBuffer *buffer )
{
     FusionLink      *l, *next;
     CoreSoundShared *shared;
     
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( buffer != NULL );
     
     DEBUGMSG( "FusionSound/Core: %s...\n", __FUNCTION__ );

     shared = core->shared;

     fusion_skirmish_prevail( &shared->playbacks.lock );

     fusion_list_foreach_safe (l, next, shared->playbacks.list) {
          CoreSoundPlayback *playback = (CoreSoundPlayback*) l;

          if (playback->buffer == buffer) {
               fs_buffer_unlink( playback->buffer );

               fusion_list_remove( &shared->playbacks.list, l );
               
               shfree( playback );
          }
     }
     
     fusion_skirmish_dismiss( &shared->playbacks.lock );

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
          
          //DEBUGMSG("written %d\n", written);
          
          if (last_time) {
               int       played;
               long long this_time = dfb_get_micros();
               long long diff_time = this_time - last_time;

               played = diff_time * total_rate / 1000000;
               
               written -= played;

               if (written < 0) {
                    CAUTION( "buffer underrun" );

                    written   = 0;
                    last_time = 0;
               }

               //DEBUGMSG("     -> %d (played %d)\n", written, played);
               
               last_time = this_time;

               /* do not buffer more than 150 ms */
               if (written > total_rate * 150 / 1000)
                    usleep( 20000 );
               
               dfb_thread_testcancel( thread );
          }

          memset( mixing, 0, sizeof(int) * samples );

          fusion_skirmish_prevail( &shared->playbacks.lock );

          fusion_list_foreach_safe (l, next, shared->playbacks.list) {
               DFBResult          ret;
               CoreSoundPlayback *playback = (CoreSoundPlayback *) l;
               CoreSoundBuffer   *buffer   = playback->buffer;
               
               ret = fs_buffer_mixto( buffer, playback->pos,
                                      mixing, samples, playback->pan,
                                      playback->loop, &playback->pos );
               
               fs_buffer_playback_notify( buffer, ret ? CABNF_PLAYBACK_ENDED :
                                          CABNF_PLAYBACK_ADVANCED,
                                          playback->pos );
               
               if (ret) {
                    fs_buffer_unlink( playback->buffer );

                    fusion_list_remove( &shared->playbacks.list, l );

                    shfree( playback );
               }
          }
          
          fusion_skirmish_dismiss( &shared->playbacks.lock );
          
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
     fd = dfb_try_open( "/dev/dsp", "/dev/sound/dsp", O_RDWR );
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

     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &shared->config.block_size );
     if (shared->config.block_size < 1) {
          ERRORMSG( "FusionSound/Core: "
                    "Unable to query block size of '/dev/dsp'!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     if (shared->config.block_size > 1024)
          shared->config.block_size = 1024;

     /* calculate number of samples fitting into one block */
     shared->config.samples_per_block = shared->config.block_size / bytes;

     /* store file descriptor */
     core->fd = fd;

     /* initialize playback list lock */
     fusion_skirmish_init( &shared->playbacks.lock );

     /* create a pool for sound buffer objects */
     shared->buffer_pool = fs_buffer_pool_create();

     /* create sound thread */
     core->sound_thread = dfb_thread_create( CTT_CRITICAL, sound_thread, core );
     
     return DFB_OK;
}

static DFBResult
fs_core_join( CoreSound *core )
{
     return DFB_OK;
}

static DFBResult
fs_core_leave( CoreSound *core )
{
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
     
     /* stop sound thread */
     dfb_thread_cancel( core->sound_thread );
     dfb_thread_join( core->sound_thread );
     dfb_thread_destroy( core->sound_thread );

     /* flush pending sound data so we don't have to wait */
     ioctl( core->fd, SNDCTL_DSP_RESET, 0 );
     
     /* close sound device */
     close( core->fd );

     /* clear playback list */
     fusion_skirmish_prevail( &shared->playbacks.lock );

     fusion_list_foreach_safe (l, next, shared->playbacks.list) {
          CoreSoundPlayback *playback = (CoreSoundPlayback*) l;

          fs_buffer_unlink( playback->buffer );

          shfree( playback );
     }
     
     /* destroy buffer object pool */
     fusion_object_pool_destroy( shared->buffer_pool );

     /* destroy playback list lock */
     fusion_skirmish_destroy( &shared->playbacks.lock );
     
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
     shared = shcalloc( 1, sizeof(CoreSoundShared) );
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
     shared->config.rate     = 44100;

     /* Initialize. */
     ret = fs_core_initialize( core );
     if (ret) {
          shfree( shared );
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
     DFBResult  ret;
     CoreSound *core   = ctx;
     void      *shared;

     DEBUGMSG( "FusionSound/Core: Joining...\n" );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", &shared ))
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

