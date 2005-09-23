/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de>,
              Ville Syrjälä <syrjala@sci.fi> and
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

#include <stddef.h>
#include <string.h>

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>

DFBResult
IDirectFBVideoProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                         IDirectFBVideoProvider **interface )
{
     DFBResult                            ret;
     DirectInterfaceFuncs                *funcs = NULL;
     IDirectFBDataBuffer_data            *buffer_data;
     IDirectFBVideoProvider              *videoprovider;
     IDirectFBVideoProvider_ProbeContext  ctx;

     /* Get the private information of the data buffer. */
     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Provide a fallback for video providers without data buffer support. */
     ctx.filename = buffer_data->filename;
     ctx.buffer   = buffer;
     
     /* Wait until 512 bytes are available. */
     ret = buffer->WaitForData( buffer, sizeof(ctx.header) );
     if (ret)
          return ret;

     /* Clear context header */
     memset( ctx.header, 0, sizeof(ctx.header) );

     /* Read the first 512 bytes. */
     ret = buffer->PeekData( buffer, sizeof(ctx.header), 0, ctx.header, NULL );
     if (ret)
          return ret;

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBVideoProvider", NULL, DirectProbeInterface, &ctx );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( videoprovider, IDirectFBVideoProvider );

     /* Construct the interface. */
     ret = funcs->Construct( videoprovider, buffer );
     if (ret)
          return ret;

     *interface = videoprovider;

     return DFB_OK;
}

