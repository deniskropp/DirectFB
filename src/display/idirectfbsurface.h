/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef __DIRECTFBSURFACE_H__
#define __DIRECTFBSURFACE_H__

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                    ref;             /* reference counter */
     DFBSurfaceCapabilities caps;            /* capabilities */
     DFBRectangle           req_rect;        /* requested (sub) area
                                                in surface */
     DFBRectangle           clip_rect;       /* clipped (sub) area in surface */
     int                    locked;          /* is it locked? TODO: use core */
     CoreSurface            *surface;        /* buffer to show */
     IDirectFBFont          *font;           /* font to use */
     CardState              state;           /* render state to use */
} IDirectFBSurface_data;

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFBSurface_Construct( IDirectFBSurface *thiz,
                                      DFBRectangle *req_rect,
                                      DFBRectangle *clip_rect,
                                      CoreSurface *surface,
                                      DFBSurfaceCapabilities caps );

/*
 * destroys surface(s) and frees private data
 */
void IDirectFBSurface_Destruct( IDirectFBSurface *thiz );


ReactionResult IDirectFBSurface_listener( const void *msg_data, void *ctx );

#endif
