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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <pthread.h>

#include <core/fusion/reactor.h>
#include <core/fusion/list.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/windows.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <media/idirectfbdatabuffer.h>

/*
 * private data struct of IDirectFBDataBuffer_File
 */
typedef struct {
     IDirectFBDataBuffer_data base;

     int   fd;
     off_t pos;
     off_t size;
} IDirectFBDataBuffer_File_data;


static void
IDirectFBDataBuffer_File_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_File_data *data =
          (IDirectFBDataBuffer_File_data*) thiz->priv;

     close( data->fd );

     IDirectFBDataBuffer_Destruct( thiz );
}

static DFBResult
IDirectFBDataBuffer_File_Release( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer)

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
IDirectFBDataBuffer_File_SeekTo( IDirectFBDataBuffer *thiz,
                                 unsigned int         offset )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (lseek( data->fd, offset, SEEK_SET ) < 0) {
          switch (errno) {
               case ESPIPE:
                    return DFB_UNSUPPORTED;

               default:
                    return DFB_FAILURE;
          }
     }

     data->pos = offset;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_GetPosition( IDirectFBDataBuffer *thiz,
                                      unsigned int        *offset )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!offset)
          return DFB_INVARG;

     *offset = data->pos;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_GetLength( IDirectFBDataBuffer *thiz,
                                    unsigned int        *length )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!length)
          return DFB_INVARG;

     *length = data->size;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (data->pos + length > data->size)
          return DFB_BUFFEREMPTY;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                 unsigned int         length,
                                                 unsigned int         seconds,
                                                 unsigned int         milli_seconds )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (data->pos + length > data->size)
          return DFB_BUFFEREMPTY;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_GetData( IDirectFBDataBuffer *thiz,
                                  unsigned int         length,
                                  void                *data_buffer,
                                  unsigned int        *read_out )
{
     ssize_t size;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!data || !length)
          return DFB_INVARG;

     if (data->pos >= data->size)
          return DFB_BUFFEREMPTY;
     
     size = read( data->fd, data_buffer, length );
     if (size < 0)
          return errno2dfb( errno );

     data->pos += size;

     if (read_out)
          *read_out = size;
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_PeekData( IDirectFBDataBuffer *thiz,
                                   unsigned int         length,
                                   int                  offset,
                                   void                *data_buffer,
                                   unsigned int        *read_out )
{
     ssize_t size;

     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (!data || !length)
          return DFB_INVARG;

     if (data->pos + offset >= data->size)
          return DFB_BUFFEREMPTY;
     
     if (data->pos + offset < 0)
          return DFB_INVARG;
     
     if (offset && lseek( data->fd, offset, SEEK_CUR ) < 0) {
          switch (errno) {
               case ESPIPE:
                    return DFB_UNSUPPORTED;

               default:
                    return DFB_FAILURE;
          }
     }
     
     size = read( data->fd, data_buffer, length );
     if (size < 0) {
          int erno = errno;

          lseek( data->fd, - offset, SEEK_CUR );

          return errno2dfb( erno );
     }

     if (lseek( data->fd, - size - offset, SEEK_CUR ) < 0)
          return DFB_FAILURE;
     
     if (read_out)
          *read_out = size;
     
     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_HasData( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer_File)

     if (data->pos >= data->size)
          return DFB_BUFFEREMPTY;

     return DFB_OK;
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
     DFBResult   ret;
     struct stat status;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_File)

     ret = IDirectFBDataBuffer_Construct( thiz, filename );
     if (ret)
          return ret;

     data->fd = open( filename, O_RDONLY );
     if (data->fd < 0) {
          int erno = errno;
          
          PERRORMSG("DirectFB/DataBuffer: opening '%s' failed!\n", filename);
          
          DFB_DEALLOCATE_INTERFACE( thiz );
          
          return errno2dfb( erno );
     }

     if (fstat( data->fd, &status ) < 0) {
          int erno = errno;
          
          PERRORMSG("DirectFB/DataBuffer: fstat failed!\n");
          
          close( data->fd );
          
          DFB_DEALLOCATE_INTERFACE( thiz );
          
          return errno2dfb( erno );
     }

     data->size = status.st_size;
     
     thiz->Release                = IDirectFBDataBuffer_File_Release;
     thiz->Flush                  = IDirectFBDataBuffer_File_Flush;
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

