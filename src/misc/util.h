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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <errno.h>

#include <directfb.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define SGN(x)   ((x<0) ?  -1  :  ((x>0) ? 1 : 0))

void trim( char **s );

/*
 * translates errno to DirectFB DFBResult
 */
DFBResult errno2dfb( int erno );

int region_intersect( DFBRegion *region,
                      int x1, int y1, int x2, int y2 );

int unsafe_region_intersect( DFBRegion *region,
                             int x1, int y1, int x2, int y2 );

int unsafe_region_rectangle_intersect( DFBRegion *region,
                                       DFBRectangle *rect );

int rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                          DFBRegion    *region );

int rectangle_intersect( DFBRectangle *rectangle,
                         DFBRectangle *clip );

#endif
