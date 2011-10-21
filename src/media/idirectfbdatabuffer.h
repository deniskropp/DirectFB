/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
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

#ifndef __IDIRECTFBDATABUFFER_H__
#define __IDIRECTFBDATABUFFER_H__

#include <core/core.h>

#include <fusion/call.h>

/*
 * private data struct of IDirectFBDataBuffer
 */
typedef struct {
     int          ref;        /* reference counter */
     char        *filename;   /* Only set if databuffer is created from file. */

     CoreDFB     *core;
     IDirectFB   *idirectfb;

     bool         is_memory;

     FusionCall   call;       /* for remote access */
} IDirectFBDataBuffer_data;

/*
 * private data struct of IDirectFBDataBuffer_File
 */
typedef struct {
     IDirectFBDataBuffer_data base;

     DirectStream    *stream;
     DirectMutex      mutex;
} IDirectFBDataBuffer_File_data;

/*
 * private data struct of IDirectFBDataBuffer_Memory
 */
typedef struct {
     IDirectFBDataBuffer_data  base;

     const void               *buffer;
     unsigned int              length;

     unsigned int              pos;
} IDirectFBDataBuffer_Memory_data;

/*
 * base constructor
 *
 * If the databuffer is created for a file, the filename can be provided
 * for fallbacks.
 */
DFBResult IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                                         const char          *filename,
                                         CoreDFB             *core,
                                         IDirectFB           *idirectfb );

/*
 * base destructor
 */
void IDirectFBDataBuffer_Destruct( IDirectFBDataBuffer *thiz );

/*
 * generic streamed data buffer
 */
DFBResult IDirectFBDataBuffer_Streamed_Construct( IDirectFBDataBuffer *thiz,
                                                  CoreDFB             *core,
                                                  IDirectFB           *idirectfb );

/*
 * file based static data buffer
 */
DFBResult IDirectFBDataBuffer_File_Construct( IDirectFBDataBuffer *thiz,
                                              const char          *filename,
                                              CoreDFB             *core,
                                              IDirectFB           *idirectfb );

/*
 * memory based static data buffer
 */
DFBResult IDirectFBDataBuffer_Memory_Construct( IDirectFBDataBuffer *thiz,
                                                const void          *data,
                                                unsigned int         length,
                                                CoreDFB             *core,
                                                IDirectFB           *idirectfb );

#endif
