/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

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

#include <pthread.h>

#include <fusionsound.h>

#include <core/playback.h>

#include <direct/interface.h>
#include <direct/util.h>

#include "ifusionsoundplayback.h"

/*
 * private data struct of IFusionSoundPlayback
 */
typedef struct {
     int                    ref;             /* reference counter */

     CorePlayback          *playback;
     bool                   stream;
     int                    length;
     Reaction               reaction;

     float                  volume;
     float                  pan;
     
     int                    pitch;
     int                    dir;

     pthread_mutex_t        lock;
     pthread_cond_t         wait;
     bool                   playing;
     bool                   looping;
     int                    position;
} IFusionSoundPlayback_data;

/******/

static ReactionResult IFusionSoundPlayback_React( const void *msg_data,
                                                  void       *ctx );

static DFBResult
IFusionSoundPlayback_UpdateVolume( IFusionSoundPlayback_data* data );

/******/

static void
IFusionSoundPlayback_Destruct( IFusionSoundPlayback *thiz )
{
     IFusionSoundPlayback_data *data = (IFusionSoundPlayback_data*)thiz->priv;

     D_ASSERT( data->playback != NULL );

     fs_playback_detach( data->playback, &data->reaction );

     if (!data->stream)
          fs_playback_stop( data->playback, false );

     fs_playback_unref( data->playback );

     pthread_cond_destroy( &data->wait );
     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundPlayback_AddRef( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Release( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     if (--data->ref == 0)
          IFusionSoundPlayback_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Start( IFusionSoundPlayback *thiz,
                            int                   start,
                            int                   stop )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p, %d -> %d)\n",
              __FUNCTION__, data->playback, start, stop );

     if (data->stream)
          return DFB_UNSUPPORTED;

     if (start < 0 || start >= data->length)
          return DFB_INVARG;

     if (stop >= data->length)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     fs_playback_set_position( data->playback, start );
     fs_playback_set_stop( data->playback, stop );
     fs_playback_start( data->playback, false );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Stop( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p)\n", __FUNCTION__, data->playback );

     return fs_playback_stop( data->playback, false );
}

static DFBResult
IFusionSoundPlayback_Continue( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p)\n", __FUNCTION__, data->playback );

     return fs_playback_start( data->playback, false );
}

static DFBResult
IFusionSoundPlayback_Wait( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p)\n", __FUNCTION__, data->playback );

     pthread_mutex_lock( &data->lock );

     while (data->playing) {
          if (data->looping) {
               pthread_mutex_unlock( &data->lock );
               return DFB_UNSUPPORTED;
          }

          pthread_cond_wait( &data->wait, &data->lock );
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_GetStatus( IFusionSoundPlayback *thiz,
                                DFBBoolean           *playing,
                                int                  *position )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p)\n", __FUNCTION__, data->playback );

     if (playing)
          *playing = data->playing;

     if (position)
          *position = data->position;

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_SetVolume( IFusionSoundPlayback *thiz,
                                float                 level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p, %.3f)\n", __FUNCTION__, data->playback, level );

     if (level < 0.0f)
          return DFB_INVARG;
          
     if (level > 64.0f)
          return DFB_UNSUPPORTED;

     data->volume = level;

     return IFusionSoundPlayback_UpdateVolume( data );
}

static DFBResult
IFusionSoundPlayback_SetPan( IFusionSoundPlayback *thiz,
                             float                 value )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p, %.3f)\n", __FUNCTION__, data->playback, value );

     if (value < -1.0f || value > 1.0f)
          return DFB_INVARG;

     data->pan = value;

     return IFusionSoundPlayback_UpdateVolume( data );
}

static DFBResult
IFusionSoundPlayback_SetPitch( IFusionSoundPlayback *thiz,
                               float                 value )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)

     D_DEBUG( "%s (%p, %.3f)\n", __FUNCTION__, data->playback, value );

     if (value < 0.0f)
          return DFB_INVARG;
          
     if (value > 64.0f)
          return DFB_UNSUPPORTED;
          
     data->pitch = (value * 1024.0f + 0.5f);

     fs_playback_set_pitch( data->playback, data->pitch*data->dir );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_SetDirection( IFusionSoundPlayback *thiz,
                                   FSPlaybackDirection   direction )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback)
     
     D_DEBUG( "%s (%p, %d)\n", __FUNCTION__, data->playback, direction );
     
     switch (direction) {
          case FSPD_FORWARD:
          case FSPD_BACKWARD:
               data->dir = direction;
               break;
          default:
               return DFB_INVARG;
     }
     
     fs_playback_set_pitch( data->playback, data->pitch*data->dir );
     
     return DFB_OK;
}

/******/

DFBResult
IFusionSoundPlayback_Construct( IFusionSoundPlayback *thiz,
                                CorePlayback         *playback,
                                int                   length )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundPlayback)

     /* Increase reference counter of the playback. */
     if (fs_playback_ref( playback )) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

     /* Attach our listener to the playback. */
     if (fs_playback_attach( playback, IFusionSoundPlayback_React,
                             data, &data->reaction ))
     {
          fs_playback_unref( playback );

          DIRECT_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

     /* Initialize private data. */
     data->ref      = 1;
     data->playback = playback;
     data->stream   = (length < 0);
     data->length   = length;
     data->volume   = 1.0f;
     data->pitch    = 0x400;
     data->dir      = +1;

     /* Initialize lock and condition. */
     direct_util_recursive_pthread_mutex_init( &data->lock );
     pthread_cond_init( &data->wait, NULL );

     /* Initialize method table. */
     thiz->AddRef       = IFusionSoundPlayback_AddRef;
     thiz->Release      = IFusionSoundPlayback_Release;

     thiz->Start        = IFusionSoundPlayback_Start;
     thiz->Stop         = IFusionSoundPlayback_Stop;
     thiz->Continue     = IFusionSoundPlayback_Continue;
     thiz->Wait         = IFusionSoundPlayback_Wait;
 
     thiz->GetStatus    = IFusionSoundPlayback_GetStatus;

     thiz->SetVolume    = IFusionSoundPlayback_SetVolume;
     thiz->SetPan       = IFusionSoundPlayback_SetPan;
     thiz->SetPitch     = IFusionSoundPlayback_SetPitch;
     thiz->SetDirection = IFusionSoundPlayback_SetDirection;

     return DFB_OK;
}

/******/

static ReactionResult
IFusionSoundPlayback_React( const void *msg_data,
                            void       *ctx )
{
     const CorePlaybackNotification *notification = msg_data;
     IFusionSoundPlayback_data      *data         = ctx;

     if (notification->flags & CPNF_START)
          D_DEBUG( "%s: playback started at position %d\n", __FUNCTION__, notification->pos );

     if (notification->flags & CPNF_STOP)
          D_DEBUG( "%s: playback stopped at position %d!\n", __FUNCTION__, notification->pos );

     if (notification->flags & CPNF_ADVANCE)
          D_DEBUG( "%s: playback advanced to position %d\n", __FUNCTION__, notification->pos );

     pthread_mutex_lock( &data->lock );

     data->position = notification->pos;

     if (notification->flags & (CPNF_START | CPNF_ADVANCE)) {
          data->playing = true;
          data->looping = notification->stop < 0 ? true : false;
     }

     if (notification->flags & CPNF_STOP) {
          data->playing = false;
          data->looping = false;
     }

     pthread_cond_broadcast( &data->wait );

     pthread_mutex_unlock( &data->lock );

     return RS_OK;
}

static DFBResult
IFusionSoundPlayback_UpdateVolume( IFusionSoundPlayback_data* data )
{
     float left  = 1.0f;
     float right = 1.0f;

     if (data->pan != 0.0f) {
          if (data->pan < 0.0f)
               right = 1.0f + data->pan;
          else if (data->pan > 0.0f)
               left = 1.0f - data->pan;
     }

     if (data->volume != 1.0f) {
          left *= data->volume;
          if (left > 64.0f)
               left = 64.0f;

          right *= data->volume;
          if (right > 64.0f)
               right = 64.0f;
     }

     return fs_playback_set_volume( data->playback, left, right );
}

