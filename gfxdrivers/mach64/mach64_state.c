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

     mdev->pix_width &= ~DST_PIX_WIDTH;
     switch (destination->format) {
          case DSPF_RGB332:
               mdev->pix_width |= DST_PIX_WIDTH_8BPP;
               break;
          case DSPF_ARGB1555:
               mdev->pix_width |= DST_PIX_WIDTH_15BPP;
               break;
          case DSPF_RGB16:
               mdev->pix_width |= DST_PIX_WIDTH_16BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->pix_width |= DST_PIX_WIDTH_32BPP;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, DST_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );
}

void mach64gt_set_destination( Mach64DriverData *mdrv,
                               Mach64DeviceData *mdev,
                               CardState        *state )
{
     volatile __u8 *mmio        = mdrv->mmio_base;
     CoreSurface   *destination = state->destination;
     SurfaceBuffer *buffer      = destination->back_buffer;
     int            pitch       = buffer->video.pitch / DFB_BYTES_PER_PIXEL( destination->format );

     mdev->pix_width &= ~DST_PIX_WIDTH;
     switch (destination->format) {
          case DSPF_RGB332:
               mdev->pix_width |= DST_PIX_WIDTH_RGB332;
               break;
          case DSPF_ARGB1555:
               mdev->pix_width |= DST_PIX_WIDTH_ARGB1555;
               break;
          case DSPF_ARGB4444:
               mdev->pix_width |= DST_PIX_WIDTH_ARGB4444;
               break;
          case DSPF_RGB16:
               mdev->pix_width |= DST_PIX_WIDTH_RGB565;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->pix_width |= DST_PIX_WIDTH_ARGB8888;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mdev->draw_blend &= ~DITHER_EN;
     mdev->blit_blend &= ~DITHER_EN;
     if (DFB_COLOR_BITS_PER_PIXEL( destination->format ) < 24) {
          mdev->draw_blend |= DITHER_EN;
          mdev->blit_blend |= DITHER_EN;
     }

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

     mdev->pix_width &= ~SRC_PIX_WIDTH;
     switch (source->format) {
          case DSPF_RGB332:
               mdev->pix_width |= SRC_PIX_WIDTH_8BPP;
               break;
          case DSPF_ARGB1555:
               mdev->pix_width |= SRC_PIX_WIDTH_15BPP;
               break;
          case DSPF_RGB16:
               mdev->pix_width |= SRC_PIX_WIDTH_16BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->pix_width |= SRC_PIX_WIDTH_32BPP;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, SRC_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );

     MACH64_VALIDATE( m_source );
}

void mach64gt_set_source( Mach64DriverData *mdrv,
                          Mach64DeviceData *mdev,
                          CardState        *state )
{
     volatile __u8 *mmio   = mdrv->mmio_base;
     CoreSurface   *source = state->source;
     SurfaceBuffer *buffer = source->front_buffer;
     int            pitch  = buffer->video.pitch / DFB_BYTES_PER_PIXEL( source->format );

     if (MACH64_IS_VALID( m_source ))
          return;

     mdev->pix_width &= ~SRC_PIX_WIDTH;
     switch (source->format) {
          case DSPF_RGB332:
               mdev->pix_width |= SRC_PIX_WIDTH_RGB332;
               break;
          case DSPF_ARGB1555:
               mdev->pix_width |= SRC_PIX_WIDTH_ARGB1555;
               break;
          case DSPF_ARGB4444:
               mdev->pix_width |= SRC_PIX_WIDTH_ARGB4444;
               break;
          case DSPF_RGB16:
               mdev->pix_width |= SRC_PIX_WIDTH_RGB565;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->pix_width |= SRC_PIX_WIDTH_ARGB8888;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, SRC_OFF_PITCH, (buffer->video.offset/8) | ((pitch/8) << 22) );

     MACH64_VALIDATE( m_source );
}

void mach64gt_set_source_scale( Mach64DriverData *mdrv,
                                Mach64DeviceData *mdev,
                                CardState        *state )
{
     volatile __u8 *mmio   = mdrv->mmio_base;
     CoreSurface   *source = state->source;
     SurfaceBuffer *buffer = source->front_buffer;
     int            offset = buffer->video.offset;
     int            pitch  = buffer->video.pitch;
     int            height = source->height;

     if (MACH64_IS_VALID( m_source_scale ))
          return;

     mdev->pix_width &= ~SCALE_PIX_WIDTH;
     switch (source->format) {
          case DSPF_RGB332:
               mdev->pix_width |= SCALE_PIX_WIDTH_RGB332;
               break;
          case DSPF_ARGB1555:
               mdev->pix_width |= SCALE_PIX_WIDTH_ARGB1555;
               break;
          case DSPF_ARGB4444:
               mdev->pix_width |= SCALE_PIX_WIDTH_ARGB4444;
               break;
          case DSPF_RGB16:
               mdev->pix_width |= SCALE_PIX_WIDTH_RGB565;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mdev->pix_width |= SCALE_PIX_WIDTH_ARGB8888;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mdev->blit_blend &= ~SCALE_PIX_EXPAND;
     if (DFB_COLOR_BITS_PER_PIXEL( source->format ) < 24)
          mdev->blit_blend |= SCALE_PIX_EXPAND;

     mdev->field = source->field;
     if (mdev->blit_deinterlace) {
          if (mdev->field) {
               if (source->caps & DSCAPS_SEPARATED) {
                    offset += height/2 * pitch;
               } else {
                    offset += pitch;
                    pitch  *= 2;
               }
          }
          height /= 2;
     }

     mdev->source = source;

     mdev->scale_offset = offset;
     mdev->scale_pitch  = pitch;
     
     mdev->tex_offset = offset;
     mdev->tex_pitch  = direct_log2( pitch / DFB_BYTES_PER_PIXEL( source->format ) );
     mdev->tex_height = direct_log2( height );
     mdev->tex_size   = MAX( mdev->tex_pitch, mdev->tex_height );

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, TEX_SIZE_PITCH, (mdev->tex_pitch  << 0) |
                                         (mdev->tex_size   << 4) |
                                         (mdev->tex_height << 8) );

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mach64_waitfifo( mdrv, mdev, 1 );
          mach64_out32( mmio, TEX_CNTL, TEX_CACHE_FLUSH );
     }

     MACH64_VALIDATE( m_source_scale );
}

void mach64_set_clip( Mach64DriverData *mdrv,
                      Mach64DeviceData *mdev,
                      CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 2 );
     mach64_out32( mmio, SC_LEFT_RIGHT, (S13( state->clip.x2 ) << 16) | S13( state->clip.x1 ) );
     mach64_out32( mmio, SC_TOP_BOTTOM, (S14( state->clip.y2 ) << 16) | S14( state->clip.y1 ) );
}

void mach64_set_color( Mach64DriverData *mdrv,
                       Mach64DeviceData *mdev,
                       CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u32          clr;

     if (MACH64_IS_VALID( m_color ))
          return;

     switch (state->destination->format) {
          case DSPF_RGB332:
               clr = PIXEL_RGB332( state->color.r,
                                   state->color.g,
                                   state->color.b );
               break;
          case DSPF_ARGB1555:
               clr = PIXEL_ARGB1555( state->color.a,
                                     state->color.r,
                                     state->color.g,
                                     state->color.b );
               break;
          case DSPF_ARGB4444:
               clr = PIXEL_ARGB4444( state->color.a,
                                     state->color.r,
                                     state->color.g,
                                     state->color.b );
               break;
          case DSPF_RGB16:
               clr = PIXEL_RGB16( state->color.r,
                                  state->color.g,
                                  state->color.b );
               break;
          case DSPF_RGB32:
               clr = PIXEL_RGB32( state->color.r,
                                  state->color.g,
                                  state->color.b );
               break;
          case DSPF_ARGB:
               clr = PIXEL_ARGB( state->color.a,
                                 state->color.r,
                                 state->color.g,
                                 state->color.b );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, DP_FRGD_CLR, clr );

     MACH64_VALIDATE( m_color );
}

void mach64_set_color_3d( Mach64DriverData *mdrv,
                          Mach64DeviceData *mdev,
                          CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_color_3d ))
          return;

     /* Some 3D color registers scaler registers are shared. */
     mach64_waitfifo( mdrv, mdev, 7 );
     mach64_out32( mmio, RED_X_INC, 0 );
     mach64_out32( mmio, RED_START, state->color.r << 16 );
     mach64_out32( mmio, GREEN_X_INC, 0 );
     mach64_out32( mmio, GREEN_START, state->color.g << 16 );
     mach64_out32( mmio, BLUE_X_INC, 0 );
     mach64_out32( mmio, BLUE_START, state->color.b << 16 );
     mach64_out32( mmio, ALPHA_START, state->color.a << 16 );

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
     mach64_out32( mmio, CLR_CMP_MSK,
                   (1 << DFB_COLOR_BITS_PER_PIXEL( state->source->format )) - 1 );
     mach64_out32( mmio, CLR_CMP_CLR, state->src_colorkey );
     mach64_out32( mmio, CLR_CMP_CNTL, CLR_CMP_FN_EQUAL | CLR_CMP_SRC_2D );

     MACH64_VALIDATE( m_srckey );
     MACH64_INVALIDATE( m_srckey_scale | m_dstkey | m_disable_key );
}

void mach64_set_src_colorkey_scale( Mach64DriverData *mdrv,
                                    Mach64DeviceData *mdev,
                                    CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u32          clr, msk;

     if (MACH64_IS_VALID( m_srckey_scale ))
          return;

     if (mdev->chip < CHIP_3D_RAGE_PRO) {
          switch (state->source->format) {
               case DSPF_RGB332:
                    clr = ((state->src_colorkey & 0xE0) << 16) |
                          ((state->src_colorkey & 0x1C) << 11) |
                          ((state->src_colorkey & 0x03) <<  6);
                    msk = 0xE0E0C0;
                    break;
               case DSPF_ARGB4444:
                    clr = ((state->src_colorkey & 0x0F00) << 12) |
                          ((state->src_colorkey & 0x00F0) <<  8) |
                          ((state->src_colorkey & 0x000F) <<  4);
                    msk = 0xF0F0F0;
                    break;
               case DSPF_ARGB1555:
                    clr = ((state->src_colorkey & 0x7C00) << 9) |
                          ((state->src_colorkey & 0x03E0) << 6) |
                          ((state->src_colorkey & 0x001F) << 3);
                    msk = 0xF8F8F8;
                    break;
               case DSPF_RGB16:
                    clr = ((state->src_colorkey & 0xF800) << 8) |
                          ((state->src_colorkey & 0x07E0) << 5) |
                          ((state->src_colorkey & 0x001F) << 3);
                    msk = 0xF8FCF8;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
                    clr = state->src_colorkey;
                    msk = 0xFFFFFF;
                    break;
               default:
                    D_BUG( "unexpected pixelformat!" );
                    return;
          }
     } else {
          clr = state->src_colorkey;
          msk = (1 << DFB_COLOR_BITS_PER_PIXEL( state->source->format )) - 1;
     }

     mach64_waitfifo( mdrv, mdev, 3 );
     mach64_out32( mmio, CLR_CMP_MSK, msk );
     mach64_out32( mmio, CLR_CMP_CLR, clr );
     mach64_out32( mmio, CLR_CMP_CNTL, CLR_CMP_FN_EQUAL | CLR_CMP_SRC_SCALE );

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
     mach64_out32( mmio, CLR_CMP_MSK,
                   (1 << DFB_COLOR_BITS_PER_PIXEL( state->destination->format )) - 1 );
     mach64_out32( mmio, CLR_CMP_CLR, state->dst_colorkey );
     mach64_out32( mmio, CLR_CMP_CNTL, CLR_CMP_FN_NOT_EQUAL | CLR_CMP_SRC_DEST );

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
     mach64_out32( mmio, CLR_CMP_CNTL, CLR_CMP_FN_FALSE );

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
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_draw_blend ))
          return;

     mdev->draw_blend &= DITHER_EN;
     mdev->draw_blend |= ALPHA_FOG_EN_ALPHA |
                         mach64SourceBlend[state->src_blend - 1] |
                         mach64DestBlend  [state->dst_blend - 1];

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          /* FIXME: This is wrong. */
          mach64_waitfifo( mdrv, mdev, 1 );
          mach64_out32( mmio, ALPHA_TST_CNTL, ALPHA_DST_SEL_DSTALPHA );
     }

     MACH64_VALIDATE( m_draw_blend );
}

void mach64_set_blit_blend( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MACH64_IS_VALID( m_blit_blend ))
          return;

     mdev->blit_blend &= SCALE_PIX_EXPAND | DITHER_EN;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          mdev->blit_blend |= ALPHA_FOG_EN_ALPHA |
                              mach64SourceBlend[state->src_blend - 1] |
                              mach64DestBlend  [state->dst_blend - 1];

          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) {
               if (DFB_PIXELFORMAT_HAS_ALPHA( state->source->format )) {
                    mdev->blit_blend |= TEX_MAP_AEN;
               } else {
                    mach64_waitfifo( mdrv, mdev, 1 );
                    mach64_out32( mmio, ALPHA_START, 0xFF << 16 );
                    MACH64_INVALIDATE( m_color_3d );
               }
          }

          if (mdev->chip >= CHIP_3D_RAGE_PRO) {
               /* FIXME: This is wrong. */
               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, ALPHA_TST_CNTL, ALPHA_DST_SEL_DSTALPHA );
          }
     } else {
          /* This needs to be set even without alpha blending.
           * Otherwise alpha channel won't get copied.
           */
          if (DFB_PIXELFORMAT_HAS_ALPHA( state->source->format ))
               mdev->blit_blend |= TEX_MAP_AEN;

          if (mdev->chip >= CHIP_3D_RAGE_PRO) {
               /* FIXME: This is wrong. */
               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, ALPHA_TST_CNTL, ALPHA_DST_SEL_SRCALPHA );
          }
     }

     if (state->blittingflags & DSBLIT_COLORIZE)
          mdev->blit_blend |= TEX_LIGHT_FCN_MODULATE;

     MACH64_VALIDATE( m_blit_blend );
}
