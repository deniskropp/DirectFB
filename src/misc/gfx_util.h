/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

   Scaling routines ported from gdk_pixbuf by Sven Neumann
   <sven@convergence.de>.

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

#ifndef __GFX_UTIL_H__
#define __GFX_UTIL_H__

#include <directfb.h>

#include <asm/types.h>

void copy_buffer_32( void *dst, __u32 *src, int w, int h, int dskip,
                     DFBSurfacePixelFormat dst_format );

void scale_linear_32( void *dst, __u32 *src, int sw, int sh, int dw, int dh,
                      int dskip, DFBSurfacePixelFormat dst_format );

int clip_line( DFBRegion *clip, DFBRegion *line );

unsigned int clip_rectangle( DFBRegion *clip, DFBRectangle *rect );

int clip_triangle_precheck( DFBRegion *clip, DFBTriangle *tri );

int clip_blit_precheck( DFBRegion *clip, int w, int h, int dx, int dy );

void clip_blit( DFBRegion *clip, DFBRectangle *srect, int *dx, int *dy );

void clip_stretchblit( DFBRegion    *clip,
                       DFBRectangle *srect,
                       DFBRectangle *drect );

#endif
