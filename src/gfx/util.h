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

#ifndef __GFX__UTIL_H__
#define __GFX__UTIL_H__

#include <core/surface.h>

void dfb_gfx_copy( CoreSurface *source, CoreSurface *destination, const DFBRectangle *rect );
void dfb_gfx_copy_to( CoreSurface *source, CoreSurface *destination, const DFBRectangle *rect, int x, int y, bool from_back );
void dfb_gfx_copy_stereo( CoreSurface         *source,
                          DFBSurfaceStereoEye  source_eye,
                          CoreSurface         *destination,
                          DFBSurfaceStereoEye  destination_eye,
                          const DFBRectangle  *rect,
                          int                  x,
                          int                  y,
                          bool                 from_back );
void dfb_gfx_clear( CoreSurface *surface, CoreSurfaceBufferRole role );
void dfb_gfx_stretch_to( CoreSurface *source, CoreSurface *destination, const DFBRectangle *srect, const DFBRectangle *drect, bool from_back );
void dfb_gfx_stretch_stereo( CoreSurface         *source,
                             DFBSurfaceStereoEye  source_eye,
                             CoreSurface         *destination,
                             DFBSurfaceStereoEye  destination_eye,
                             const DFBRectangle  *srect,
                             const DFBRectangle  *drect,
                             bool                 from_back );
void dfb_back_to_front_copy( CoreSurface *surface, const DFBRegion *region );
void dfb_back_to_front_copy_rotation( CoreSurface *surface, const DFBRegion *region, int rotation );
void dfb_back_to_front_copy_stereo( CoreSurface         *surface,
                                    DFBSurfaceStereoEye  eyes,
                                    const DFBRegion     *left_region,
                                    const DFBRegion     *right_region,
                                    int                  rotation );
void dfb_clear_depth( CoreSurface *surface, const DFBRegion *region );

void dfb_sort_triangle( DFBTriangle *tri );
void dfb_sort_trapezoid( DFBTrapezoid *trap );


void dfb_gfx_copy_regions( CoreSurface           *source,
                           CoreSurfaceBufferRole  from,
                           CoreSurface           *destination,
                           CoreSurfaceBufferRole  to,
                           const DFBRegion       *regions,
                           unsigned int           num,
                           int                    x,
                           int                    y );

void dfb_gfx_copy_regions_stereo( CoreSurface           *source,
                                  CoreSurfaceBufferRole  from,
                                  DFBSurfaceStereoEye    source_eye,
                                  CoreSurface           *destination,
                                  CoreSurfaceBufferRole  to,
                                  DFBSurfaceStereoEye    destination_eye,
                                  const DFBRegion       *regions,
                                  unsigned int           num,
                                  int                    x,
                                  int                    y );

/*
 * Simplyfy blitting flags
 *
 * Allow any combination of DSBLIT_ROTATE_ and DSBLIT_FLIP_ flags to be reduced
 * to a combination of DSBLIT_ROTATE_90, DSBLIT_FLIP_HORIZONTAL and DSBLIT_FLIP_VERTICAL
 *
 */
static inline void dfb_simplify_blittingflags( DFBSurfaceBlittingFlags *flags )
{
     if (*flags & DSBLIT_ROTATE180)
          *flags = (DFBSurfaceBlittingFlags)(*flags ^ (DSBLIT_ROTATE180 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL));

     if (*flags & DSBLIT_ROTATE270) {
          if (*flags & DSBLIT_ROTATE90)
               *flags = (DFBSurfaceBlittingFlags)(*flags ^ (DSBLIT_ROTATE90 | DSBLIT_ROTATE270));
          else
               *flags = (DFBSurfaceBlittingFlags)(*flags ^ (DSBLIT_ROTATE90 | DSBLIT_ROTATE270 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL));
     }
}

#endif
