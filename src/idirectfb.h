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

#ifndef __IDIRECTFB_H__
#define __IDIRECTFB_H__

#include <directfb.h>

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                 ref;      /* reference counter */
     DFBCooperativeLevel level;    /* current cooperative level */

     struct {
          int            width;    /* IDirectFB stores window width    */
          int            height;   /* and height and the pixel depth   */
          int            bpp;      /* from SetVideoMode() parameters.  */
     } primary;                    /* Used for DFSCL_NORMAL's primary. */
} IDirectFB_data;

/*
 * IDirectFB constructor/destructor
 */
DFBResult IDirectFB_Construct( IDirectFB *thiz );

#endif
