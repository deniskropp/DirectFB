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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <math.h>

#include <pthread.h>


#include <fusionsound.h>
#include <directfb_internals.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <misc/memcpy.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/playback.h>
#include <core/sound_buffer.h>

#include "ifusionsoundstream.h"

/******/

static ReactionResult IFusionSoundStream_React( const void *msg_data,
                                                void       *ctx );

/******/

static void
IFusionSoundStream_Destruct( IFusionSoundStream *thiz )
{
     IFusionSoundStream_data *data = (IFusionSoundStream_data*)thiz->priv;

     DFB_ASSERT( data->buffer != NULL );
     DFB_ASSERT( data->playback != NULL );

     fs_playback_detach( data->playback, &data->reaction );

     fs_playback_stop( data->playback );

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
                   FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
     desc->buffersize   = data->size;
     desc->channels     = data->channels;
     desc->sampleformat = data->format;
     desc->samplerate   = data->rate;

     return DFB_OK;
}

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

     DEBUGMSG( "%s (%p, %d)\n", __FUNCTION__, data->buffer, length );

     while (length) {
          int num = MIN( length, data->size - data->pos_write );

          /* Write data. */
          ret = fs_buffer_lock( data->buffer, data->pos_write,
                                num, &lock_data, &lock_bytes );
          if (ret)
               return ret;

          dfb_memcpy( lock_data, sample_data + offset, lock_bytes );

          fs_buffer_unlock( data->buffer );

          /* Update input parameters. */
          length -= num;
          offset += lock_bytes;

          /* Update write position. */
          data->pos_write += num;
          if (data->pos_write == data->size)
               data->pos_write = 0;
     }

     if (ret_bytes)
          *ret_bytes = offset;

     /* Set new stop position. */
     return fs_playback_set_stop( data->playback, data->pos_write );
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

          DEBUGMSG( "%s: remaining %d, read pos %d, write pos %d\n",
                    __FUNCTION__, length, data->pos_read, data->pos_write );

          /* Wait for at least one free sample. */
          while (true) {
               int max_pos = data->pos_read - 1;

               if (max_pos == -1)
                    max_pos += data->size;

               if (data->pos_write != max_pos)
                    break;

               pthread_cond_wait( &data->wait, &data->lock );
          }

          /* Calculate number of free samples in the buffer. */
          if (data->pos_write < data->pos_read)
               num = data->pos_read - data->pos_write - 1;
          else
               num = data->size - data->pos_write + data->pos_read - 1;

          if (num > length)
               num = length;

          /* Fill free space with automatic wrap around. */
          ret = IFusionSoundStream_FillBuffer( data, sample_data, num, &bytes);
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }

          /* (Re)start if playback had stopped (buffer underrun). */
          if (!data->playing) {
               int written;

               if (data->pos_write < data->pos_read)
                    written = data->size - data->pos_read + data->pos_write;
               else
                    written = data->pos_write - data->pos_read;

               if (!data->prebuffer || written >= data->prebuffer) {
                    data->playing = true;

                    fs_playback_start( data->playback, data->pos_read );
               }
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

     if (length < 0 || length >= data->size)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     while (true) {
          if (length) {
               int num;

               /* Calculate number of free samples in the buffer. */
               if (data->pos_write < data->pos_read)
                    num = data->pos_read - data->pos_write - 1;
               else
                    num = data->size - data->pos_write + data->pos_read - 1;

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
                              int                *write_position )
{
     INTERFACE_GET_DATA(IFusionSoundStream)

     pthread_mutex_lock( &data->lock );

     if (filled) {
          if (data->pos_write >= data->pos_read)
               *filled = data->pos_write - data->pos_read;
          else
               *filled = data->size - data->pos_read + data->pos_write;
     }

     if (total)
          *total = data->size;

     if (read_position)
          *read_position = data->pos_read;

     if (write_position)
          *write_position = data->pos_write;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
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
          DFB_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

     /* Create a playback object for the buffer. */
     ret = fs_playback_create( core, buffer, true, &playback );
     if (ret) {
          fs_buffer_unref( buffer );

          DFB_DEALLOCATE_INTERFACE( thiz );

          return ret;
     }

     /* Attach our listener to the playback object. */
     if (fs_playback_attach( playback, IFusionSoundStream_React,
                             data, &data->reaction ))
     {
          fs_playback_unref( playback );
          fs_buffer_unref( buffer );

          DFB_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

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

     pthread_mutex_init( &data->lock, NULL );
     pthread_cond_init( &data->wait, NULL );

     /* Initialize method table. */
     thiz->AddRef    = IFusionSoundStream_AddRef;
     thiz->Release   = IFusionSoundStream_Release;

     thiz->GetDescription = IFusionSoundStream_GetDescription;

     thiz->Write     = IFusionSoundStream_Write;
     thiz->Wait      = IFusionSoundStream_Wait;
     thiz->GetStatus = IFusionSoundStream_GetStatus;

     return DFB_OK;
}

/******/

static ReactionResult
IFusionSoundStream_React( const void *msg_data,
                          void       *ctx )
{
     const CorePlaybackNotification *notification = msg_data;
     IFusionSoundStream_data        *data         = ctx;

     if (notification->flags & CPNF_ADVANCED)
          DEBUGMSG( "%s: playback advanced, next read at position %d\n",
                    __FUNCTION__, notification->pos );

     if (notification->flags & CPNF_ENDED)
          DEBUGMSG( "%s: playback ended at position %d!\n",
                    __FUNCTION__, notification->pos );

     pthread_mutex_lock( &data->lock );

     data->pos_read = notification->pos;

     if (notification->flags & CPNF_ENDED)
          data->playing = false;

     pthread_cond_broadcast( &data->wait );

     pthread_mutex_unlock( &data->lock );

     return RS_OK;
}

