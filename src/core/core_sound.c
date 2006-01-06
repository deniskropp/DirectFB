/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <direct/signals.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <fusion/arena.h>
#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/object.h>

#include <misc/conf.h>

#include <misc/fs_config.h>

#include <core/core.h>   /* FIXME */

#include <core/fs_types.h>
#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

/******************************************************************************/

#ifdef WORDS_BIGENDIAN
# ifndef AFMT_S24_BE
#  define AFMT_S24_BE 0x00001000
# endif
# ifndef AFMT_S32_BE
#  define AFMT_S32_BE 0x00010000
# endif
# define AFMT_S16  AFMT_S16_BE
# define AFMT_S24  AFMT_S24_BE
# define AFMT_S32  AFMT_S32_BE
#else
# ifndef AFMT_S24_LE
#  define AFMT_S24_LE 0x00000800
# endif
# ifndef AFMT_S32_LE
#  define AFMT_S32_LE 0x00008000
# endif
# define AFMT_S16  AFMT_S16_LE
# define AFMT_S24  AFMT_S24_LE
# define AFMT_S32  AFMT_S32_LE
#endif

/******************************************************************************/

typedef struct {
     DirectLink           link;

     CorePlayback        *playback;
} CorePlaylistEntry;

struct __FS_CoreSoundShared {
     FusionObjectPool    *buffer_pool;
     FusionObjectPool    *playback_pool;

     FusionSHMPoolShared *shmpool;

     struct {
          DirectLink     *entries;
          FusionSkirmish  lock;
     } playlist;

     struct {
          int             fmt;                 /* hack */
          int             bits;                /* hack */
          long            rate;                /* hack */
          int             channels;            /* hack */
          int             block_size;          /* hack */
          int             samples_per_channel; /* hack */
     } config;

     int                  output_delay;      /* output buffer size in ms */
};

struct __FS_CoreSound {
     int                  refs;

     int                  fusion_id;

     FusionWorld         *world;
     FusionArena         *arena;

     CoreSoundShared     *shared;

     bool                 master;
     int                  fd;
     DirectThread        *sound_thread;

     DirectSignalHandler *signal_handler;
};

/******************************************************************************/

static DirectSignalHandlerResult fs_core_signal_handler( int   num,
                                                         void *addr,
                                                         void *ctx );

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

     ret = fusion_enter( fs_config->session,
#if DIRECT_BUILD_DEBUG
                         -DIRECTFB_CORE_ABI,
#else
                         DIRECTFB_CORE_ABI,
#endif
                         &core->world );
     if (ret) {
          D_FREE( core );
          pthread_mutex_unlock( &core_sound_lock );
          return ret;
     }

     core->fusion_id = fusion_id( core->world );

#if FUSION_BUILD_MULTI
     D_DEBUG( "FusionSound/Core: world %d, fusion id %d\n",
              fusion_world_index( core->world ), core->fusion_id );
#endif

     /* Initialize the references. */
     core->refs = 1;

     direct_signal_handler_add( -1, fs_core_signal_handler, core, &core->signal_handler );

     /* Enter the FusionSound core arena. */
     if (fusion_arena_enter( core->world, "FusionSound/Core",
                             fs_core_arena_initialize, fs_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          direct_signal_handler_remove( core->signal_handler );
          fusion_exit( core->world, false );
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
fs_core_destroy( CoreSound *core, bool emergency )
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

     direct_signal_handler_remove( core->signal_handler );
     
     /* Exit the FusionSound core arena. */
     if (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                            core->master ? NULL : fs_core_arena_leave,
                            core, emergency, NULL ) == DFB_BUSY)
     {
          if (core->master) {
               if (emergency) {
                    fusion_kill( core->world, 0, SIGKILL, 1000 );
               }
               else {
                    fusion_kill( core->world, 0, SIGTERM, 5000 );
                    fusion_kill( core->world, 0, SIGKILL, 2000 );
               }
          }
          
          while (fusion_arena_exit( core->arena, fs_core_arena_shutdown,
                                    core->master ? NULL : fs_core_arena_leave,
                                    core, emergency, NULL ) == DFB_BUSY)
          {
               D_ONCE( "waiting for FusionSound slaves to terminate" );
               usleep( 100000 );
          }
     }

     fusion_exit( core->world, emergency );

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
     return (CoreSoundBuffer*) fusion_object_create( core->shared->buffer_pool, core->world );
}

CorePlayback *
fs_core_create_playback( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->playback_pool != NULL );

     /* Create a new object in the playback pool. */
     return (CorePlayback*) fusion_object_create( core->shared->playback_pool, core->world );
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
     entry = SHCALLOC( shared->shmpool, 1, sizeof(CorePlaylistEntry) );
     if (!entry)
          return DFB_NOSYSTEMMEMORY;

     /* Link playback to playlist entry. */
     if (fs_playback_link( &entry->playback, playback )) {
          SHFREE( shared->shmpool, entry );
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

               SHFREE( shared->shmpool, entry );
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

FusionWorld *
fs_core_world( CoreSound *core )
{
     D_ASSERT( core != NULL );

     return core->world;
}

FusionSHMPoolShared *
fs_core_shmpool( CoreSound *core )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     return core->shared->shmpool;
}

/******************************************************************************/

static DirectSignalHandlerResult
fs_core_signal_handler( int num, void *addr, void *ctx )
{
     CoreSound *core = (CoreSound*) ctx;

     D_ASSERT( core != NULL );
     D_ASSERT( core == core_sound );

     fs_core_destroy( core, true );

     return DFB_OK;
}

/******************************************************************************/

static void *
sound_thread( DirectThread *thread, void *arg )
{
     CoreSound       *core    = arg;
     CoreSoundShared *shared  = core->shared;
     int              samples = shared->config.samples_per_channel * 2;

     __u8             output[shared->config.block_size];
     __fsf            mixing[samples];

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
          memset( mixing, 0, sizeof(mixing) );

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

                    SHFREE( shared->shmpool, entry );
               }
          }

          fusion_skirmish_dismiss( &shared->playlist.lock );

          /* Convert mixing buffer to output format, clipping each sample. */
          switch (shared->config.fmt) {
               case AFMT_U8:
                    if (shared->config.channels == 1) {
                         for (i = 0; i < samples; i += 2) {
                              register __fsf s;
                              s = fsf_add( mixing[i], mixing[i+1] );
                              s = fsf_shr( s, 1 );
                              s = fsf_clip( s );                  
                              output[i>>1] = fsf_to_u8( s );
                         }
                    } else {      
                         for (i = 0; i < samples; i++) {
                              register __fsf s;
                              s = mixing[i];                       
                              s = fsf_clip( s );                  
                              output[i] = fsf_to_u8( s );
                         }
                    }
                    break;
               case AFMT_S16:
                    if (shared->config.channels == 1) {
                         for (i = 0; i < samples; i += 2) {
                              register __fsf s;
                              s = fsf_add( mixing[i], mixing[i+1] );
                              s = fsf_shr( s, 1 );
                              s = fsf_clip( s );                         
                              ((__s16*)output)[i>>1] = fsf_to_s16( s );
                         }
                    } else {
                         for (i = 0; i < samples; i++) {
                              register __fsf s;
                              s = mixing[i];
                              s = fsf_clip( s );                         
                              ((__s16*)output)[i] = fsf_to_s16( s );
                         }
                    }
                    break;
               case AFMT_S24:
                    if (shared->config.channels == 1) {
                         for (i = 0; i < samples; i += 2) {
                              register __fsf s;
                              register int   d;
                              s = fsf_add( mixing[i], mixing[i+1] );
                              s = fsf_shr( s, 1 );
                              s = fsf_clip( s );
                              d = fsf_to_s24( s );
#ifdef WORDS_BIGENDIAN
                              output[(i>>1)*3+0] = d >> 16;
                              output[(i>>1)*3+1] = d >>  8;
                              output[(i>>1)*3+2] = d      ;
#else
                              output[(i>>1)*3+0] = d      ;
                              output[(i>>1)*3+1] = d >>  8;
                              output[(i>>1)*3+2] = d >> 16;
#endif
                         }
                    } else {
                         for (i = 0; i < samples; i++) {
                              register __fsf s;
                              register int   d;
                              s = mixing[i];
                              s = fsf_clip( s );
                              d = fsf_to_s24( s );
#ifdef WORDS_BIGENDIAN
                              output[i*3+0] = d >> 16;
                              output[i*3+1] = d >>  8;
                              output[i*3+2] = d      ;
#else
                              output[i*3+0] = d      ;
                              output[i*3+1] = d >>  8;
                              output[i*3+2] = d >> 16;
#endif
                         }
                    }
                    break;
               case AFMT_S32:
                    if (shared->config.channels == 1) {
                         for (i = 0; i < samples; i += 2) {
                              register __fsf s;
                              s = fsf_add( mixing[i], mixing[i+1] );
                              s = fsf_shr( s, 1 );
                              s = fsf_clip( s );                         
                              ((__s32*)output)[i>>1] = fsf_to_s32( s );
                         }
                    } else {
                         for (i = 0; i < samples; i++) {
                              register __fsf s;
                              s = mixing[i];
                              s = fsf_clip( s );                         
                              ((__s32*)output)[i] = fsf_to_s32( s );
                         }
                    }
                    break;
               default:
                    D_BUG( "unexpected sample format" );
                    break;
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
     int              prof     = APF_NORMAL;
#endif
     CoreSoundShared *shared   = core->shared;
     int              fmt      = shared->config.fmt;
     int              channels = shared->config.channels;
     int              rate     = shared->config.rate;

     /* open sound device in non-blocking mode */
     if (fs_config->device)
          fd = open( fs_config->device, O_WRONLY | O_NONBLOCK );
     else
          fd = direct_try_open( "/dev/dsp", "/dev/sound/dsp", O_WRONLY | O_NONBLOCK, true );
     
     if (fd < 0) {
          D_ERROR( "FusionSound/Core: "
                   "Couldn't open output device!\n" );
          return DFB_INIT;
     }

     /* reset to blocking mode */
     fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) & ~O_NONBLOCK );

     /* set application profile */
#ifdef SNDCTL_DSP_PROFILE
     if (ioctl( fd, SNDCTL_DSP_PROFILE, &prof ) == -1) {
          D_ERROR( "FusionSound/Core: Unable to set application profile!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }
#endif
     
     /* set bits per sample */
     if (ioctl( fd, SNDCTL_DSP_SETFMT, &fmt ) == -1)
          fmt = 0;
     
     /* if ioctl failed, try AFMT_S16 */
     if (shared->config.fmt != fmt) {
          if (shared->config.fmt != AFMT_S16) {
               D_INFO( "FusionSound/Core: "
                       "Setting bits to '%d' failed, trying 16!\n",
                        shared->config.bits );

               shared->config.fmt = fmt = AFMT_S16;
               shared->config.bits = 16;
               ioctl( fd, SNDCTL_DSP_SETFMT, &fmt );
          }
          
          if (shared->config.fmt != fmt) {
               D_ERROR( "FusionSound/Core: "
                        "Unable to set bits to '%d'!\n", shared->config.bits );
               close( fd );
               return DFB_UNSUPPORTED;
          }
     }

     /* set mono/stereo */
     if (ioctl( fd, SNDCTL_DSP_CHANNELS, &channels ) == -1)
          channels = 0;

     switch (channels) {
          case 1:
          case 2:
               shared->config.channels = channels;
               break;               
          default:               
               D_ERROR( "FusionSound/Core: Unable to set '%d' channels!\n",
                         shared->config.channels );
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
     
     D_DEBUG( "FusionSound/Core: "
              "device opened for %ldHz - %d channel(s) - %d bit output.\n", 
              shared->config.rate, shared->config.channels, shared->config.bits );

     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &shared->config.block_size );
     if (shared->config.block_size < 1) {
          D_ERROR( "FusionSound/Core: Unable to query block size!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     D_DEBUG( "FusionSound/Core: got block size %d\n", 
              shared->config.block_size );

     if (shared->config.block_size < 4096)
          shared->config.block_size = 4096;
     else if (shared->config.block_size > 8192)
          shared->config.block_size = 8192;

     D_DEBUG( "FusionSound/Core: using block size %d\n", 
              shared->config.block_size );

     /* calculate number of samples fitting into one block */
     shared->config.samples_per_channel = shared->config.block_size /
                                         (shared->config.channels * shared->config.bits/8);

     /* store file descriptor */
     core->fd = fd;

     /* initialize playback list lock */
     fusion_skirmish_init( &shared->playlist.lock, "FusionSound Playlist", core->world );

     /* create a pool for sound buffer objects */
     shared->buffer_pool = fs_buffer_pool_create( core->world );

     /* create a pool for playback objects */
     shared->playback_pool = fs_playback_pool_create( core->world );

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

          SHFREE( shared->shmpool, entry );
     }

     /* destroy playback object pool */
     fusion_object_pool_destroy( shared->playback_pool, core->world );

     /* destroy buffer object pool */
     fusion_object_pool_destroy( shared->buffer_pool, core->world );

     /* destroy playlist lock */
     fusion_skirmish_destroy( &shared->playlist.lock );

     return DFB_OK;
}

/******************************************************************************/

static int
fs_core_arena_initialize( FusionArena *arena,
                          void        *ctx )
{
     DFBResult            ret;
     CoreSound           *core   = ctx;
     CoreSoundShared     *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG( "FusionSound/Core: Initializing...\n" );

     /* Create the shared memory pool first! */
     ret = fusion_shm_pool_create( core->world, "FusionSound Main Pool", 0x1000000, &pool );
     if (ret)
          return ret;

     /* Allocate shared structure in the new pool. */
     shared = SHCALLOC( pool, 1, sizeof(CoreSoundShared) );
     if (!shared) {
          fusion_shm_pool_destroy( core->world, pool );
          return D_OOSHM();
     }

     core->shared = shared;
     core->master = true;

     shared->shmpool = pool;

     /* FIXME: add live configuration */
     switch (fs_config->sampleformat) {
          case FSSF_U8:
               shared->config.fmt  = AFMT_U8;
               shared->config.bits = 8;
               break;
          case FSSF_S24:
               shared->config.fmt  = AFMT_S24;
               shared->config.bits = 24;
               break;
          case FSSF_S32:
               shared->config.fmt  = AFMT_S32;
               shared->config.bits = 32;
               break;
          default:
               shared->config.fmt  = AFMT_S16;
               shared->config.bits = 16;
               break;
     }
     shared->config.channels = fs_config->channels;
     shared->config.rate     = fs_config->samplerate;

     /* Initialize. */
     ret = fs_core_initialize( core );
     if (ret) {
          SHFREE( pool, shared );
          fusion_shm_pool_destroy( core->world, pool );
          return ret;
     }

     /* Register shared data. */
     fusion_arena_add_shared_field( arena, "Core/Shared", shared );

     return DFB_OK;
}

static int
fs_core_arena_shutdown( FusionArena *arena,
                        void        *ctx,
                        bool         emergency)
{
     DFBResult            ret;
     CoreSound           *core = ctx;
     CoreSoundShared     *shared;
     FusionSHMPoolShared *pool;

     shared = core->shared;

     pool = shared->shmpool;

     D_DEBUG( "FusionSound/Core: Shutting down...\n" );

     if (!core->master) {
          D_DEBUG( "FusionSound/Core: Refusing shutdown in slave.\n" );
          return fs_core_leave( core );
     }

     /* Shutdown. */
     ret = fs_core_shutdown( core );
     if (ret)
          return ret;

     SHFREE( pool, shared );

     fusion_shm_pool_destroy( core->world, pool );

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

