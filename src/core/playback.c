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

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/lock.h>

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

     bool             disabled;
     bool             running;
     int              position;
     int              stop;

     __fsf            left;        /* multiplier for left channel */
     __fsf            right;       /* multiplier for right channel */
     int              pitch;       /* multiplier for sample rate in FS_PITCH_ONE units */
};

static void fs_playback_notify( CorePlayback                  *playback,
                                CorePlaybackNotificationFlags  flags,
                                int                            num );

/******************************************************************************/

static void
playback_destructor( FusionObject *object, bool zombie )
{
     CorePlayback *playback = (CorePlayback*) object;

     D_DEBUG( "FusionSound/Core: %s (%p, buffer: %p)%s\n", __FUNCTION__,
              playback, playback->buffer, zombie ? " ZOMBIE!" : "" );

     fs_buffer_unlink( &playback->buffer );

     fusion_skirmish_destroy( &playback->lock );

     fusion_object_destroy( object );
}

FusionObjectPool *
fs_playback_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Playbacks", sizeof(CorePlayback),
                                       sizeof(CorePlaybackNotification),
                                       playback_destructor, world );
}

/******************************************************************************/

DFBResult
fs_playback_create( CoreSound        *core,
                    CoreSoundBuffer  *buffer,
                    bool              notify,
                    CorePlayback    **ret_playback )
{
     CorePlayback *playback;

     D_ASSERT( buffer != NULL );
     D_ASSERT( ret_playback != NULL );

     D_DEBUG( "FusionSound/Core: %s (buffer %p, notify %d)\n",
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
     playback->left   = fsf_from_int( 1 );
     playback->right  = fsf_from_int( 1 );
     playback->pitch  = FS_PITCH_ONE;

     fusion_skirmish_init( &playback->lock, "FusionSound Playback", fs_core_world(core) );

     /* Activate playback object. */
     fusion_object_activate( &playback->object );

     /* Return playback object. */
     *ret_playback = playback;

     return DFB_OK;
}

DFBResult
fs_playback_enable( CorePlayback *playback )
{
     DFBResult ret = DFB_OK;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Enable playback. */
     playback->disabled = false;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return ret;
}

DFBResult
fs_playback_start( CorePlayback *playback, bool enable )
{
     DFBResult ret = DFB_OK;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     /* Lock playlist. */
     if (fs_core_playlist_lock( playback->core ))
          return DFB_FUSION;

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock )) {
          fs_core_playlist_unlock( playback->core );
          return DFB_FUSION;
     }

     /* If the playback is disabled, it won't begin to play. */
     if (enable)
          playback->disabled = false;

     /* Start the playback if it's not running already. */
     if (!playback->running) {
          if (playback->disabled) {
               ret = DFB_TEMPUNAVAIL;
          }
          else {
               ret = fs_core_add_playback( playback->core, playback );

               /* Notify listeners about the start of the playback. */
               if (ret == DFB_OK)
                    fs_playback_notify( playback, CPNF_START, 0 );
          }
     }

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Unlock playlist. */
     fs_core_playlist_unlock( playback->core );

     return ret;
}

DFBResult
fs_playback_stop( CorePlayback *playback, bool disable )
{
     D_ASSERT( playback != NULL );

     /* Lock playlist. */
     if (fs_core_playlist_lock( playback->core ))
          return DFB_FUSION;

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock )) {
          fs_core_playlist_unlock( playback->core );
          return DFB_FUSION;
     }

     /* Stop the playback if it's running. */
     if (playback->running) {
          fs_core_remove_playback( playback->core, playback );

          /* Notify listeners about the end of the playback. */
          fs_playback_notify( playback, CPNF_STOP, 0 );
     }

     /* If this the playback is disabled, it can only be started with enable = true. */
     if (disable)
          playback->disabled = true;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Unlock playlist. */
     fs_core_playlist_unlock( playback->core );

     return DFB_OK;
}

DFBResult
fs_playback_set_stop( CorePlayback *playback,
                      int           stop )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( stop <= playback->buffer->length );

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
fs_playback_set_position( CorePlayback *playback,
                          int           position )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( position >= 0 );
     D_ASSERT( position < playback->buffer->length );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust the playback position. */
     playback->position = position;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_set_volume( CorePlayback *playback,
                        float         left,
                        float         right )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( left >= 0.0f );
     D_ASSERT( left < 256.0f );
     D_ASSERT( right >= 0.0f );
     D_ASSERT( right < 256.0f );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust volume. */
     playback->left  = fsf_from_float( left  );
     playback->right = fsf_from_float( right );

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_set_pitch( CorePlayback *playback,
                       int           pitch )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( pitch >= -(64*FS_PITCH_ONE) );
     D_ASSERT( pitch <= +(64*FS_PITCH_ONE) );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Adjust pitch. */
     playback->pitch = pitch;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

DFBResult
fs_playback_get_status( CorePlayback       *playback,
                        CorePlaybackStatus *ret_status,
                        int                *ret_position )
{
     D_ASSERT( playback != NULL );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Return status. */
     if (ret_status) {
          CorePlaybackStatus status = CPS_NONE;

          if (playback->running) {
               status |= CPS_PLAYING;

               if (playback->stop < 0)
                    status |= CPS_LOOPING;
          }

          *ret_status = status;
     }

     /* Return position. */
     if (ret_position)
          *ret_position = playback->position;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DFB_OK;
}

/******************************************************************************/

DFBResult
fs_playback_mixto( CorePlayback *playback,
                   __fsf        *dest,
                   int           dest_rate,
                   int           max_samples,
                   int          *ret_samples)
{
     DFBResult ret;
     int       pos;
     int       num;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( dest != NULL );
     D_ASSERT( max_samples > 0 );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DFB_FUSION;

     /* Mix samples... */
     ret = fs_buffer_mixto( playback->buffer, dest, dest_rate, max_samples,
                            playback->position, playback->stop, playback->left,
                            playback->right, playback->pitch, &pos, &num, ret_samples );
     if (ret)
          playback->running = false;

     /* Write back new position. */
     playback->position = pos;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Notify listeners about the new position and a possible end. */
     fs_playback_notify( playback, ret ? (CPNF_ADVANCE | CPNF_STOP) : CPNF_ADVANCE, num );

     return ret;
}

/******************************************************************************/

static void
fs_playback_notify( CorePlayback                  *playback,
                    CorePlaybackNotificationFlags  flags,
                    int                            num )
{
     CorePlaybackNotification notification;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( ! (flags & ~(CPNF_START | CPNF_STOP | CPNF_ADVANCE)) );

     if (flags & CPNF_START)
          playback->running = true;

     if (flags & CPNF_STOP)
          playback->running = false;

     if (!playback->notify)
          return;

     notification.flags    = flags;
     notification.playback = playback;
     notification.pos      = playback->position;
     notification.stop     = playback->running ? playback->stop : playback->position;
     notification.num      = num;

     fs_playback_dispatch( playback, &notification, NULL );
}

