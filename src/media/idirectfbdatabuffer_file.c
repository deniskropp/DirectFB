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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <fusion/reactor.h>
#include <direct/list.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/windows.h>

#include <misc/util.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/stream.h>
#include <direct/util.h>

#include <media/idirectfbdatabuffer.h>

/*
 * private data struct of IDirectFBDataBuffer_File
 */
typedef struct {
     IDirectFBDataBuffer_data base;

     DirectStream    *stream;
     pthread_mutex_t  mutex;
} IDirectFBDataBuffer_File_data;


static void
IDirectFBDataBuffer_File_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_File_data *data =
          (IDirectFBDataBuffer_File_data*) thiz->priv;

     direct_stream_destroy( data->stream );

     pthread_mutex_destroy( &data->mutex );

     IDirectFBDataBuffer_Destruct( thiz );
}

static DFBResult
IDirectFBDataBuffer_File_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_File_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_Flush( IDirectFBDataBuffer *thiz )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_File_Finish( IDirectFBDataBuffer *thiz )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_File_SeekTo( IDirectFBDataBuffer *thiz,
                                 unsigned int         offset )
{
     DFBResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!direct_stream_seekable( data->stream ))
          return DFB_UNSUPPORTED;
          
     pthread_mutex_lock( &data->mutex );
     ret = direct_stream_seek( data->stream, offset );
     pthread_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_GetPosition( IDirectFBDataBuffer *thiz,
                                      unsigned int        *offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!offset)
          return DFB_INVARG;

     *offset = direct_stream_offset( data->stream );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_GetLength( IDirectFBDataBuffer *thiz,
                                    unsigned int        *length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!length)
          return DFB_INVARG;

     *length = direct_stream_length( data->stream );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length )
{
     DFBResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     pthread_mutex_lock( &data->mutex );          
     ret = direct_stream_wait( data->stream, length, NULL );
     pthread_mutex_unlock( &data->mutex );
     
     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                 unsigned int         length,
                                                 unsigned int         seconds,
                                                 unsigned int         milli_seconds )
{
     DFBResult      ret;
     struct timeval tv;
     
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     tv.tv_sec  = seconds;
     tv.tv_usec = milli_seconds*1000;

     while (pthread_mutex_trylock( &data->mutex )) {
          struct timespec t, r;
          
          if (errno != EBUSY)
               return errno2result( errno );

          t.tv_sec  = 0;
          t.tv_nsec = 10000;
          nanosleep( &t, &r );
          
          tv.tv_usec -= (t.tv_nsec - r.tv_nsec + 500) / 1000;
          if (tv.tv_usec < 0) {
               if (tv.tv_sec < 1)
                    return DFB_TIMEOUT;
               
               tv.tv_sec--;
               tv.tv_usec += 999999;
          }
     }
         
     pthread_mutex_lock( &data->mutex ); 
     ret = direct_stream_wait( data->stream, length, &tv );
     pthread_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_GetData( IDirectFBDataBuffer *thiz,
                                  unsigned int         length,
                                  void                *data_buffer,
                                  unsigned int        *read_out )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!data_buffer || !length)
          return DFB_INVARG;

     pthread_mutex_lock( &data->mutex );
     ret = direct_stream_read( data->stream, length, data_buffer, read_out );
     pthread_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_PeekData( IDirectFBDataBuffer *thiz,
                                   unsigned int         length,
                                   int                  offset,
                                   void                *data_buffer,
                                   unsigned int        *read_out )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!data_buffer || !length)
          return DFB_INVARG;

     pthread_mutex_lock( &data->mutex );
     ret = direct_stream_peek( data->stream, length,
                               offset, data_buffer, read_out );
     pthread_mutex_unlock( &data->mutex );
     
     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_HasData( IDirectFBDataBuffer *thiz )
{
     DFBResult      ret;
     struct timeval tv = {0,0};
     
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     pthread_mutex_lock( &data->mutex );          
     ret = direct_stream_wait( data->stream, 1, &tv );
     pthread_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_PutData( IDirectFBDataBuffer *thiz,
                                  const void          *data_buffer,
                                  unsigned int         length )
{
     return DFB_UNSUPPORTED;
}

DFBResult
IDirectFBDataBuffer_File_Construct( IDirectFBDataBuffer *thiz,
                                    const char          *filename )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_File)

     ret = IDirectFBDataBuffer_Construct( thiz, filename );
     if (ret)
          return ret;

     ret = direct_stream_create( filename, &data->stream );
     if (ret)
          return ret;

     direct_util_recursive_pthread_mutex_init( &data->mutex );

     thiz->Release                = IDirectFBDataBuffer_File_Release;
     thiz->Flush                  = IDirectFBDataBuffer_File_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_File_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_File_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_File_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_File_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_File_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_File_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_File_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_File_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_File_HasData;
     thiz->PutData                = IDirectFBDataBuffer_File_PutData;

     return DFB_OK;
}

