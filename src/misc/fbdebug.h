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

#ifndef __FBDEBUG_H__
#define __FBDEBUG_H__

#include <core/fusion/lock.h>

#include <core/coretypes.h>


typedef struct _FBDebugArea FBDebugArea;


#ifdef DFB_DEBUG

DFBResult fbdebug_init();
void      fbdebug_exit();

void      fbdebug_get_size ( unsigned int  *width,
                             unsigned int  *height );

DFBResult fbdebug_get_area ( unsigned int   x,
                             unsigned int   y,
                             unsigned int   width,
                             unsigned int   height,
                             FBDebugArea  **area );

void      fbdebug_free_area( FBDebugArea   *area );

DFBResult fbdebug_fill     ( FBDebugArea   *area,
                             unsigned int   x,
                             unsigned int   y,
                             unsigned int   width,
                             unsigned int   height,
                             __u8           r,
                             __u8           g,
                             __u8           b );

#endif

#endif
