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
#include <malloc.h>
#include <errno.h>

#include <sys/time.h>

#include <pthread.h>

#include <core/fusion/reactor.h>
#include <core/fusion/list.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <misc/memcpy.h>

#include <media/idirectfbdatabuffer.h>

typedef struct {
     FusionLink    link;

     void         *data;   /* actual data hold */
     unsigned int  length; /* length of chunk */

     unsigned int  done;   /* number of bytes already consumed */
} DataChunk;

static DataChunk *
create_chunk( const void *data, int length );

static void
destroy_chunk( DataChunk *chunk );

/*
 * private data struct of IDirectFBDataBuffer_Streamed
 */
typedef struct {
     IDirectFBDataBuffer_data  base;

     FusionLink               *chunks;          /* data chunks */
     FusionLink               *last;            /* last chunk is the
                                                   first to read */

     unsigned int              length;          /* total length of all chunks */

     pthread_mutex_t           chunks_mutex;    /* mutex lock for accessing
                                                   the chunk list */

     pthread_cond_t            wait_condition;  /* condition used for idle
                                                   wait in WaitForEvent() */
} IDirectFBDataBuffer_Streamed_data;

static void
DestroyAllChunks( IDirectFBDataBuffer_Streamed_data *data );

static void
ReadChunkData( IDirectFBDataBuffer_Streamed_data *data,
               void                              *buffer,
               unsigned int                       offset,
               unsigned int                       length,
               bool                               flush );


static void
IDirectFBDataBuffer_Streamed_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_Streamed_data *data =
          (IDirectFBDataBuffer_Streamed_data*) thiz->priv;
     
     pthread_cond_destroy( &data->wait_condition );
     pthread_mutex_destroy( &data->chunks_mutex );
     
     IDirectFBDataBuffer_Destruct( thiz );
}

static DFBResult
IDirectFBDataBuffer_Streamed_Release( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_Streamed_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_Flush( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     pthread_mutex_lock( &data->chunks_mutex );
     
     DestroyAllChunks( data );
     
     pthread_mutex_unlock( &data->chunks_mutex );
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_SeekTo( IDirectFBDataBuffer *thiz,
                                     unsigned int         offset )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetPosition( IDirectFBDataBuffer *thiz,
                                          unsigned int        *offset )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetLength( IDirectFBDataBuffer *thiz,
                                        unsigned int        *length )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     /* Check arguments. */
     if (!length)
          return DFB_INVARG;

     /* Return total length of all chunks. */
     *length = data->length;
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForData( IDirectFBDataBuffer *thiz,
                                          unsigned int         length )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)
     
     pthread_mutex_lock( &data->chunks_mutex );

     while (data->length < length)
          pthread_cond_wait( &data->wait_condition, &data->chunks_mutex );

     pthread_mutex_unlock( &data->chunks_mutex );
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                     unsigned int         length,
                                                     unsigned int         seconds,
                                                     unsigned int         milli_seconds )
{
     struct timeval  now;
     struct timespec timeout;
     DFBResult       ret          = DFB_OK;
     bool            locked       = false;
     long int        nano_seconds = milli_seconds * 1000000;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     if (pthread_mutex_trylock( &data->chunks_mutex ) == 0) {
          if (data->length >= length) {
               pthread_mutex_unlock( &data->chunks_mutex );

               return DFB_OK;
          }

          locked = true;
     }

     gettimeofday( &now, NULL );

     timeout.tv_sec  = now.tv_sec + seconds;
     timeout.tv_nsec = (now.tv_usec * 1000) + nano_seconds;

     timeout.tv_sec  += timeout.tv_nsec / 1000000000;
     timeout.tv_nsec %= 1000000000;

     if (!locked)
          pthread_mutex_lock( &data->chunks_mutex );

     while (data->length < length) {
          if (pthread_cond_timedwait( &data->wait_condition,
                                      &data->chunks_mutex,
                                      &timeout ) == ETIMEDOUT)
               ret = DFB_TIMEOUT;
     }

     pthread_mutex_unlock( &data->chunks_mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length,
                                      void                *data_buffer,
                                      unsigned int        *read_out )
{
     unsigned int len;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     if (!data_buffer || !length)
          return DFB_INVARG;

     pthread_mutex_lock( &data->chunks_mutex );
     
     if (!data->chunks) {
          pthread_mutex_unlock( &data->chunks_mutex );
          return DFB_BUFFEREMPTY;
     }

     /* Calculate maximum number of bytes to be read. */
     len = MIN( length, data->length );

     /* Read data from chunks (destructive). */
     ReadChunkData( data, data_buffer, 0, len, true );

     /* Decrease total number of bytes. */
     data->length -= len;

     /* Return number of bytes read. */
     if (read_out)
          *read_out = len;
     
     pthread_mutex_unlock( &data->chunks_mutex );
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PeekData( IDirectFBDataBuffer *thiz,
                                       unsigned int         length,
                                       int                  offset,
                                       void                *data_buffer,
                                       unsigned int        *read_out )
{
     unsigned int len;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     if (!data_buffer || !length)
          return DFB_INVARG;

     pthread_mutex_lock( &data->chunks_mutex );
     
     if (!data->chunks || offset >= data->length) {
          pthread_mutex_unlock( &data->chunks_mutex );
          return DFB_BUFFEREMPTY;
     }

     /* Calculate maximum number of bytes to be read. */
     len = MIN( length, data->length - offset );

     /* Read data from chunks (non-destructive). */
     ReadChunkData( data, data_buffer, offset, len, false );

     /* Return number of bytes read. */
     if (read_out)
          *read_out = len;
     
     pthread_mutex_unlock( &data->chunks_mutex );
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_HasData( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     /* If there's no chunk there's no data. */
     if (!data->chunks)
          return DFB_BUFFEREMPTY;
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PutData( IDirectFBDataBuffer *thiz,
                                      const void          *data_buffer,
                                      unsigned int         length )
{
     DataChunk *chunk;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_Streamed)

     /* Check arguments. */
     if (!data_buffer || !length)
          return DFB_INVARG;

     /* Create a chunk containing a copy of the provided data. */
     chunk = create_chunk( data_buffer, length );
     if (!chunk)
          return DFB_NOSYSTEMMEMORY;

     pthread_mutex_lock( &data->chunks_mutex );
     
     /* Prepend new chunk. */
     fusion_list_prepend( &data->chunks, &chunk->link );

     /* If no chunk has been there before it's the last one. */
     if (!data->last)
          data->last = data->chunks;
     
     /* Increase total length. */
     data->length += length;

     pthread_cond_broadcast( &data->wait_condition );
     
     pthread_mutex_unlock( &data->chunks_mutex );
     
     return DFB_OK;
}

DFBResult
IDirectFBDataBuffer_Streamed_Construct( IDirectFBDataBuffer *thiz )
{
     DFBResult ret;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Streamed)

     ret = IDirectFBDataBuffer_Construct( thiz, NULL );
     if (ret)
          return ret;

     pthread_mutex_init( &data->chunks_mutex, NULL );
     pthread_cond_init( &data->wait_condition, NULL );
     
     thiz->Release                = IDirectFBDataBuffer_Streamed_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Streamed_Flush;
     thiz->SeekTo                 = IDirectFBDataBuffer_Streamed_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Streamed_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Streamed_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Streamed_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Streamed_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Streamed_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Streamed_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Streamed_PutData;

     return DFB_OK;
}

/******************************************************************************/

static void
DestroyAllChunks( IDirectFBDataBuffer_Streamed_data *data )
{
     FusionLink *l;

     DFB_ASSERT( data != NULL );

     /* Fetch first link. */
     l = data->chunks;

     /* Loop through links. */
     while (l) {
          DataChunk *chunk = (DataChunk*) l;

          /* Fetch next link. */
          l = l->next;

          /* Deallocate chunk. */
          destroy_chunk( chunk );
     }

     /* Clear links. */
     data->last = data->chunks = NULL;
}

static void
ReadChunkData( IDirectFBDataBuffer_Streamed_data *data,
               void                              *buffer,
               unsigned int                       offset,
               unsigned int                       length,
               bool                               flush )
{
     FusionLink *l;

     DFB_ASSERT( data != NULL );
     DFB_ASSERT( buffer != NULL );

     /* Fetch last link (the first to read). */
     l = data->last;

     /* Loop through links. */
     while (l && length) {
          unsigned int  len;
          unsigned int  off   = 0;
          DataChunk    *chunk = (DataChunk*) l;
          FusionLink   *prev  = l->prev;

          /* Is there data to be skipped? */
          if (offset) {
               /* Calculate number of bytes to be skipped from this chunk. */
               off = MIN( offset, chunk->length - chunk->done );

               /* Decrease number of bytes to skipped. */
               offset -= off;
          }

          /* Calculate number of bytes to be read from this chunk. */
          len = MIN( length, chunk->length - chunk->done - off );

          /* Can we read from this chunk? */
          if (len) {
               /* Copy as many bytes as possible. */
               dfb_memcpy( buffer, chunk->data + chunk->done + off, len );

               /* Increase write pointer. */
               buffer += len;

               /* Decrease number of bytes to read. */
               length -= len;
          }

          /* Destructive read? */
          if (flush) {
               /* Increase number of consumed bytes. */
               chunk->done += len + off;

               /* Completely consumed? */
               if (chunk->done == chunk->length) {
                    /* If this chunk is the last,
                       the previous is the new last. */
                    if (data->last == l)
                         data->last = prev;

                    /* Remove the chunk from the list. */
                    fusion_list_remove( &data->chunks, l );

                    /* Deallocate chunk. */
                    destroy_chunk( chunk );
               }
          }
          
          /* Proceed with previous link. */
          l = prev;
     }

     DFB_ASSERT( length == 0 );
     DFB_ASSERT( offset == 0 );
}

/******************************************************************************/

static DataChunk *
create_chunk( const void *data, int length )
{
     DataChunk *chunk;

     DFB_ASSERT( data != NULL );
     DFB_ASSERT( length > 0 );

     /* Allocate chunk information. */
     chunk = DFBCALLOC( 1, sizeof(DataChunk) );
     if (!chunk)
          return NULL;

     /* Allocate chunk data. */
     chunk->data = DFBMALLOC( length );
     if (!chunk->data) {
          DFBFREE( chunk );
          return NULL;
     }

     /* Fill chunk data. */
     dfb_memcpy( chunk->data, data, length );

     /* Remember chunk length. */
     chunk->length = length;

     return chunk;
}

static void
destroy_chunk( DataChunk *chunk )
{
     DFB_ASSERT( chunk != NULL );
     DFB_ASSERT( chunk->data != NULL );

     /* Deallocate chunk data. */
     DFBFREE( chunk->data );

     /* Deallocate chunk information. */
     DFBFREE( chunk );
}

