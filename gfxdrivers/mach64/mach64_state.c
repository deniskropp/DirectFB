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

#include <direct/messages.h>
#include <direct/util.h>

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

     switch (destination->format) {
          case DSPF_RGB332:
               mdev->dst_pix_width = DST_PIX_WIDTH_RGB332;
               break;
          case DSPF_ARGB1555:
               mdev->dst_pix_width = DST_PIX_WIDTH_ARGB1555;
               break;
          case DSPF_RGB16:
               mdev->dst_pix_width = DST_PIX_WIDTH_RGB565;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->dst_pix_width = DST_PIX_WIDTH_ARGB8888;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }
     mdev->dst_key_mask = (1 << DFB_COLOR_BITS_PER_PIXEL( destination->format )) - 1;

     mach64_waitfifo( mdrv, mdev, 1 );
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
               mdev->src_pix_width = SRC_PIX_WIDTH_RGB332 | SCALE_PIX_WIDTH_RGB332;
               break;
          case DSPF_ARGB1555:
               mdev->src_pix_width = SRC_PIX_WIDTH_ARGB1555 | SCALE_PIX_WIDTH_ARGB1555;
               break;
          case DSPF_RGB16:
               mdev->src_pix_width = SRC_PIX_WIDTH_RGB565 | SCALE_PIX_WIDTH_RGB565;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->src_pix_width = SRC_PIX_WIDTH_ARGB8888 | SCALE_PIX_WIDTH_ARGB8888;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }
     mdev->src_key_mask = (1 << DFB_COLOR_BITS_PER_PIXEL( source->format )) - 1;

#if 0
     mdev->tex_pitch = dfb_log2( pitch );
     mdev->tex_height = dfb_log2( source->height );
     mdev->tex_size = MAX( mdev->tex_pitch, mdev->tex_height );
#endif

     mach64_waitfifo( mdrv, mdev, 2 );

     mach64_out32( mmio, TEX_SIZE_PITCH, (mdev->tex_pitch  << 0) |
                                         (mdev->tex_size   << 4) |
                                         (mdev->tex_height << 8) );

     mach64_out32( mmio, SRC_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );

     mdev->source = source;

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
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, DP_FRGD_CLR, color );

     MACH64_VALIDATE( m_color );
}

void mach64_set_color_3d( Mach64DriverData *mdrv,
                          Mach64DeviceData *mdev,
                          CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_color_3d ))
          return;

     mach64_waitfifo( mdrv, mdev, 6 );
     mach64_out32( mmio, RED_START, state->color.r << 16 );
     mach64_out32( mmio, GREEN_START, state->color.g << 16 );
     mach64_out32( mmio, BLUE_START, state->color.b << 16 );
     mach64_out32( mmio, ALPHA_START, state->color.a << 16 );

     /* These (and RED_START) are shared with some scaler registers. */
     mach64_out32( mmio, RED_X_INC, 0 );
     mach64_out32( mmio, GREEN_X_INC, 0 );

     MACH64_INVALIDATE( m_blit_blend );
     MACH64_VALIDATE( m_color_3d );
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
     MACH64_INVALIDATE( m_srckey_scale | m_dstkey | m_disable_key );
}

void mach64_set_src_colorkey_scale( Mach64DriverData *mdrv,
                                    Mach64DeviceData *mdev,
                                    CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_srckey_scale ))
          return;

     mach64_waitfifo( mdrv, mdev, 3 );
     mach64_out32( mmio, CLR_CMP_MASK, mdev->src_key_mask );
     mach64_out32( mmio, CLR_CMP_CLR, state->src_colorkey );
     mach64_out32( mmio, CLR_CMP_CNTL, COMPARE_EQUAL | COMPARE_SCALE );

     MACH64_VALIDATE( m_srckey_scale );
     MACH64_INVALIDATE( m_srckey | m_dstkey | m_disable_key );
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
     MACH64_INVALIDATE( m_srckey | m_srckey_scale | m_disable_key );
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
     MACH64_INVALIDATE( m_srckey | m_srckey_scale | m_dstkey );
}

static __u32 mach64SourceBlend[] = {
     ALPHA_BLND_SRC_ZERO,
     ALPHA_BLND_SRC_ONE,
     0,
     0,
     ALPHA_BLND_SRC_SRCALPHA,
     ALPHA_BLND_SRC_INVSRCALPHA,
     ALPHA_BLND_SRC_DSTALPHA,
     ALPHA_BLND_SRC_INVDSTALPHA,
     ALPHA_BLND_SRC_DSTCOLOR,
     ALPHA_BLND_SRC_INVDSTCOLOR,
     ALPHA_BLND_SAT
};

static __u32 mach64DestBlend[] = {
     ALPHA_BLND_DST_ZERO,
     ALPHA_BLND_DST_ONE,
     ALPHA_BLND_DST_SRCCOLOR,
     ALPHA_BLND_DST_INVSRCCOLOR,
     ALPHA_BLND_DST_SRCALPHA,
     ALPHA_BLND_DST_INVSRCALPHA,
     ALPHA_BLND_DST_DSTALPHA,
     ALPHA_BLND_DST_INVDSTALPHA,
     0,
     0,
     0
};

void mach64_set_draw_blend( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            CardState        *state )
{
     __u32 scale_3d_cntl;

     if (MACH64_IS_VALID( m_draw_blend ))
          return;

     scale_3d_cntl = SCALE_3D_FCN_SHADE | ALPHA_FOG_EN_ALPHA |
                     mach64SourceBlend[state->src_blend - 1] |
                     mach64DestBlend  [state->dst_blend - 1];

     mdev->draw_blend = scale_3d_cntl;

     MACH64_VALIDATE( m_draw_blend );
}

void mach64_set_blit_blend( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            CardState        *state )
{
     __u32 scale_3d_cntl;

     if (MACH64_IS_VALID( m_blit_blend ))
          return;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                 DSBLIT_COLORIZE))
          scale_3d_cntl = SCALE_3D_FCN_TEXTURE | MIP_MAP_DISABLE;
     else
          scale_3d_cntl = SCALE_3D_FCN_SCALE;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                 DSBLIT_BLEND_COLORALPHA)) {
          scale_3d_cntl |= ALPHA_FOG_EN_ALPHA |
                           mach64SourceBlend[state->src_blend - 1] |
                           mach64DestBlend  [state->dst_blend - 1];

          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) {
               if (state->source->format == DSPF_RGB32) {
                    mach64_waitfifo( mdrv, mdev, 1 );
                    mach64_out32( mdrv->mmio_base, ALPHA_START, 0xFF << 16 );
                    MACH64_INVALIDATE( m_color_3d );
               } else {
                    scale_3d_cntl |= TEX_MAP_AEN;
               }
          }
     }

     if (state->blittingflags & DSBLIT_COLORIZE)
          scale_3d_cntl |= TEX_LIGHT_FCN_MODULATE;

     mdev->blit_blend = scale_3d_cntl;

     MACH64_VALIDATE( m_blit_blend );
}
