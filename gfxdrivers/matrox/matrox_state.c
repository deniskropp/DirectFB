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

#include <config.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/palette.h>

#include <gfx/convert.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

#include "matrox_state.h"

#define MGA_KEYMASK(format)   ((1 << DFB_COLOR_BITS_PER_PIXEL(format)) - 1)

static void matrox_calc_offsets( MatroxDeviceData *mdev,
                                 CoreSurface      *surface,
                                 SurfaceBuffer    *buffer,
                                 bool              unit_pixel,
                                 int               offset[2][3] )
{
     int bytes_per_pixel = DFB_BYTES_PER_PIXEL( surface->format );
     int pitch;

     if (unit_pixel) {
          offset[0][0] = buffer->video.offset / bytes_per_pixel;
          pitch        = buffer->video.pitch / bytes_per_pixel;
     } else {
          offset[0][0] = mdev->fb.offset + buffer->video.offset;
          pitch        = buffer->video.pitch;
     }

     switch (surface->format) {
     case DSPF_NV12:
     case DSPF_NV21:
          offset[0][1] = offset[0][0] + surface->height * pitch;
          offset[0][2] = 0;
          break;
     case DSPF_I420:
          offset[0][1] = offset[0][0] + surface->height * pitch;
          offset[0][2] = offset[0][1] + surface->height/2 * pitch/2;
          break;
     case DSPF_YV12:
          offset[0][2] = offset[0][0] + surface->height * pitch;
          offset[0][1] = offset[0][2] + surface->height/2 * pitch/2;
          break;
     default:
          offset[0][1] = 0;
          offset[0][2] = 0;
     }

     D_ASSERT( offset[0][0] % 64 == 0 );
     D_ASSERT( offset[0][1] % 64 == 0 );
     D_ASSERT( offset[0][2] % 64 == 0 );

     if (mdev->blit_fields || mdev->blit_deinterlace) {
          if (surface->caps & DSCAPS_SEPARATED) {
               offset[1][0] = offset[0][0] + surface->height/2 * pitch;
               switch (surface->format) {
               case DSPF_NV12:
               case DSPF_NV21:
                    offset[1][1] = offset[0][1] + surface->height/4 * pitch;
                    offset[1][2] = 0;
                    break;
               case DSPF_I420:
               case DSPF_YV12:
                    offset[1][1] = offset[0][1] + surface->height/4 * pitch/2;
                    offset[1][2] = offset[0][2] + surface->height/4 * pitch/2;
                    break;
               default:
                    offset[1][1] = 0;
                    offset[1][2] = 0;
               }
          } else {
               offset[1][0] = offset[0][0] + pitch;
               switch (surface->format) {
               case DSPF_NV12:
               case DSPF_NV21:
                    offset[1][1] = offset[0][1] + pitch;
                    offset[1][2] = 0;
                    break;
               case DSPF_I420:
               case DSPF_YV12:
                    offset[1][1] = offset[0][1] + pitch/2;
                    offset[1][2] = offset[0][2] + pitch/2;
                    break;
               default:
                    offset[1][1] = 0;
                    offset[1][2] = 0;
               }
          }

          D_ASSERT( offset[1][0] % 64 == 0 );
          D_ASSERT( offset[1][1] % 64 == 0 );
          D_ASSERT( offset[1][2] % 64 == 0 );
     }
}

void matrox_validate_destination( MatroxDriverData *mdrv,
                                  MatroxDeviceData *mdev,
                                  CardState        *state )
{
     volatile u8   *mmio            = mdrv->mmio_base;
     CoreSurface   *destination     = state->destination;
     SurfaceBuffer *buffer          = destination->back_buffer;
     SurfaceBuffer *depth_buffer    = destination->depth_buffer;
     int            bytes_per_pixel = DFB_BYTES_PER_PIXEL(buffer->format);

     if (MGA_IS_VALID( m_destination ))
          return;

     mdev->dst_pitch = buffer->video.pitch / bytes_per_pixel;

     mdev->depth_buffer = depth_buffer != NULL;

     if (destination->format == DSPF_YUY2 || destination->format == DSPF_UYVY)
          mdev->dst_pitch /= 2;

     if (mdev->blit_fields && !(destination->caps & DSCAPS_SEPARATED))
          mdev->dst_pitch *= 2;

     D_ASSERT( mdev->dst_pitch % 32 == 0 );

     matrox_calc_offsets( mdev, destination, buffer, mdev->old_matrox, mdev->dst_offset );

     mga_waitfifo( mdrv, mdev, depth_buffer ? 4 : 3 );

     mga_out32( mmio, mdev->dst_offset[0][0], mdev->old_matrox ? YDSTORG : DSTORG );
     mga_out32( mmio, mdev->dst_pitch, PITCH );

     if (depth_buffer)
          mga_out32( mmio, depth_buffer->video.offset, ZORG );

     switch (buffer->format) {
          case DSPF_A8:
          case DSPF_ALUT44:
          case DSPF_LUT8:
          case DSPF_RGB332:
               mga_out32( mmio, PW8, MACCESS );
               break;
          case DSPF_ARGB1555:
               mga_out32( mmio, PW16 | DIT555, MACCESS );
               break;
          case DSPF_ARGB4444:
          case DSPF_RGB16:
               mga_out32( mmio, PW16, MACCESS );
               break;
          case DSPF_RGB24:
               mga_out32( mmio, PW24, MACCESS );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mga_out32( mmio, PW32, MACCESS );
               break;
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
               mga_out32( mmio, PW8 | BYPASS332 | NODITHER, MACCESS );
               break;
          case DSPF_YUY2:
          case DSPF_UYVY:
               mga_out32( mmio, PW32 | NODITHER, MACCESS );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     MGA_VALIDATE( m_destination );
}

void matrox_set_clip( MatroxDriverData *mdrv,
                      MatroxDeviceData *mdev,
                      DFBRegion        *clip )
{
     volatile u8 *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 3 );

     if (mdev->old_matrox) {
          mga_out32( mmio, (mdev->dst_offset[0][0] +
                            mdev->dst_pitch * clip->y1) & 0xFFFFFF, YTOP );
          mga_out32( mmio, (mdev->dst_offset[0][0] +
                            mdev->dst_pitch * clip->y2) & 0xFFFFFF, YBOT );
     }
     else {
          mga_out32( mmio, (mdev->dst_pitch * clip->y1) & 0xFFFFFF, YTOP );
          mga_out32( mmio, (mdev->dst_pitch * clip->y2) & 0xFFFFFF, YBOT );
     }

     mga_out32( mmio, ((clip->x2 & 0x0FFF) << 16) | (clip->x1 & 0x0FFF), CXBNDRY );
}

void matrox_validate_drawColor( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state )
{
     DFBColor       color = state->color;
     volatile u8   *mmio  = mdrv->mmio_base;

     if (MGA_IS_VALID( m_drawColor ))
          return;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          color.r = (color.r * (color.a + 1)) >> 8;
          color.g = (color.g * (color.a + 1)) >> 8;
          color.b = (color.b * (color.a + 1)) >> 8;
     }

     mga_waitfifo( mdrv, mdev, 4 );

     mga_out32( mmio, U8_TO_F0915(color.a), ALPHASTART );
     mga_out32( mmio, U8_TO_F0915(color.r), DR4 );
     mga_out32( mmio, U8_TO_F0915(color.g), DR8 );
     mga_out32( mmio, U8_TO_F0915(color.b), DR12 );

     MGA_VALIDATE( m_drawColor );
     MGA_INVALIDATE( m_blitColor );
     MGA_INVALIDATE( m_blitBlend );
}

void matrox_validate_blitColor( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state )
{
     DFBColor       color = state->color;
     volatile u8   *mmio  = mdrv->mmio_base;

     if (MGA_IS_VALID( m_blitColor ))
          return;

     if (state->blittingflags & DSBLIT_COLORIZE) {
          if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
               color.r = (color.r * (color.a + 1)) >> 8;
               color.g = (color.g * (color.a + 1)) >> 8;
               color.b = (color.b * (color.a + 1)) >> 8;
          }
     }
     else {
          if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR)
               color.r = color.g = color.b = color.a;
          else
               color.r = color.g = color.b = 0xff;
     }

     mga_waitfifo( mdrv, mdev, 4 );

     mga_out32( mmio, U8_TO_F0915(color.a), ALPHASTART );
     mga_out32( mmio, U8_TO_F0915(color.r), DR4 );
     mga_out32( mmio, U8_TO_F0915(color.g), DR8 );
     mga_out32( mmio, U8_TO_F0915(color.b), DR12 );

     MGA_VALIDATE( m_blitColor );
     MGA_INVALIDATE( m_drawColor );
     MGA_INVALIDATE( m_blitBlend );
}

void matrox_validate_color( MatroxDriverData *mdrv,
                            MatroxDeviceData *mdev,
                            CardState        *state )
{
     DFBColor       color = state->color;
     volatile u8   *mmio  = mdrv->mmio_base;

     u32 fcol;
     u8  cb, cr;

     if (MGA_IS_VALID( m_color ))
          return;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          color.r = (color.r * (color.a + 1)) >> 8;
          color.g = (color.g * (color.a + 1)) >> 8;
          color.b = (color.b * (color.a + 1)) >> 8;
     }

     switch (state->destination->format) {
          case DSPF_ALUT44:
               fcol = (color.a & 0xF0) | state->color_index;
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               break;
          case DSPF_LUT8:
               fcol = state->color_index;
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               break;
          case DSPF_RGB332:
               fcol = PIXEL_RGB332( color.r,
                                    color.g,
                                    color.b );
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               break;
          case DSPF_ARGB4444:
               fcol = PIXEL_ARGB4444( color.a,
                                      color.r,
                                      color.g,
                                      color.b );
               fcol |= fcol << 16;
               break;
          case DSPF_ARGB1555:
               fcol = PIXEL_ARGB1555( color.a,
                                      color.r,
                                      color.g,
                                      color.b );
               fcol |= fcol << 16;
               break;
          case DSPF_RGB16:
               fcol = PIXEL_RGB16( color.r,
                                   color.g,
                                   color.b );
               fcol |= fcol << 16;
               break;
          case DSPF_RGB24:
               fcol = PIXEL_RGB32( color.r,
                                   color.g,
                                   color.b );
               fcol |= fcol << 24;
               break;
          case DSPF_RGB32:
               fcol = PIXEL_RGB32( color.r,
                                   color.g,
                                   color.b );
               break;
          case DSPF_ARGB:
               fcol = PIXEL_ARGB( color.a,
                                  color.r,
                                  color.g,
                                  color.b );
               break;
          case DSPF_A8:
               fcol = color.a;
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               break;
          case DSPF_I420:
          case DSPF_YV12:
               RGB_TO_YCBCR( color.r,
                             color.g,
                             color.b,
                             fcol, cb, cr );
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               mdev->color[0] = fcol;
               mdev->color[1] = (cb << 24) | (cb << 16) | (cb << 8) | cb;
               mdev->color[2] = (cr << 24) | (cr << 16) | (cr << 8) | cr;
               break;
          case DSPF_NV12:
               RGB_TO_YCBCR( color.r,
                             color.g,
                             color.b,
                             fcol, cb, cr );
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               mdev->color[0] = fcol;
               mdev->color[1] = (cr << 24) | (cb << 16) | (cr << 8) | cb;
               break;
          case DSPF_NV21:
               RGB_TO_YCBCR( color.r,
                             color.g,
                             color.b,
                             fcol, cb, cr );
               fcol |= fcol << 8;
               fcol |= fcol << 16;
               mdev->color[0] = fcol;
               mdev->color[1] = (cb << 24) | (cr << 16) | (cb << 8) | cr;
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r,
                             color.g,
                             color.b,
                             fcol, cb, cr );
               fcol = PIXEL_YUY2( fcol, cb, cr );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r,
                             color.g,
                             color.b,
                             fcol, cb, cr );
               fcol = PIXEL_UYVY( fcol, cb, cr );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               return;
     }

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, fcol, FCOL );

     MGA_VALIDATE( m_color );
     MGA_INVALIDATE( m_srckey );
}

static u32 matroxSourceBlend[] = {
     SRC_ZERO,                /* DSBF_ZERO         */
     SRC_ONE,                 /* DSBF_ONE          */
     0,                       /* DSBF_SRCCOLOR     */
     0,                       /* DSBF_INVSRCCOLOR  */
     SRC_ALPHA,               /* DSBF_SRCALPHA     */
     SRC_ONE_MINUS_SRC_ALPHA, /* DSBF_INVSRCALPHA  */
     SRC_DST_ALPHA,           /* DSBF_DESTALPHA    */
     SRC_ONE_MINUS_DST_ALPHA, /* DSBF_INVDESTALPHA */
     SRC_DST_COLOR,           /* DSBF_DESTCOLOR    */
     SRC_ONE_MINUS_DST_COLOR, /* DSBF_INVDESTCOLOR */
     SRC_SRC_ALPHA_SATURATE   /* DSBF_SRCALPHASAT  */
};

static u32 matroxDestBlend[] = {
     DST_ZERO,                /* DSBF_ZERO         */
     DST_ONE,                 /* DSBF_ONE          */
     DST_SRC_COLOR,           /* DSBF_SRCCOLOR     */
     DST_ONE_MINUS_SRC_COLOR, /* DSBF_INVSRCCOLOR  */
     DST_SRC_ALPHA,           /* DSBF_SRCALPHA     */
     DST_ONE_MINUS_SRC_ALPHA, /* DSBF_INVSRCALPHA  */
     DST_DST_ALPHA,           /* DSBF_DESTALPHA    */
     DST_ONE_MINUS_DST_ALPHA, /* DSBF_INVDESTALPHA */
     0,                       /* DSBF_DESTCOLOR    */
     0,                       /* DSBF_INVDESTCOLOR */
     0                        /* DSBF_SRCALPHASAT  */
};

void matrox_validate_drawBlend( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state )
{
     volatile u8 *mmio = mdrv->mmio_base;

     u32 alphactrl;

     if (MGA_IS_VALID( m_drawBlend ))
          return;

     alphactrl = matroxSourceBlend[state->src_blend - 1] |
                 matroxDestBlend  [state->dst_blend - 1] |
                 ALPHACHANNEL | DIFFUSEDALPHA;

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, alphactrl, ALPHACTRL );

     MGA_VALIDATE( m_drawBlend );
     MGA_INVALIDATE( m_blitBlend );
}

static u32 matroxAlphaSelect[] = {
     0,
     0,
     DIFFUSEDALPHA,
     MODULATEDALPHA
};

void matrox_validate_blitBlend( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state )
{
     volatile u8 *mmio = mdrv->mmio_base;

     u32 alphactrl;

     if (MGA_IS_VALID( m_blitBlend ))
          return;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                 DSBLIT_BLEND_COLORALPHA))
     {
          if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY)
               /* src_blend == ONE and dst_blend == INVSRCALPHA/INVSRCCOLOR */
               alphactrl = matroxSourceBlend[DSBF_SRCALPHA    - 1] |
                           matroxDestBlend  [state->dst_blend - 1] |
                           VIDEOALPHA;
          else
               alphactrl = matroxSourceBlend[state->src_blend - 1] |
                           matroxDestBlend  [state->dst_blend - 1] |
                           ALPHACHANNEL;

          if (state->source->format == DSPF_RGB32) {
               alphactrl |= DIFFUSEDALPHA;

               if (! (state->blittingflags & DSBLIT_BLEND_COLORALPHA)) {
                    mga_out32( mmio, U8_TO_F0915(0xff), ALPHASTART );
                    MGA_INVALIDATE( m_drawColor | m_blitColor );
               }
          }
          else
               alphactrl |= matroxAlphaSelect [state->blittingflags & 3];
     }
     else {
          alphactrl = SRC_ONE | DST_ZERO | ALPHACHANNEL;

          if (state->source->format == DSPF_RGB32) {
               alphactrl |= DIFFUSEDALPHA;

               mga_out32( mmio, U8_TO_F0915(0xff), ALPHASTART );
               MGA_INVALIDATE( m_drawColor | m_blitColor );
          }
     }

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, alphactrl, ALPHACTRL );

     MGA_VALIDATE( m_blitBlend );
     MGA_INVALIDATE( m_drawBlend );
}

static void matrox_tlutload( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CorePalette      *palette )
{
     volatile u8  *mmio = mdrv->mmio_base;
     volatile u16 *dst  = dfb_gfxcard_memory_virtual( NULL, mdev->tlut_offset );
     int             i;

     for (i = 0; i < palette->num_entries; i++)
          *dst++ = PIXEL_RGB16( palette->entries[i].r,
                                palette->entries[i].g,
                                palette->entries[i].b );

     mga_waitfifo( mdrv, mdev, mdev->old_matrox ? 8 : 9 );
     mga_out32( mmio, BLTMOD_BU32RGB | BOP_COPY | SHFTZERO |
                SGNZERO | LINEAR | ATYPE_RSTR | OP_BITBLT, DWGCTL );
     mga_out32( mmio, 1024, PITCH );
     if (mdev->old_matrox) {
          mga_out32( mmio, mdev->tlut_offset / 2, AR3 );
          mga_out32( mmio, palette->num_entries, AR0 );
          mga_out32( mmio, 0, YDSTORG );
     }
     else {
          mga_out32( mmio, 0, AR3 );
          mga_out32( mmio, palette->num_entries, AR0 );
          mga_out32( mmio, mdev->fb.offset + mdev->tlut_offset, SRCORG );
          mga_out32( mmio, 0, DSTORG );

          MGA_INVALIDATE( m_source );
     }
     mga_out32( mmio, 0, FXBNDRY );
     mga_out32( mmio, PW16 | TLUTLOAD, MACCESS );
     mga_out32( mmio, palette->num_entries, YDSTLEN | EXECUTE );

     MGA_INVALIDATE( m_destination );
}

void matrox_validate_Source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile u8   *mmio            = mdrv->mmio_base;
     CoreSurface   *surface         = state->source;
     SurfaceBuffer *buffer          = surface->front_buffer;
     int            bytes_per_pixel = DFB_BYTES_PER_PIXEL(surface->format);
     u32            texctl = 0, texctl2 = 0;

     if (MGA_IS_VALID( m_Source ))
          return;

     mdev->src_pitch = buffer->video.pitch / bytes_per_pixel;
     mdev->field     = surface->field;
     mdev->w         = surface->width;
     mdev->h         = surface->height;

     if (state->destination->format == DSPF_YUY2 || state->destination->format == DSPF_UYVY) {
          mdev->w /= 2;
          mdev->src_pitch /= 2;
     }

     if (mdev->blit_deinterlace || mdev->blit_fields) {
          mdev->h /= 2;
          if (!(surface->caps & DSCAPS_SEPARATED))
               mdev->src_pitch *= 2;
     }

     D_ASSERT( mdev->src_pitch % 32 == 0 );

     matrox_calc_offsets( mdev, surface, buffer, false, mdev->src_offset );

     if (mdev->blit_deinterlace && mdev->field) {
          mdev->src_offset[0][0] = mdev->src_offset[1][0];
          mdev->src_offset[0][1] = mdev->src_offset[1][1];
          mdev->src_offset[0][2] = mdev->src_offset[1][1];
     }

     mdev->w2 = mga_log2( mdev->w );
     mdev->h2 = mga_log2( mdev->h );

     if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
          texctl = TAMASK;
     else
          texctl = TAKEY;

     switch (surface->format) {
          case DSPF_YUY2:
               texctl |= (state->destination->format == DSPF_YUY2) ? TW32 : TW422;
               break;
          case DSPF_UYVY:
               texctl |= (state->destination->format == DSPF_UYVY) ? TW32 : TW422UYVY;
               break;
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_A8:
               texctl |= TW8A;
               break;
          case DSPF_ARGB4444:
               texctl |= TW12;
               break;
          case DSPF_ARGB1555:
               texctl |= TW15;
               break;
          case DSPF_RGB16:
               texctl |= TW16;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               texctl |= TW32;
               break;
          case DSPF_LUT8:
               matrox_tlutload( mdrv, mdev, surface->palette );
               texctl |= TW8;
               break;
          case DSPF_RGB332:
               matrox_tlutload( mdrv, mdev, mdev->rgb332_palette );
               texctl |= TW8;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     texctl |= ((mdev->src_pitch << 9) & TPITCHEXT) | TPITCHLIN;

     if (1 << mdev->w2 != mdev->w  ||  1 << mdev->h2 != mdev->h)
          texctl |= CLAMPUV;

     if (state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR))
          texctl |= TMODULATE;
     else
          texctl2 |= DECALDIS;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          texctl |= DECALCKEY | STRANS;
     else
          texctl2 |= CKSTRANSDIS;

     if (surface->format == DSPF_A8)
          texctl2 |= IDECAL | DECALDIS;

     mdev->texctl = texctl;

     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, texctl,  TEXCTL );
     mga_out32( mmio, texctl2, TEXCTL2 );

     mga_out32( mmio, ( (((u32)(mdev->w - 1) & 0x7ff) << 18) |
                        (((u32)(4 - mdev->w2) & 0x3f) <<  9) |
                        (((u32)(mdev->w2 + 4) & 0x3f)      )  ), TEXWIDTH );

     mga_out32( mmio, ( (((u32)(mdev->h - 1) & 0x7ff) << 18) |
                        (((u32)(4 - mdev->h2) & 0x3f) <<  9) |
                        (((u32)(mdev->h2 + 4) & 0x3f)      )  ), TEXHEIGHT );

     mga_out32( mmio, mdev->src_offset[0][0], TEXORG );

     MGA_VALIDATE( m_Source );
}

void matrox_validate_source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile u8   *mmio            = mdrv->mmio_base;
     CoreSurface   *surface         = state->source;
     SurfaceBuffer *buffer          = surface->front_buffer;
     int            bytes_per_pixel = DFB_BYTES_PER_PIXEL(surface->format);

     if (MGA_IS_VALID( m_source ))
          return;

     mdev->src_pitch = buffer->video.pitch / bytes_per_pixel;

     if (state->destination->format == DSPF_YUY2 || state->destination->format == DSPF_UYVY)
          mdev->src_pitch /= 2;

     if (mdev->blit_fields && !(surface->caps & DSCAPS_SEPARATED))
          mdev->src_pitch *= 2;

     D_ASSERT( mdev->src_pitch % 32 == 0 );

     matrox_calc_offsets( mdev, surface, buffer, mdev->old_matrox, mdev->src_offset );

     if (!mdev->old_matrox) {
          mga_waitfifo( mdrv, mdev, 1 );
          mga_out32( mmio, mdev->src_offset[0][0], SRCORG );
     }

     MGA_VALIDATE( m_source );
}

void matrox_validate_SrcKey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile u8   *mmio    = mdrv->mmio_base;
     CoreSurface   *surface = state->source;
     u32            key;
     u32            mask;

     if (MGA_IS_VALID( m_SrcKey ))
          return;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          mask = MGA_KEYMASK(surface->format);
          key  = state->src_colorkey & mask;
     } else {
          mask = 0;
          key  = 0xFFFF;
     }

     mga_waitfifo( mdrv, mdev, 2);

     mga_out32( mmio, ((mask & 0xFFFF) << 16) | (key & 0xFFFF), TEXTRANS );
     mga_out32( mmio, (mask & 0xFFFF0000) | (key >> 16), TEXTRANSHIGH );

     MGA_VALIDATE( m_SrcKey );
}

void matrox_validate_srckey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile u8   *mmio    = mdrv->mmio_base;
     CoreSurface   *surface = state->source;
     u32            key;
     u32            mask;

     if (MGA_IS_VALID( m_srckey ))
          return;

     mask = MGA_KEYMASK(surface->format);
     key  = state->src_colorkey & mask;

     switch (DFB_BYTES_PER_PIXEL(state->source->format)) {
          case 1:
               mask |= mask << 8;
               key  |= key  << 8;
          case 2:
               mask |= mask << 16;
               key  |= key  << 16;
     }

     mga_waitfifo( mdrv, mdev, 2);
     mga_out32( mmio, mask, BCOL );
     mga_out32( mmio, key, FCOL );

     MGA_VALIDATE( m_srckey );
     MGA_INVALIDATE( m_color );
}

