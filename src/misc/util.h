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

#include <config.h>

#include <errno.h>

#include <directfb.h>

#include <core/coredefs.h>


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define SIGN(x)  ((x<0) ?  -1  :  ((x>0) ? 1 : 0))

void trim( char **s );


/*
 * translates errno to DirectFB DFBResult
 */
DFBResult errno2dfb( int erno );

int region_intersect( DFBRegion *region,
                      int x1, int y1, int x2, int y2 );

int region_rectangle_intersect( DFBRegion    *region,
                                DFBRectangle *rect );

int unsafe_region_intersect( DFBRegion *region,
                             int x1, int y1, int x2, int y2 );

int unsafe_region_rectangle_intersect( DFBRegion    *region,
                                       DFBRectangle *rect );

int rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                          DFBRegion    *region );

int rectangle_intersect( DFBRectangle *rectangle,
                         DFBRectangle *clip );

/* returns the result in the first rectangle */
void rectangle_union ( DFBRectangle *rect1,
                       DFBRectangle *rect2 );

/* floor and ceil implementation to get rid of libm */


/*
 IEEE floor for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static inline int IFLOOR(float f)
{
        int ai, bi;
        double af, bf;

        af = (3 << 22) + 0.5 + (double)f;
        bf = (3 << 22) + 0.5 - (double)f;

#if defined(__GNUC__) && defined(__i386__)
        /*
         GCC generates an extra fstp/fld without this.
        */
        asm ("fstps %0" : "=m" (ai) : "t" (af) : "st");
        asm ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
#else
        {
                union { int i; float f; } u;
                u.f = af; ai = u.i;
                u.f = bf; bi = u.i;
        }
#endif

        return (ai - bi) >> 1;
}


/*
 IEEE ceil for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This ceil operation is done by "(iround(f + .5) + iround(f - .5) + 1) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static inline int ICEIL(float f)
{
        int ai, bi;
        double af, bf;

        af = (3 << 22) + 0.5 + (double)f;
        bf = (3 << 22) + 0.5 - (double)f;

#if defined(__GNUC__) && defined(__i386__)
        /*
         GCC generates an extra fstp/fld without this.
        */
        asm ("fstps %0" : "=m" (ai) : "t" (af) : "st");
        asm ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
#else
        {
                union { int i; float f; } u;
                u.f = af; ai = u.i;
                u.f = bf; bi = u.i;
        }
#endif

        return (ai - bi + 1) >> 1;
}



#endif
