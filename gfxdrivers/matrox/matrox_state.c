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

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <misc/util.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

#include "matrox_state.h"

#define MGA_KEYMASK(format)   ((1 << MIN(24, DFB_BITS_PER_PIXEL(format))) - 1)

void matrox_set_destination( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CoreSurface      *destination )
{
     volatile __u8 *mmio            = mdrv->mmio_base;
     SurfaceBuffer *buffer          = destination->back_buffer;
     int            bytes_per_pixel = DFB_BYTES_PER_PIXEL(destination->format);

     mdev->dst_pixelpitch  = buffer->video.pitch / bytes_per_pixel;
     mdev->dst_pixeloffset = buffer->video.offset / bytes_per_pixel;

     mga_waitfifo( mdrv, mdev, 3 );

     if (mdev->old_matrox)
          mga_out32( mmio, mdev->dst_pixeloffset, YDSTORG );
     else
          mga_out32( mmio, dfb_gfxcard_memory_physical( buffer->video.offset) &
                     0x1FFFFFF, DSTORG );

     mga_out32( mmio, mdev->dst_pixelpitch, PITCH );

     switch (destination->format) {
          case DSPF_A8:
          case DSPF_RGB332:
               mga_out32( mmio, PW8, MACCESS );
               break;
          case DSPF_RGB15:
               mga_out32( mmio, PW16 | DIT555, MACCESS );
               break;
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
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }
}

void matrox_set_clip( MatroxDriverData *mdrv,
                      MatroxDeviceData *mdev,
                      DFBRegion        *clip )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 3 );

     if (mdev->old_matrox) {
          mga_out32( mmio, (mdev->dst_pixeloffset +
                            mdev->dst_pixelpitch * clip->y1) & 0xFFFFFF, YTOP );
          mga_out32( mmio, (mdev->dst_pixeloffset +
                            mdev->dst_pixelpitch * clip->y2) & 0xFFFFFF, YBOT );
     }
     else {
          mga_out32( mmio, (mdev->dst_pixelpitch * clip->y1) & 0xFFFFFF, YTOP );
          mga_out32( mmio, (mdev->dst_pixelpitch * clip->y2) & 0xFFFFFF, YBOT );
     }

     mga_out32( mmio, ((clip->x2 & 0x0FFF) << 16) |
                       (clip->x1 & 0x0FFF), CXBNDRY );
}

void matrox_validate_Color( MatroxDriverData *mdrv,
                            MatroxDeviceData *mdev,
                            CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (MGA_IS_VALID( m_Color ))
          return;

     mga_waitfifo( mdrv, mdev, 4 );

     mga_out32( mmio, U8_TO_F0915(state->color.a), ALPHASTART );
     mga_out32( mmio, U8_TO_F0915(state->color.r), DR4 );
     mga_out32( mmio, U8_TO_F0915(state->color.g), DR8 );
     mga_out32( mmio, U8_TO_F0915(state->color.b), DR12 );

     MGA_VALIDATE( m_Color );
     MGA_INVALIDATE( m_blitBlend );
}

void matrox_validate_color( MatroxDriverData *mdrv,
                            MatroxDeviceData *mdev,
                            CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 color;

     if (MGA_IS_VALID( m_color ))
          return;

     switch (state->destination->format) {
          case DSPF_RGB332:
               color = PIXEL_RGB332( state->color.r,
                                     state->color.g,
                                     state->color.b );
               color |= color << 8;
               color |= color << 16;
               break;
          case DSPF_RGB15:
               color = PIXEL_RGB15( state->color.r,
                                    state->color.g,
                                    state->color.b );
               color |= color << 16;
               break;
          case DSPF_RGB16:
               color = PIXEL_RGB16( state->color.r,
                                    state->color.g,
                                    state->color.b );
               color |= color << 16;
               break;
          case DSPF_RGB24:
               color = PIXEL_RGB24( state->color.r,
                                    state->color.g,
                                    state->color.b );
               color |= color << 24;
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
          case DSPF_A8:
               color = state->color.a;
               color |= color << 8;
               color |= color << 16;
               break;
          default:
               BUG( "unexpected pixelformat!" );
               return;
     }

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, color, FCOL );

     MGA_VALIDATE( m_color );
     MGA_INVALIDATE( m_srckey );
}

static __u32 matroxSourceBlend[] = {
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

static __u32 matroxDestBlend[] = {
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
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 alphactrl;

     if (MGA_IS_VALID( m_drawBlend ))
          return;
     
     alphactrl = matroxSourceBlend[state->src_blend - 1] |
                 matroxDestBlend  [state->dst_blend - 1] | DIFFUSEDALPHA;

     if (state->dst_blend == DSBF_ZERO)
          alphactrl |= ALPHACHANNEL;
     else
          alphactrl |= VIDEOALPHA;

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, alphactrl, ALPHACTRL );

     MGA_VALIDATE( m_drawBlend );
     MGA_INVALIDATE( m_blitBlend );
}

static __u32 matroxAlphaSelect[] = {
     0,
     0,
     DIFFUSEDALPHA,
     MODULATEDALPHA
};

void matrox_validate_blitBlend( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 alphactrl;

     if (MGA_IS_VALID( m_blitBlend ))
          return;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                 DSBLIT_BLEND_COLORALPHA))
     {
          alphactrl = matroxSourceBlend[state->src_blend - 1] |
                      matroxDestBlend  [state->dst_blend - 1];
          
          if (state->source->format == DSPF_RGB32) {
               alphactrl |= DIFFUSEDALPHA;

               if (! (state->blittingflags & DSBLIT_BLEND_COLORALPHA)) {
                    mga_out32( mmio, U8_TO_F0915(0xff), ALPHASTART );
                    MGA_INVALIDATE( m_Color );
               }
          }
          else
               alphactrl |= matroxAlphaSelect [state->blittingflags & 3];
          
          if (state->dst_blend == DSBF_ZERO)
               alphactrl |= ALPHACHANNEL;
          else
               alphactrl |= VIDEOALPHA;
     }
     else {
          alphactrl = SRC_ONE | ALPHACHANNEL;
          
          if (state->source->format == DSPF_RGB32) {
               alphactrl |= DIFFUSEDALPHA;

               mga_out32( mmio, U8_TO_F0915(0xff), ALPHASTART );
               MGA_INVALIDATE( m_Color );
          }
     }

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mmio, alphactrl, ALPHACTRL );

     MGA_VALIDATE( m_blitBlend );
     MGA_INVALIDATE( m_drawBlend );
}

void matrox_validate_Source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 texctl, texctl2;

     CoreSurface   *surface = state->source;
     SurfaceBuffer *buffer  = surface->front_buffer;

     if (MGA_IS_VALID( m_Source ))
          return;

     mdev->src_pixelpitch = buffer->video.pitch / DFB_BYTES_PER_PIXEL(surface->format);

     mdev->matrox_w2 = log2( surface->width );
     mdev->matrox_h2 = log2( surface->height );

     if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
          texctl = TAMASK;
     else
          texctl = TAKEY;

     switch (surface->format) {
          case DSPF_YUY2:
               texctl |= TW422;
               break;
          case DSPF_UYVY:
               texctl |= TW422UYVY;
               break;
          case DSPF_A8:
               texctl |= TW8A;
               break;
          case DSPF_RGB15:
               texctl |= TW15;
               break;
          case DSPF_RGB16:
               texctl |= TW16;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               texctl |= TW32;
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }

     texctl |= CLAMPUV | ((mdev->src_pixelpitch&0x7ff)<<9) | PITCHEXT | NOPERSPECTIVE;

     if (state->blittingflags & DSBLIT_COLORIZE)
          texctl |= TMODULATE;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          texctl |= DECALCKEY | STRANS;
          texctl2 = DECALDIS;
     }
     else
          texctl2 = DECALDIS | CKSTRANSDIS;

     if (surface->format == DSPF_A8)
          texctl2 |= IDECAL;

     mga_waitfifo( mdrv, mdev, 5);

     mga_out32( mmio, texctl, TEXCTL );
     mga_out32( mmio, texctl2, TEXCTL2 );
     mga_out32( mmio, ((surface->width -1)<<18) |
                      ((8-mdev->matrox_w2)&63)<<9 | mdev->matrox_w2, TEXWIDTH );
     mga_out32( mmio, ((surface->height-1)<<18) |
                      ((8-mdev->matrox_h2)&63)<<9 | mdev->matrox_h2, TEXHEIGHT );
     mga_out32( mmio, dfb_gfxcard_memory_physical( buffer->video.offset ) &
                0x1FFFFFF, TEXORG );

     MGA_VALIDATE( m_Source );
}

void matrox_validate_source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile __u8 *mmio            = mdrv->mmio_base;
     CoreSurface   *surface         = state->source;
     SurfaceBuffer *buffer          = surface->front_buffer;
     int            bytes_per_pixel = DFB_BYTES_PER_PIXEL(surface->format);

     if (MGA_IS_VALID( m_source ))
          return;

     mdev->src_pixelpitch = buffer->video.pitch / bytes_per_pixel;

     if (mdev->old_matrox)
          mdev->src_pixeloffset = buffer->video.offset / bytes_per_pixel;
     else {
          mga_waitfifo( mdrv, mdev, 1);
          mga_out32( mmio, dfb_gfxcard_memory_physical( buffer->video.offset) &
                     0x1FFFFFF, SRCORG );
     }

     MGA_VALIDATE( m_source );
}

void matrox_validate_SrcKey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile __u8 *mmio    = mdrv->mmio_base;
     CoreSurface   *surface = state->source;
     __u32          key;
     __u32          mask;

     if (MGA_IS_VALID( m_SrcKey ))
          return;

     mask = MGA_KEYMASK(surface->format);
     key  = state->src_colorkey & mask;

     mga_waitfifo( mdrv, mdev, 2);

     mga_out32( mmio, ((mask & 0xFFFF) << 16) | (key & 0xFFFF), TEXTRANS );
     mga_out32( mmio, (mask & 0xFFFF0000) | (key >> 16), TEXTRANSHIGH );

     MGA_VALIDATE( m_SrcKey );
}

void matrox_validate_srckey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state )
{
     volatile __u8 *mmio    = mdrv->mmio_base;
     CoreSurface   *surface = state->source;
     __u32          key;
     __u32          mask;

     if (MGA_IS_VALID( m_srckey ))
          return;

     mask = MGA_KEYMASK(surface->format);
     key  = state->src_colorkey & mask;

     mga_waitfifo( mdrv, mdev, 2);

     if (DFB_BYTES_PER_PIXEL(state->source->format) > 2)
          mga_out32( mmio, mask, BCOL );
     else
          mga_out32( mmio, 0xFFFFFFFF, BCOL );

     switch (DFB_BYTES_PER_PIXEL(state->source->format)) {
          case 1:
               mga_out32( mmio, key | (key <<  8) |
                                      (key << 16) |
                                      (key << 24), FCOL );
               break;
          case 2:
               mga_out32( mmio, key | (key << 16), FCOL );
               break;
          case 3:
          case 4:
               mga_out32( mmio, key, FCOL );
               break;
          default:
               BUG( "unexpected bytes per pixel" );
               break;
     }

     MGA_VALIDATE( m_srckey );
     MGA_INVALIDATE( m_color );
}

