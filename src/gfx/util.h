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

#ifndef __GFX__UTIL_H__
#define __GFX__UTIL_H__

#include <core/surfaces.h>

void dfb_gfx_copy( CoreSurface *source, CoreSurface *destination, DFBRectangle *rect );
void dfb_back_to_front_copy( CoreSurface *surface, DFBRectangle *rect );

void dfb_sort_triangle( DFBTriangle *tri );

static inline bool dfb_colors_equal( const DFBColor *c1, const DFBColor *c2 )
{
     return ((c1->a == c2->a) && (c1->r == c2->r) &&
             (c1->g == c2->g) && (c1->b == c2->b));
}

#endif
