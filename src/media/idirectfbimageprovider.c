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

#include <stddef.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <misc/mem.h>

#include <media/idirectfbimageprovider.h>
#include <media/idirectfbdatabuffer.h>

DFBResult
IDirectFBImageProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                         IDirectFBImageProvider **interface )
{
     DFBResult                            ret;
     DFBInterfaceFuncs                   *funcs = NULL;
     IDirectFBDataBuffer_data            *buffer_data;
     IDirectFBImageProvider              *imageprovider;
     IDirectFBImageProvider_ProbeContext  ctx;

     /* Get the private information of the data buffer. */
     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Provide a fallback for image providers without data buffer support. */
     ctx.filename = buffer_data->filename;

     /* Wait until 32 bytes are available */
     ret = buffer->WaitForData( buffer, 32 );
     if (ret)
          return ret;
     
     /* Read the first 32 bytes */
     ret = buffer->PeekData( buffer, 32, 0, ctx.header, NULL );
     if (ret)
          return ret;
     
     /* Find a suitable implementation */
     ret = DFBGetInterface( &funcs,
                            "IDirectFBImageProvider", NULL,
                            DFBProbeInterface, &ctx );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( imageprovider, IDirectFBImageProvider );

     /* Construct the interface */
     ret = funcs->Construct( imageprovider, buffer );
     if (ret)
          return ret;
        
     *interface = imageprovider;
     
     return DFB_OK;
}

