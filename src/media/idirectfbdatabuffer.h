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

#ifndef __IDIRECTFBDATABUFFER_H__
#define __IDIRECTFBDATABUFFER_H__

/*
 * private data struct of IDirectFBDataBuffer
 */
typedef struct {
     int          ref;        /* reference counter */
     char        *filename;   /* Only set if databuffer is created from file. */
} IDirectFBDataBuffer_data;

/*
 * base constructor
 *
 * If the databuffer is created for a file, the filename can be provided
 * for fallbacks.
 */
DFBResult IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                                         const char          *filename );

/*
 * base destructor
 */
void IDirectFBDataBuffer_Destruct( IDirectFBDataBuffer *thiz );

/*
 * generic streamed data buffer
 */
DFBResult IDirectFBDataBuffer_Streamed_Construct( IDirectFBDataBuffer *thiz );

/*
 * file based static data buffer
 */
DFBResult IDirectFBDataBuffer_File_Construct( IDirectFBDataBuffer *thiz,
                                              const char          *filename );

/*
 * memory based static data buffer
 */
DFBResult IDirectFBDataBuffer_Memory_Construct( IDirectFBDataBuffer *thiz,
                                                void                *data,
                                                unsigned int         length );

#endif
