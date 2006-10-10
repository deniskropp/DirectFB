/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

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

#include <pthread.h>

#include <fusionsound.h>

#include <core/playback.h>
#include <core/sound_buffer.h>

#include <direct/interface.h>
#include <direct/util.h>

#include "ifusionsoundplayback.h"

#include "ifusionsoundbuffer.h"

/*
 * private data struct of IFusionSoundBuffer
 */
typedef struct {
     int                    ref;             /* reference counter */

     CoreSound             *core;
     CoreSoundBuffer       *buffer;

     int                    size;
     int                    channels;
     FSSampleFormat         format;
     int                    rate;

     bool                   locked;

     int                    pos;

     CorePlayback          *looping;
     pthread_mutex_t        lock;
} IFusionSoundBuffer_data;


static void
IFusionSoundBuffer_Destruct( IFusionSoundBuffer *thiz )
{
     IFusionSoundBuffer_data *data = (IFusionSoundBuffer_data*)thiz->priv;

     D_ASSERT( data->buffer != NULL );

     if (data->locked)
          fs_buffer_unlock( data->buffer );

     /* Stop and throw away looping playback. */
     if (data->looping) {
          fs_playback_stop( data->looping, false );
          fs_playback_unref( data->looping );
     }

     fs_buffer_unref( data->buffer );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundBuffer_AddRef( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Release( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (--data->ref == 0)
          IFusionSoundBuffer_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IFusionSoundBuffer_GetDescription( IFusionSoundBuffer  *thiz,
                                   FSBufferDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!desc)
          return DFB_INVARG;

     desc->flags = FSBDF_CHANNELS | FSBDF_LENGTH |
                   FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE;

     desc->channels     = data->channels;
     desc->length       = data->size;
     desc->sampleformat = data->format;
     desc->samplerate   = data->rate;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_SetPosition( IFusionSoundBuffer *thiz,
                                int                 position )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)
     
     if (position < 0 || position >= data->size)
          return DFB_INVARG;
          
     data->pos = position;
    
     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Lock( IFusionSoundBuffer  *thiz,
                         void               **ret_data,
                         int                 *ret_frames,
                         int                 *ret_bytes )
{
     DFBResult  ret;
     void      *lock_data;
     int        lock_bytes;

     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!ret_data)
          return DFB_INVARG;

     if (data->locked)
          return DFB_LOCKED;

     ret = fs_buffer_lock( data->buffer, data->pos, 0, &lock_data, &lock_bytes );
     if (ret)
          return ret;

     data->locked = true;

     *ret_data = lock_data;
     
     if (ret_frames)
          *ret_frames = lock_bytes / 
                        (data->channels * FS_BYTES_PER_SAMPLE(data->format));
     
     if (ret_bytes)
          *ret_bytes = lock_bytes;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Unlock( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->locked)
          return DFB_OK;

     fs_buffer_unlock( data->buffer );

     data->locked = false;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Play( IFusionSoundBuffer *thiz,
                         FSBufferPlayFlags   flags )
{
     DFBResult     ret;
     CorePlayback *playback;

     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (flags & ~FSPLAY_ALL)
          return DFB_INVARG;

     /* Choose looping playback mode. */
     if (flags & FSPLAY_LOOPING) {
          pthread_mutex_lock( &data->lock );

          /* Return error if already running a looping playing. */
          if (data->looping) {
               pthread_mutex_unlock( &data->lock );
               return DFB_BUSY;
          }

          /* Create a playback object. */
          ret = fs_playback_create( data->core, data->buffer, false, &playback );
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
               
          /* Set playback direction. */
          if (flags & FSPLAY_REWIND)
               fs_playback_set_pitch( playback, -0x400 );
          else
               fs_playback_set_pitch( playback, +0x400 );
               
          /* Set the start of the playback. */
          fs_playback_set_position( playback, data->pos );

          /* Set looping playback. */
          fs_playback_set_stop( playback, -1 );

          /* Start the playback. */
          ret = fs_playback_start( playback, false );
          if (ret) {
               fs_playback_unref( playback );
               pthread_mutex_unlock( &data->lock );
               return ret;
          }

          /* Remember looping playback. */
          data->looping = playback;

          pthread_mutex_unlock( &data->lock );
     }
     else {
          /* Create a playback object. */
          ret = fs_playback_create( data->core, data->buffer, false, &playback );
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
               
          /* Set playback direction. */
          if (flags & FSPLAY_REWIND)
               fs_playback_set_pitch( playback, -0x400 );
          else
               fs_playback_set_pitch( playback, +0x400 );
               
          /* Set the start of the playback. */
          fs_playback_set_position( playback, data->pos );
          
          /* Set the end of the playback. */
          if (flags & FSPLAY_CYCLE)
               fs_playback_set_stop( playback, data->pos );
          else
               fs_playback_set_stop( playback, 0 );

          /* Start the playback. */
          ret = fs_playback_start( playback, false );

          /*
           * Already throw away playback object. It has a global reference while
           * it's being played and gets destroyed after playback has finished.
           */
          fs_playback_unref( playback );
     }

     return ret;
}

static DFBResult
IFusionSoundBuffer_Stop( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     pthread_mutex_lock( &data->lock );

     /* Stop and throw away looping playback. */
     if (data->looping) {
          fs_playback_stop( data->looping, false );
          fs_playback_unref( data->looping );

          data->looping = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_CreatePlayback( IFusionSoundBuffer    *thiz,
                                   IFusionSoundPlayback **ret_interface )
{
     DFBResult             ret;
     CorePlayback         *playback;
     IFusionSoundPlayback *interface;

     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!ret_interface)
          return DFB_INVARG;

     ret = fs_playback_create( data->core, data->buffer, true, &playback );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IFusionSoundPlayback );

     ret = IFusionSoundPlayback_Construct( interface, playback, data->size );
     if (ret)
          *ret_interface = NULL;
     else
          *ret_interface = interface;

     fs_playback_unref( playback );

     return ret;
}


/******/

DFBResult
IFusionSoundBuffer_Construct( IFusionSoundBuffer *thiz,
                              CoreSound          *core,
                              CoreSoundBuffer    *buffer,
                              int                 size,
                              int                 channels,
                              FSSampleFormat      format,
                              int                 rate )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundBuffer)

     /* Increase reference counter of the buffer. */
     if (fs_buffer_ref( buffer )) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

     /* Initialize private data. */
     data->ref      = 1;
     data->core     = core;
     data->buffer   = buffer;
     data->size     = size;
     data->channels = channels;
     data->format   = format;
     data->rate     = rate;

     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* Initialize method table. */
     thiz->AddRef         = IFusionSoundBuffer_AddRef;
     thiz->Release        = IFusionSoundBuffer_Release;

     thiz->GetDescription = IFusionSoundBuffer_GetDescription;
     
     thiz->SetPosition    = IFusionSoundBuffer_SetPosition;

     thiz->Lock           = IFusionSoundBuffer_Lock;
     thiz->Unlock         = IFusionSoundBuffer_Unlock;

     thiz->Play           = IFusionSoundBuffer_Play;
     thiz->Stop           = IFusionSoundBuffer_Stop;

     thiz->CreatePlayback = IFusionSoundBuffer_CreatePlayback;

     return DFB_OK;
}

