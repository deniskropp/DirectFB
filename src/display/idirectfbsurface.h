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

#ifndef __IDIRECTFBSURFACE_H__
#define __IDIRECTFBSURFACE_H__

#include <directfb.h>
#include <core/coretypes.h>
#include <core/state.h>

/*
 * private data struct of IDirectFBSurface
 */
typedef struct {
     int                    ref;             /* reference counter */

     DFBSurfaceCapabilities caps;            /* capabilities */

     struct {
         DFBRectangle       wanted;          /* passed to GetSubSurface */
         DFBRectangle       granted;         /* clipped by parent on creation */
         DFBRectangle       current;         /* currently available area */
     } area;

     int                    clip_set;        /* fixed clip set? (SetClip called
                                                with clip != NULL */
     DFBRegion              clip_wanted;     /* last region passed to SetClip
                                                intersected by wanted area,
                                                only valid if clip_set != 0 */

     int                    locked;          /* which buffer is locked? */
     CoreSurface            *surface;        /* buffer to show */
     IDirectFBFont          *font;           /* font to use */
     CardState              state;           /* render state to use */

     struct {
          __u8              r;               /* red component */
          __u8              g;               /* green component */
          __u8              b;               /* blue component */
          __u32             value;           /* r/g/b in surface's format */
     } src_key;                              /* src key for blitting from
                                                this surface */

     struct {
          __u8              r;               /* red component */
          __u8              g;               /* green component */
          __u8              b;               /* blue component */
          __u32             value;           /* r/g/b in surface's format */
     } dst_key;                              /* dst key for blitting to
                                                this surface */
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


#endif
