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
#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/sound_buffer.h>

#include "ifusionsoundstream.h"

/*
 * private data struct of IFusionSoundStream
 */
typedef struct {
     int                    ref;             /* reference counter */

     CoreSoundBuffer       *buffer;
     int                    size;

     Reaction               reaction;

     pthread_mutex_t        lock;
     pthread_cond_t         wait;
     bool                   playing;
     int                    pos_write;
     int                    pos_read;
} IFusionSoundStream_data;

/******/

static ReactionResult IFusionSoundStream_React( const void *msg_data,
                                                void       *ctx );

/******/

static void
IFusionSoundStream_Destruct( IFusionSoundStream *thiz )
{
     IFusionSoundStream_data *data = (IFusionSoundStream_data*)thiz->priv;

     if (data->buffer) {
          CoreSoundBuffer *buffer = data->buffer;

          data->buffer = NULL;

          fs_buffer_detach( buffer, &data->reaction );
          
          fs_buffer_stop_all( buffer );
          
          fs_buffer_unref( buffer );
     }

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
     
     /* Set new break position. */
     return fs_buffer_set_break( data->buffer, data->pos_write );
}

static DFBResult
IFusionSoundStream_Write( IFusionSoundStream *thiz,
                          const void         *sample_data,
                          int                 length )
{
     CoreSoundBuffer *buffer;

     INTERFACE_GET_DATA(IFusionSoundStream)

     buffer = data->buffer;
     if (!buffer)
          return DFB_DESTROYED;

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
               data->playing = true;

               fs_buffer_playback( buffer, data->pos_read, 0x8000, true );
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
     CoreSoundBuffer *buffer;

     INTERFACE_GET_DATA(IFusionSoundStream)

     buffer = data->buffer;
     if (!buffer)
          return DFB_DESTROYED;

     if (length < 1 || length >= data->size)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );
     
     while (true) {
          int num;

          /* Calculate number of free samples in the buffer. */
          if (data->pos_write < data->pos_read)
               num = data->pos_read - data->pos_write - 1;
          else
               num = data->size - data->pos_write + data->pos_read - 1;
          
          if (num >= length)
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

     if (!data->buffer)
          return DFB_DESTROYED;

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
                              CoreSoundBuffer    *buffer,
                              int                 size )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundStream)

     data->ref    = 1;
     data->buffer = buffer;
     data->size   = size;

     pthread_mutex_init( &data->lock, NULL );
     pthread_cond_init( &data->wait, NULL );

     if (fs_buffer_attach( buffer, IFusionSoundStream_React,
                           data, &data->reaction ))
     {
          DFB_DEALLOCATE_INTERFACE( thiz );

          return DFB_FUSION;
     }

     thiz->AddRef    = IFusionSoundStream_AddRef;
     thiz->Release   = IFusionSoundStream_Release;

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
     const CoreSoundBufferNotification *notification = msg_data;
     IFusionSoundStream_data           *data         = ctx;

     if (notification->flags & CABNF_PLAYBACK_ADVANCED)
          DEBUGMSG( "%s: playback advanced, next read at position %d\n",
                    __FUNCTION__, notification->pos );

     if (notification->flags & CABNF_PLAYBACK_ENDED)
          DEBUGMSG( "%s: playback ended at position %d!\n",
                    __FUNCTION__, notification->pos );
     
     pthread_mutex_lock( &data->lock );
     
     data->pos_read = notification->pos;

     if (notification->flags & CABNF_PLAYBACK_ENDED)
          data->playing = false;

     pthread_cond_broadcast( &data->wait );
     
     pthread_mutex_unlock( &data->lock );
     
     return RS_OK;
}

