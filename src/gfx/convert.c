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

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>

#include "misc/memcpy.h"

#include "convert.h"


/* Totally unused yet, not even declared */

#if 0

void dfb_convert_buffer( void                 *src,
                         void                 *dst,
                         int                   width,
                         int                   height,
                         int                   src_pitch,
                         int                   dst_pitch,
                         DFBSurfacePixelFormat src_format,
                         DFBSurfacePixelFormat dst_format )
{
     switch (src_format) {
          case DSPF_RGB15:
               switch (dst_format) {
                    case DSPF_RGB15:
                         while (height--) {
                              dfb_memcpy( dst, src, width*2 );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB16:
                         while (height--) {
                              span_rgb15_to_rgb16( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB32:
                         while (height--) {
                              span_rgb15_to_rgb32( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_ARGB:
                         while (height--) {
                              span_rgb15_to_argb( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    default:
                         ERRORMSG( "DirectFB/gfx: unimplemented conversion!\n");
                         break;
               }
               break;
          case DSPF_RGB16:
               switch (dst_format) {
                    case DSPF_RGB15:
                         while (height--) {
                              span_rgb16_to_rgb15( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB16:
                         while (height--) {
                              dfb_memcpy( dst, src, width*2 );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB32:
                         while (height--) {
                              span_rgb16_to_rgb32( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_ARGB:
                         while (height--) {
                              span_rgb16_to_argb( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    default:
                         ERRORMSG( "DirectFB/gfx: unimplemented conversion!\n");
                         break;
               }
               break;
          case DSPF_RGB32:
               switch (dst_format) {
                    case DSPF_RGB15:
                         while (height--) {
                              span_rgb32_to_rgb15( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB16:
                         while (height--) {
                              span_rgb32_to_rgb16( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB32:
                         while (height--) {
                              dfb_memcpy( dst, src, width*4 );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_ARGB:
                         while (height--) {
                              span_rgb32_to_argb( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    default:
                         ERRORMSG( "DirectFB/gfx: unimplemented conversion!\n");
                         break;
               }
               break;
          case DSPF_ARGB:
               switch (dst_format) {
                    case DSPF_RGB15:
                         while (height--) {
                              span_rgb32_to_rgb15( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB16:
                         while (height--) {
                              span_rgb32_to_rgb16( src, dst, width );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    case DSPF_RGB32:
                    case DSPF_ARGB:
                         while (height--) {
                              dfb_memcpy( dst, src, width*4 );
                              ((__u8*)src) += src_pitch;
                              ((__u8*)dst) += dst_pitch;
                         }
                         break;
                    default:
                         ERRORMSG( "DirectFB/gfx: unimplemented conversion!\n");
                         break;
               }
               break;
          default:
               ERRORMSG( "DirectFB/gfx: unimplemented conversion!\n");
               break;
     }
}

#endif

