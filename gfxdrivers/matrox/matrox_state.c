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

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

#include "matrox_state.h"

static int pixelpitch = 0;

int matrox_tmu = 0;
int matrox_w2;
int matrox_h2;

inline void matrox_set_destination()
{
     CoreSurface   *surface = matrox->state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;

     pixelpitch = buffer->video.pitch / BYTES_PER_PIXEL(surface->format);

     mga_waitfifo( mmio_base, 3 );
     mga_out32( mmio_base, pixelpitch, PITCH );
     mga_out32( mmio_base, buffer->video.offset, DSTORG );

     switch (surface->format) {
          case DSPF_RGB15:
               mga_out32( mmio_base, PW16 | NODITHER | DIT555, MACCESS );
               break;
          case DSPF_RGB16:
               mga_out32( mmio_base, PW16 | NODITHER, MACCESS );
               break;
          case DSPF_RGB24:
               mga_out32( mmio_base, PW24 | NODITHER, MACCESS );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               mga_out32( mmio_base, PW32 | NODITHER, MACCESS );
               break;
     }
}

inline void matrox_set_clip()
{
     mga_waitfifo( mmio_base, 3 );
     mga_out32( mmio_base, ((matrox->state->clip.x2 & 0x0FFF) << 16) |
                           (matrox->state->clip.x1 & 0x0FFF), CXBNDRY );
     mga_out32( mmio_base, (pixelpitch * matrox->state->clip.y1) & 0xFFFFFF, YTOP );
     mga_out32( mmio_base, (pixelpitch * matrox->state->clip.y2) & 0xFFFFFF, YBOT );
}

inline void matrox_validate_Color()
{
     if (m_Color)
          return;

     mga_waitfifo( mmio_base, 4 );

     mga_out32( mmio_base, U8_TO_F0915(matrox->state->color.a), ALPHASTART );
     mga_out32( mmio_base, U8_TO_F0915(matrox->state->color.r), DR4 );
     mga_out32( mmio_base, U8_TO_F0915(matrox->state->color.g), DR8 );
     mga_out32( mmio_base, U8_TO_F0915(matrox->state->color.b), DR12 );

     m_Color = 1;
}

inline void matrox_validate_color()
{
     __u32 color;

     if (m_color)
          return;

     switch (matrox->state->destination->format) {
          case DSPF_RGB15:
               color = PIXEL_RGB15( matrox->state->color.r,
                                    matrox->state->color.g,
                                    matrox->state->color.b );
               break;
          case DSPF_RGB16:
               color = PIXEL_RGB16( matrox->state->color.r,
                                    matrox->state->color.g,
                                    matrox->state->color.b );
               break;
          case DSPF_RGB24:
               color = PIXEL_RGB24( matrox->state->color.r,
                                    matrox->state->color.g,
                                    matrox->state->color.b );
               break;
          case DSPF_RGB32:
               color = PIXEL_RGB32( matrox->state->color.r,
                                    matrox->state->color.g,
                                    matrox->state->color.b );
               break;
          case DSPF_ARGB:
               color = PIXEL_ARGB( matrox->state->color.a,
                                   matrox->state->color.r,
                                   matrox->state->color.g,
                                   matrox->state->color.b );
               break;
          case DSPF_A8:
               color = matrox->state->color.a;
               break;
          default:
               ONCE( "Warning, unsupported pixel format in Matrox driver!" );
               color = 0;
     }

     mga_waitfifo( mmio_base, 1 );
     mga_out32( mmio_base, color, FCOL );

     m_color = 1;
     m_srckey = 0;
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

static __u32 matroxModulation[] = {
     ALPHACHANNEL,
     ALPHACHANNEL,
     ALPHACHANNEL | DIFFUSEDALPHA,
     ALPHACHANNEL | MODULATEDALPHA
};

inline void matrox_validate_Blend()
{
     __u32 alphactrl;

     if (m_Blend)
          return;

     alphactrl = matroxSourceBlend[matrox->state->src_blend - 1] |
                 matroxDestBlend  [matrox->state->dst_blend - 1] |
                 matroxModulation [matrox->state->blittingflags & 3];

     mga_waitfifo( mmio_base, 1 );
     mga_out32( mmio_base, alphactrl, ALPHACTRL );

     m_Blend = 1;
}

inline void matrox_validate_Source()
{
     __u32 texctl, texctl2;
     
     CoreSurface   *surface = matrox->state->source;
     SurfaceBuffer *buffer  = surface->front_buffer;

     if (m_Source)
          return;

     src_pixelpitch = buffer->video.pitch / BYTES_PER_PIXEL(surface->format);
     
     matrox_w2 = log2( src_pixelpitch );
     matrox_h2 = log2( surface->height );

     texctl = (matrox->state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) ?
              TAMASK : TAKEY;

     switch (surface->format) {
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
               texctl |= TW32;
               break;
          case DSPF_ARGB:
               texctl |= TW32;
               break;
          default:
               BUG( "There is no fallback!" );
               return;
     }

     if (matrox->state->blittingflags & DSBLIT_COLORIZE)
          texctl |= TMODULATE;
     
     if (matrox->state->blittingflags & DSBLIT_SRC_COLORKEY) {
          texctl |= DECALCKEY | STRANS;
          texctl2 = DECALDIS;
     }
     else
          texctl2 = DECALDIS | CKSTRANSDIS;

     mga_waitfifo( mmio_base, 10);
     
     mga_out32( mmio_base, 0, TMR1 );
     mga_out32( mmio_base, 0, TMR2 );
     mga_out32( mmio_base, 0, TMR4 );
     mga_out32( mmio_base, 0, TMR5 );
     mga_out32( mmio_base, 0x10000, TMR8 );

     mga_out32( mmio_base, CLAMPUV |
                           ((src_pixelpitch)<<9) | PITCHEXT | texctl, TEXCTL );
     mga_out32( mmio_base, texctl2, TEXCTL2 );
     mga_out32( mmio_base, ((src_pixelpitch -1)<<18) | matrox_w2<<9 | matrox_w2, TEXWIDTH );
     mga_out32( mmio_base, ((surface->height-1)<<18) | matrox_h2<<9 | matrox_h2, TEXHEIGHT );
     mga_out32( mmio_base, buffer->video.offset, TEXORG );

     m_Source = 1;
}

inline void matrox_validate_source()
{
     CoreSurface   *surface = matrox->state->source;
     SurfaceBuffer *buffer  = surface->front_buffer;
     
     if (m_source)
          return;

     src_pixelpitch = buffer->video.pitch / BYTES_PER_PIXEL(surface->format);
     
     mga_waitfifo( mmio_base, 2);

     mga_out32( mmio_base,
                (1 << BITS_PER_PIXEL(surface->format)) - 1, BCOL );
     mga_out32( mmio_base, buffer->video.offset, SRCORG );

     m_source = 1;
}

inline void matrox_validate_SrcKey()
{
     if (m_SrcKey)
          return;

     mga_waitfifo( mmio_base, 2);
     
     if (BYTES_PER_PIXEL(matrox->state->source->format) > 2) {
          mga_out32( mmio_base, (0xFFFF << 16) |
                     (matrox->state->src_colorkey & 0xFFFF),
                     TEXTRANS );
          mga_out32( mmio_base, (((1 << (BITS_PER_PIXEL(matrox->state->source->format)-16)) - 1) << 16) |
                     ((matrox->state->src_colorkey & 0xFFFF0000) >> 16),
                     TEXTRANSHIGH );
     }
     else {
          mga_out32( mmio_base, (((1 << BITS_PER_PIXEL(matrox->state->source->format)) - 1) << 16) |
                     (matrox->state->src_colorkey & 0xFFFF),
                     TEXTRANS );
          mga_out32( mmio_base, 0, TEXTRANSHIGH );
     }

     m_SrcKey = 1;
}

inline void matrox_validate_srckey()
{
     if (m_srckey)
          return;

     mga_waitfifo( mmio_base, 1);
     mga_out32( mmio_base, matrox->state->src_colorkey, FCOL );

     m_srckey = 1;
     m_color = 0;
}

inline void matrox_set_dwgctl( DFBAccelerationMask accel )
{
     mga_waitfifo( mmio_base, 1 );
     
     switch (accel) {
          case DFXL_FILLRECTANGLE: {
               unsigned int atype = config->matrox_sgram ? ATYPE_BLK : ATYPE_RSTR;

               if (matrox->state->drawingflags & DSDRAW_BLEND)
                    mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO |
                                          ARZERO | ATYPE_I | OP_TRAP,
                               DWGCTL );
               else
                    mga_out32( mmio_base, TRANSC | BOP_COPY | SHFTZERO |
                                          SGNZERO | ARZERO | SOLID |
                                          atype | OP_TRAP,
                               DWGCTL );

               break;
          }
          case DFXL_DRAWRECTANGLE: {
               if (matrox->state->drawingflags & DSDRAW_BLEND)
                    mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                                          OP_AUTOLINE_OPEN,
                               DWGCTL );
               else
                    mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                                          ATYPE_RSTR | OP_AUTOLINE_OPEN,
                               DWGCTL );
               
               break;
          }
          case DFXL_DRAWLINE: {
               if (matrox->state->drawingflags & DSDRAW_BLEND)
                    mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                                          OP_AUTOLINE_CLOSE,
                               DWGCTL );
               else
                    mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                                          ATYPE_RSTR | OP_AUTOLINE_CLOSE,
                               DWGCTL );
               
               break;
          }
          case DFXL_FILLTRIANGLE: {
               unsigned int atype = config->matrox_sgram ? ATYPE_BLK : ATYPE_RSTR;

               if (matrox->state->drawingflags & DSDRAW_BLEND)
                    mga_out32( mmio_base, BOP_COPY | SHFTZERO | ATYPE_I | OP_TRAP,
                               DWGCTL );
               else
                    mga_out32( mmio_base, TRANSC | BOP_COPY | SHFTZERO |
                                          SOLID | atype | OP_TRAP,
                               DWGCTL );

               break;
          }
          case DFXL_BLIT: {
               if (matrox_tmu) {
                    mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                               ATYPE_I | OP_TEXTURE_TRAP,
                               DWGCTL );
                    
                    mga_waitfifo( mmio_base, 3 );
                    mga_out32( mmio_base, 0x100000 / (1<<matrox_w2), TMR0 );
                    mga_out32( mmio_base, 0x100000 / (1<<matrox_h2), TMR3 );
                    mga_out32( mmio_base, MAG_NRST | MIN_NRST, TEXFILTER );
               }
               else {
                    __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO |
                                   ATYPE_RSTR | OP_BITBLT;

                    if (matrox->state->blittingflags & DSBLIT_SRC_COLORKEY)
                         dwgctl |= TRANSC;

                    mga_out32( mmio_base, dwgctl, DWGCTL );
               }
               break;
          }
          case DFXL_STRETCHBLIT: {
               mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                          ATYPE_I | OP_TEXTURE_TRAP,
                          DWGCTL );
               
               mga_waitfifo( mmio_base, 1 );
               mga_out32( mmio_base, MAG_BILIN | MIN_BILIN, TEXFILTER );
               break;
          }
          default:
     }
}
