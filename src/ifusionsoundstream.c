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
#include <interface.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/util.h>

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

#include "ifusionsoundplayback.h"
#include "ifusionsoundstream.h"

/******/

static DFBResult      IFusionSoundStream_FillBuffer( IFusionSoundStream_data *data,
                                                     const void              *sample_data,
                                                     int                      length,
                                                     int                     *ret_bytes );

static ReactionResult IFusionSoundStream_React     ( const void              *msg_data,
                                                     void                    *ctx );

/******/

static void
IFusionSoundStream_Destruct( IFusionSoundStream *thiz )
{
     IFusionSoundStream_data *data = (IFusionSoundStream_data*)thiz->priv;

     D_ASSERT( data->buffer != NULL );
     D_ASSERT( data->playback != NULL );

     fs_playback_detach( data->playback, &data->reaction );

     fs_playback_stop( data->playback, true );

     fs_playback_unref( data->playback );

     fs_buffer_unref( data->buffer );

     pthread_cond_destroy( &data->wait );
     pthread_mutex_destroy( &data->lock );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundStream_AddRef( IFusionSoundStream *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Release( IFusionSoundStream *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     if (--data->ref == 0)
          IFusionSoundStream_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IFusionSoundStream_GetDescription( IFusionSoundStream  *thiz,
                                   FSStreamDescription *desc )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     if (!desc)
          return DFB_INVARG;

     desc->flags = FSSDF_BUFFERSIZE | FSSDF_CHANNELS |
                   FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE | FSSDF_PREBUFFER;
     desc->buffersize   = data->size;
     desc->channels     = data->channels;
     desc->sampleformat = data->format;
     desc->samplerate   = data->rate;
     desc->prebuffer    = data->prebuffer;

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Write( IFusionSoundStream *thiz,
                          const void         *sample_data,
                          int                 length )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     if (!sample_data || length < 1)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     while (length) {
          DFBResult ret;
          int       num;
          int       bytes;

          D_DEBUG( "%s: length %d, read pos %d, write pos %d, filled %d/%d (%splaying)\n",
                   __FUNCTION__, length, data->pos_read, data->pos_write, data->filled,
                   data->size, data->playing ? "" : "not " );

          D_ASSERT( data->filled <= data->size );

          /* Wait for at least one free sample. */
          while (data->filled == data->size)
               pthread_cond_wait( &data->wait, &data->lock );

          /* Calculate number of free samples in the buffer. */
          num = data->size - data->filled;

          /* Do not write more than requested. */
          if (num > length)
               num = length;

          /* Fill free space with automatic wrap around. */
          ret = IFusionSoundStream_FillBuffer( data, sample_data, num, &bytes);
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }

          /* (Re)start if playback had stopped (buffer underrun). */
          if (!data->playing && data->prebuffer >= 0 && data->filled >= data->prebuffer) {
               D_DEBUG( "%s: starting playback now!\n", __FUNCTION__ );

               fs_playback_start( data->playback, true );
          }

          /* Update input parameters. */
          length      -= num;
          sample_data += bytes;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Wait( IFusionSoundStream *thiz,
                         int                 length )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     if (length < 0 || length > data->size)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     while (true) {
          if (length) {
               int num;

               /* Calculate number of free samples in the buffer. */
               num = data->size - data->filled;

               if (num >= length)
                    break;
          }
          else if (!data->playing)
               break;

          pthread_cond_wait( &data->wait, &data->lock );
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_GetStatus( IFusionSoundStream *thiz,
                              int                *filled,
                              int                *total,
                              int                *read_position,
                              int                *write_position,
                              DFBBoolean         *playing )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     pthread_mutex_lock( &data->lock );

     if (filled)
          *filled = data->filled;

     if (total)
          *total = data->size;

     if (read_position)
          *read_position = data->pos_read;

     if (write_position)
          *write_position = data->pos_write;

     if (playing)
          *playing = data->playing;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Flush( IFusionSoundStream *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     /* Stop the playback. */
     fs_playback_stop( data->playback, true );

     pthread_mutex_lock( &data->lock );

     while (data->playing) {
          pthread_cond_wait( &data->wait, &data->lock );
     }

     /* Reset the buffer. */
     data->pos_write = data->pos_read;
     data->filled    = 0;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}


static DFBResult
IFusionSoundStream_GetPresentationDelay( IFusionSoundStream *thiz,
                                         int                *delay )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     if (!delay)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     *delay = fs_core_output_delay( data->core )  +  data->filled * 1000 / data->rate;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_GetPlayback( IFusionSoundStream    *thiz,
                                IFusionSoundPlayback **ret_interface )
{
     DFBResult             ret;
     IFusionSoundPlayback *interface;

     INTERFACE_GET_DATA(IFusionSoundStream)

     if (!ret_interface)
          return DFB_INVARG;

     DFB_ALLOCATE_INTERFACE( interface, IFusionSoundPlayback );

     ret = IFusionSoundPlayback_Construct( interface, data->playback, -1 );
     if (ret)
          *ret_interface = NULL;
     else
          *ret_interface = interface;

     return ret;
}

/******/

DFBResult
IFusionSoundStream_Construct( IFusionSoundStream *thiz,
                              CoreSound          *core,
                              CoreSoundBuffer    *buffer,
                              int                 size,
                              int                 channels,
                              FSSampleFormat      format,
                              int                 rate,
                              int                 prebuffer )
{
     DFBResult     ret;
     CorePlayback *playback;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundStream)

     /* Increase reference counter of the buffer. */
     if (fs_buffer_ref( buffer )) {
          ret = DFB_FUSION;
          goto error_ref;
     }

     /* Create a playback object for the buffer. */
     ret = fs_playback_create( core, buffer, true, &playback );
     if (ret)
          goto error_create;

     /* Attach our listener to the playback object. */
     if (fs_playback_attach( playback, IFusionSoundStream_React, data, &data->reaction )) {
          ret = DFB_FUSION;
          goto error_attach;
     }

     /* Disable the playback. */
     fs_playback_stop( playback, true );

     /* Initialize private data. */
     data->ref       = 1;
     data->core      = core;
     data->buffer    = buffer;
     data->playback  = playback;
     data->size      = size;
     data->channels  = channels;
     data->format    = format;
     data->rate      = rate;
     data->prebuffer = prebuffer;

     /* Initialize lock and condition. */
     fusion_pthread_recursive_mutex_init( &data->lock );
     pthread_cond_init( &data->wait, NULL );

     /* Initialize method table. */
     thiz->AddRef               = IFusionSoundStream_AddRef;
     thiz->Release              = IFusionSoundStream_Release;

     thiz->GetDescription       = IFusionSoundStream_GetDescription;

     thiz->Write                = IFusionSoundStream_Write;
     thiz->Wait                 = IFusionSoundStream_Wait;
     thiz->GetStatus            = IFusionSoundStream_GetStatus;
     thiz->Flush                = IFusionSoundStream_Flush;

     thiz->GetPresentationDelay = IFusionSoundStream_GetPresentationDelay;

     thiz->GetPlayback          = IFusionSoundStream_GetPlayback;

     return DFB_OK;

error_attach:
     fs_playback_unref( playback );

error_create:
     fs_buffer_unref( buffer );

error_ref:
     DFB_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

/******/

static DFBResult
IFusionSoundStream_FillBuffer( IFusionSoundStream_data *data,
                               const void              *sample_data,
                               int                      length,
                               int                     *ret_bytes )
{
     DFBResult        ret;
     void            *lock_data;
     int              lock_bytes;
     int              offset = 0;

     D_DEBUG( "%s: length %d\n", __FUNCTION__, length );

     D_ASSERT( length <= data->size - data->filled );

     while (length) {
          int num = MIN( length, data->size - data->pos_write );

          /* Write data. */
          ret = fs_buffer_lock( data->buffer, data->pos_write, num, &lock_data, &lock_bytes );
          if (ret)
               return ret;

          direct_memcpy( lock_data, sample_data + offset, lock_bytes );

          fs_buffer_unlock( data->buffer );

          /* Update input parameters. */
          length -= num;
          offset += lock_bytes;

          /* Update write position. */
          data->pos_write += num;

          /* Handle wrap around. */
          if (data->pos_write == data->size)
               data->pos_write = 0;

          /* Set new stop position. */
          ret = fs_playback_set_stop( data->playback, data->pos_write );
          if (ret)
               return ret;

          /* (Re)enable playback if buffer has been empty. */
          fs_playback_enable( data->playback );

          /* Update the fill level. */
          data->filled += num;
     }

     if (ret_bytes)
          *ret_bytes = offset;

     return DFB_OK;
}

static ReactionResult
IFusionSoundStream_React( const void *msg_data,
                          void       *ctx )
{
     const CorePlaybackNotification *notification = msg_data;
     CorePlaybackNotificationFlags   flags        = notification->flags;
     IFusionSoundStream_data        *data         = ctx;

     if (flags & CPNF_START) {
          D_DEBUG( "%s: playback started at %d\n", __FUNCTION__, notification->pos );

          /* No locking here to avoid dead possible dead lock with IFusionSoundStream_Write(). */
          data->playing = true;

          return RS_OK;
     }

     pthread_mutex_lock( &data->lock );

     if (notification->flags & CPNF_ADVANCE) {
          D_DEBUG( "%s: playback advanced by %d from %d to %d\n",
                   __FUNCTION__, notification->num, data->pos_read, notification->pos );

          D_ASSERT( data->filled >= notification->num );

          data->filled -= notification->num;
     }

     data->pos_read = notification->pos;

     if (flags & CPNF_STOP) {
          D_DEBUG( "%s: playback stopped at %d!\n", __FUNCTION__, notification->pos );

          data->playing = false;
     }

     pthread_cond_broadcast( &data->wait );

     pthread_mutex_unlock( &data->lock );

     return RS_OK;
}

