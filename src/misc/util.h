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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <config.h>

#include <errno.h>

#include <directfb.h>

#include <core/coredefs.h>


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define SIGN(x)  ((x<0) ?  -1  :  ((x>0) ? 1 : 0))
#define ABS(x)   ((x) > 0 ? (x) : -(x))

void dfb_trim( char **s );


/*
 * translates errno to DirectFB DFBResult
 */
DFBResult errno2dfb( int erno );

int dfb_region_intersect( DFBRegion *region,
                          int x1, int y1, int x2, int y2 );

int dfb_region_rectangle_intersect( DFBRegion          *region,
                                    const DFBRectangle *rect );

int dfb_unsafe_region_intersect( DFBRegion *region,
                                 int x1, int y1, int x2, int y2 );

int dfb_unsafe_region_rectangle_intersect( DFBRegion          *region,
                                           const DFBRectangle *rect );

int dfb_rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                              DFBRegion    *region );

int dfb_rectangle_intersect( DFBRectangle       *rectangle,
                             const DFBRectangle *clip );

/* returns the result in the first rectangle */
void dfb_rectangle_union ( DFBRectangle       *rect1,
                           const DFBRectangle *rect2 );


/* Returns the current time after startup of DirectFB in milliseconds */
long long dfb_get_millis();


/* floor and ceil implementation to get rid of libm */

/*
 IEEE floor for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static inline int DFB_IFLOOR(float f)
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
static inline int DFB_ICEIL(float f)
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
