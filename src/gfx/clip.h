/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __GFX__CLIP_H__
#define __GFX__CLIP_H__

#include <directfb.h>

typedef enum {
     DFEF_NONE      = 0x00000000,

     DFEF_LEFT      = 0x00000001,
     DFEF_RIGHT     = 0x00000002,
     DFEF_TOP       = 0x00000004,
     DFEF_BOTTOM    = 0x00000008,

     DFEF_ALL       = 0x0000000F
} DFBEdgeFlags;

/*
 * Clips the line to the clipping region.
 * Returns DFB_TRUE if at least one pixel of the line resides in the region.
 */
DFBBoolean   dfb_clip_line( const DFBRegion *clip, DFBRegion *line );

/*
 * Clips the rectangle to the clipping region.
 * Returns true if there was an intersection with the clipping region.
 */
DFBBoolean   dfb_clip_rectangle( const DFBRegion *clip, DFBRectangle *rect );

/*
 * Clips the rectangle to the clipping region.
 * Returns a flag for each edge that wasn't cut off.
 */
DFBEdgeFlags dfb_clip_edges( const DFBRegion *clip, DFBRectangle *rect );

/*
 * Simple check if triangle lies outside the clipping region.
 * Returns true if the triangle may be visible within the region.
 */
DFBBoolean   dfb_clip_triangle_precheck( const DFBRegion   *clip,
                                         const DFBTriangle *tri );

/*
 * Simple check if requested blitting lies outside of the clipping region.
 * Returns true if blitting may need to be performed.
 */
static inline DFBBoolean
dfb_clip_blit_precheck( const DFBRegion *clip,
                        int w, int h, int dx, int dy )
{
     if (w < 1 || h < 1 ||
         (clip->x1 >= dx + w) ||
         (clip->x2 < dx) ||
         (clip->y1 >= dy + h) ||
         (clip->y2 < dy))
          return DFB_FALSE;

     return DFB_TRUE;
}

/*
 * Clips the blitting request to the clipping region.
 * This includes adjustment of source AND destination coordinates.
 */
void dfb_clip_blit( const DFBRegion *clip,
                    DFBRectangle *srect, int *dx, int *dy );

/*
 * Clips the stretch blit request to the clipping region.
 * This includes adjustment of source AND destination coordinates
 * based on the scaling factor.
 */
void dfb_clip_stretchblit( const DFBRegion *clip,
                           DFBRectangle    *srect,
                           DFBRectangle    *drect );

#endif

