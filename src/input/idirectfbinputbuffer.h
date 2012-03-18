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

#ifndef __IDIRECTFBEVENTBUFFER_H__
#define __IDIRECTFBEVENTBUFFER_H__

#include <directfb_build.h>

#include <fusion/types.h>

#if !DIRECTFB_BUILD_PURE_VOODOO
#include <core/input.h>
#endif

typedef bool (*EventBufferFilterCallback)( DFBEvent *evt,
                                           void     *ctx );

/*
 * initializes event buffer, adds it to input listeners and initializes mutexes
 */
DFBResult IDirectFBEventBuffer_Construct( IDirectFBEventBuffer      *thiz,
                                          EventBufferFilterCallback  filter,
                                          void                      *filter_ctx );

#if !DIRECTFB_BUILD_PURE_VOODOO
DFBResult IDirectFBEventBuffer_AttachInputDevice( IDirectFBEventBuffer *thiz,
                                                  CoreInputDevice      *device );
DFBResult IDirectFBEventBuffer_DetachInputDevice( IDirectFBEventBuffer *thiz,
                                                  CoreInputDevice      *device );

DFBResult IDirectFBEventBuffer_AttachWindow( IDirectFBEventBuffer *thiz,
                                             CoreWindow           *window );
DFBResult IDirectFBEventBuffer_DetachWindow( IDirectFBEventBuffer *thiz,
                                             CoreWindow           *window );

DFBResult IDirectFBEventBuffer_AttachSurface( IDirectFBEventBuffer *thiz,
                                              CoreSurface          *surface );
DFBResult IDirectFBEventBuffer_DetachSurface( IDirectFBEventBuffer *thiz,
                                              CoreSurface          *surface );
#endif


#endif
