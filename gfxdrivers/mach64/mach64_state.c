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

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "regs.h"
#include "mmio.h"
#include "mach64.h"

#include "mach64_state.h"


void mach64_set_destination( Mach64DriverData *mdrv,
                             Mach64DeviceData *mdev,
                             CardState        *state )
{
     volatile __u8 *mmio        = mdrv->mmio_base;
     CoreSurface   *destination = state->destination;
     SurfaceBuffer *buffer      = destination->back_buffer;
     int            pitch       = buffer->video.pitch / DFB_BYTES_PER_PIXEL( destination->format );

     mach64_waitfifo( mdrv, mdev, 2 );

     switch (destination->format) {
          case DSPF_RGB332:
               mdev->dst_bpp = DST_8BPP | HOST_8BPP;
               mach64_out32( mmio, DP_CHAIN_MASK, DP_CHAIN_8BPP_RGB | BYTE_ORDER_LSB_TO_MSB );
               break;
          case DSPF_ARGB1555:
               mdev->dst_bpp = DST_15BPP | HOST_15BPP;
               mach64_out32( mmio, DP_CHAIN_MASK, DP_CHAIN_15BPP | BYTE_ORDER_LSB_TO_MSB );
               break;
          case DSPF_RGB16:
               mdev->dst_bpp = DST_16BPP | HOST_16BPP;
               mach64_out32( mmio, DP_CHAIN_MASK, DP_CHAIN_16BPP | BYTE_ORDER_LSB_TO_MSB );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->dst_bpp = DST_32BPP | HOST_32BPP;
               mach64_out32( mmio, DP_CHAIN_MASK, DP_CHAIN_32BPP | BYTE_ORDER_LSB_TO_MSB );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }
     mdev->dst_key_mask = (1 << DFB_COLOR_BITS_PER_PIXEL( destination->format )) - 1;

     mach64_out32( mmio, DST_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );
}

void mach64_set_source( Mach64DriverData *mdrv,
                        Mach64DeviceData *mdev,
                        CardState        *state )
{
     volatile __u8 *mmio   = mdrv->mmio_base;
     CoreSurface   *source = state->source;
     SurfaceBuffer *buffer = source->front_buffer;
     int            pitch  = buffer->video.pitch / DFB_BYTES_PER_PIXEL( source->format );

     if (MACH64_IS_VALID( m_source ))
          return;

     switch (source->format) {
          case DSPF_RGB332:
               mdev->src_bpp = SRC_8BPP;
               break;
          case DSPF_ARGB1555:
               mdev->src_bpp = SRC_15BPP;
               break;
          case DSPF_RGB16:
               mdev->src_bpp = SRC_16BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->src_bpp = SRC_32BPP;
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }
     mdev->src_key_mask = (1 << DFB_COLOR_BITS_PER_PIXEL( source->format )) - 1;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mmio, SRC_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );

     MACH64_VALIDATE( m_source );
}

void mach64_set_clip( Mach64DriverData *mdrv,
                      Mach64DeviceData *mdev,
                      CardState        *state )
{
     volatile __u8 *mmio   = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 2 );

     mach64_out32( mmio, SC_LEFT_RIGHT, (state->clip.x2 << 16) | state->clip.x1 );
     mach64_out32( mmio, SC_TOP_BOTTOM, (state->clip.y2 << 16) | state->clip.y1 );
}

void mach64_set_color( Mach64DriverData *mdrv,
                       Mach64DeviceData *mdev,
                       CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u32          color = 0;

     if (MACH64_IS_VALID( m_color ))
          return;

     switch (state->destination->format) {
          case DSPF_RGB332:
               color = PIXEL_RGB332( state->color.r,
                                     state->color.g,
                                     state->color.b );
               break;
          case DSPF_ARGB1555:
               color = PIXEL_ARGB1555( state->color.a,
                                       state->color.r,
                                       state->color.g,
                                       state->color.b );
               break;
          case DSPF_RGB16:
               color = PIXEL_RGB16( state->color.r,
                                    state->color.g,
                                    state->color.b );
               break;
          case DSPF_RGB32:
               color = PIXEL_RGB32( state->color.r,
                                    state->color.g,
                                    state->color.b );
               break;
          case DSPF_ARGB:
               color = PIXEL_ARGB( state->color.a,
                                   state->color.r,
                                   state->color.g,
                                   state->color.b );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, DP_FRGD_CLR, color );

     MACH64_VALIDATE( m_color );
}

void mach64_set_src_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev,
                              CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_srckey ))
          return;

     mach64_waitfifo( mdrv, mdev, 3 );
     mach64_out32( mmio, CLR_CMP_MASK, mdev->src_key_mask );
     mach64_out32( mmio, CLR_CMP_CLR, state->src_colorkey );
     mach64_out32( mmio, CLR_CMP_CNTL, COMPARE_EQUAL | COMPARE_SOURCE );

     MACH64_VALIDATE( m_srckey );
     MACH64_INVALIDATE( m_dstkey | m_disable_key );
}

void mach64_set_dst_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev,
                              CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_dstkey ))
          return;

     mach64_waitfifo( mdrv, mdev, 3 );
     mach64_out32( mmio, CLR_CMP_MASK, mdev->dst_key_mask );
     mach64_out32( mmio, CLR_CMP_CLR, state->dst_colorkey );
     mach64_out32( mmio, CLR_CMP_CNTL, COMPARE_NOT_EQUAL | COMPARE_DESTINATION );

     MACH64_VALIDATE( m_dstkey );
     MACH64_INVALIDATE( m_srckey | m_disable_key );
}

void mach64_disable_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_disable_key ))
          return;

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, CLR_CMP_CNTL, COMPARE_FALSE );

     MACH64_VALIDATE( m_disable_key );
     MACH64_INVALIDATE( m_srckey | m_dstkey );
}
