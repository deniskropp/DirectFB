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

#include <core/coredefs.h>

#include <core/fusion/lock.h>

#include <core/core_sound.h>
#include <core/sound_buffer.h>
#include <core/playback.h>

/******************************************************************************/

struct __FS_CorePlayback {
     FusionObject     object;

     FusionSkirmish   lock;

     CoreSound       *core;
     CoreSoundBuffer *buffer;
     bool             notify;

     bool             running;
     int              position;
     int              stop;

     int              left;        /* 8.8 multiplier for left channel */
     int              right;       /* 8.8 multiplier for right channel */
     int              pitch;       /* 8.8 multiplier for sample rate */
};

static void fs_playback_notify( CorePlayback                  *playback,
                                CorePlaybackNotificationFlags  flags,
                                int                            pos );

/******************************************************************************/

static void
playback_destructor( FusionObject *object, bool zombie )
{
     CorePlayback *playback = (CorePlayback*) object;

     DEBUGMSG( "FusionSound/Core: %s (%p, buffer: %p)%s\n", __FUNCTION__,
               playback, playback->buffer, zombie ? " ZOMBIE!" : "" );

     fs_buffer_unlink( &playback->buffer );

     fusion_skirmish_destroy( &playback->lock );

     fusion_object_destroy( object );
}

FusionObjectPool *
fs_playback_pool_create()
{
     return fusion_object_pool_create( "Playbacks", sizeof(CorePlayback),
                                       sizeof(CorePlaybackNotification),
                                       playback_destructor );
}

/******************************************************************************/

DFBResult
fs_playback_create( CoreSound        *core,
                    CoreSoundBuffer  *buffer,
                    bool              notify,
                    CorePlayback    **ret_playback )
{
     CorePlayback *playback;

     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( ret_playback != NULL );

     DEBUGMSG( "FusionSound/Core: %s (buffer %p, notify %d)\n",
               __FUNCTION__, buffer, notify );

     /* Create playback object. */
     playback = fs_core_create_playback( core );
     if (!playback)
          return DFB_FUSION;

     /* Link buffer to playback object. */
     if (fs_buffer_link( &playback->buffer, buffer )) {
          fusion_object_destroy( &playback->object );
          return DFB_FUSION;
     }

     /* Initialize private data. */
     playback->core   = core;
     playback->notify = notify;
     playback->left   = 0x100;
     playback->right  = 0x100;
     playback->pitch  = 0x100;

     fusion_skirmish_init( &playback->lock );

     /* Activate playback object. */
     fusion_object_activate( &playback->object );

     /* Return playback object. */
     *ret_playback = playback;

     return DFB_OK;
}

DFBResult
fs_playback_start( CorePlayback *playback,
                   int           position )
{
     DFBResult ret;

     DFB_ASSERT( playback != NULL );
     DFB_ASSERT( playback->buffer != NULL );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Set new position. */
     playback->position = position;

     /* Start the playback if it's not running already. */
     if (!playback->running) {
          ret = fs_core_add_playback( playback->core, playback );
          if (ret) {
               fusion_skirmish_dismiss( &playback->lock );
               return ret;
          }

          playback->running = true;
     }

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_stop( CorePlayback *playback )
{
     DFB_ASSERT( playback != NULL );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Stop the playback if it's running. */
     if (playback->running) {
          fs_core_remove_playback( playback->core, playback );

          playback->running = false;
     }

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_set_stop( CorePlayback *playback,
                      int           stop )
{
     DFB_ASSERT( playback != NULL );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust stop position. */
     playback->stop = stop;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_set_volume( CorePlayback *playback,
                        int           left,
                        int           right )
{
     DFB_ASSERT( playback != NULL );
     DFB_ASSERT( left >= 0 );
     DFB_ASSERT( left < 0x10000 );
     DFB_ASSERT( right >= 0 );
     DFB_ASSERT( right < 0x10000 );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust volume. */
     playback->left  = left;
     playback->right = right;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_set_pitch( CorePlayback *playback,
                       int           pitch )
{
     DFB_ASSERT( playback != NULL );
     DFB_ASSERT( pitch >= 0 );
     DFB_ASSERT( pitch < 0x10000 );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust pitch. */
     playback->pitch = pitch;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

/******************************************************************************/

DFBResult
fs_playback_mixto( CorePlayback *playback,
                   int          *dest,
                   int           dest_rate,
                   int           max_samples )
{
     DFBResult ret;
     int       pos;

     DFB_ASSERT( playback != NULL );
     DFB_ASSERT( playback->buffer != NULL );
     DFB_ASSERT( dest != NULL );
     DFB_ASSERT( max_samples > 0 );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Mix samples... */
     ret = fs_buffer_mixto( playback->buffer, dest, dest_rate, max_samples,
                            playback->position, playback->stop, playback->left,
                            playback->right, playback->pitch, &pos );
     if (ret)
          playback->running = false;

     /* Write back new position. */
     playback->position = pos;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Notify listeners about the new position and a possible end. */
     fs_playback_notify( playback, ret ? CPNF_ENDED : CPNF_ADVANCED, pos );

     return ret;
}

/******************************************************************************/

static void
fs_playback_notify( CorePlayback                  *playback,
                    CorePlaybackNotificationFlags  flags,
                    int                            pos )
{
     CorePlaybackNotification notification;

     DFB_ASSERT( playback != NULL );
     DFB_ASSERT( flags == CPNF_ADVANCED || flags == CPNF_ENDED );
     DFB_ASSERT( pos >= 0 );

     if (flags & CPNF_ENDED)
          playback->running = false;

     if (!playback->notify)
          return;

     notification.flags    = flags;
     notification.playback = playback;
     notification.pos      = pos;

     fs_playback_dispatch( playback, &notification, NULL );
}

