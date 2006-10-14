/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// Hardware setting functions ------------------------------------------------

#include <config.h>

#include "uc_hw.h"
#include <core/state.h>
#include <core/palette.h>
#include <gfx/convert.h>

/// Integer 2-logarithm, y = log2(x), where x and y are integers.
#define ILOG2(x,y) ILOG2_PORTABLE(x,y)

#define ILOG2_PORTABLE(x,y) \
    do {                    \
        unsigned int i = 0; \
        y = x;              \
        while (y != 0) {    \
            i++;            \
            y = y >> 1;     \
        }                   \
        y = i-1;            \
    } while (0)

#define ILOG2_X86(x,y)  // TODO - use BSR (bit scan reverse) instruction

/// Set alpha blending function (3D)
inline void
uc_set_blending_fn( UcDriverData *ucdrv,
                    UcDeviceData *ucdev,
                    CardState    *state )
{
     struct uc_fifo     *fifo    = ucdrv->fifo;
     struct uc_hw_alpha *hwalpha = &ucdev->hwalpha;

     if (UC_IS_VALID( uc_blending_fn ))
          return;

     uc_map_blending_fn( hwalpha, state->src_blend, state->dst_blend,
                         state->destination->format );

     UC_FIFO_PREPARE( fifo, 14 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLCsat,   hwalpha->regHABLCsat   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLCop,    hwalpha->regHABLCop    );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLAsat,   hwalpha->regHABLAsat   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLAop,    hwalpha->regHABLAop    );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRCa,    hwalpha->regHABLRCa    );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRFCa,   hwalpha->regHABLRFCa   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRCbias, hwalpha->regHABLRCbias );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRCb,    hwalpha->regHABLRCb    );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRFCb,   hwalpha->regHABLRFCb   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRAa,    hwalpha->regHABLRAa    );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HABLRAb,    hwalpha->regHABLRAb    );

     UC_FIFO_PAD_EVEN( fifo );

     UC_FIFO_CHECK( fifo );

     UC_VALIDATE( uc_blending_fn );
}

/// Set texture environment (3D)
inline void
uc_set_texenv( UcDriverData *ucdrv,
               UcDeviceData *ucdev,
               CardState    *state )
{
     struct uc_fifo       *fifo  = ucdrv->fifo;
     struct uc_hw_texture *hwtex = &ucdev->hwtex;

     if (UC_IS_VALID( uc_texenv ))
          return;

     uc_map_blitflags( hwtex, state->blittingflags, state->source->format );

     // Texture mapping method
     hwtex->regHTXnTB   = HC_HTXnFLSs_Linear | HC_HTXnFLTs_Linear |
                          HC_HTXnFLSe_Linear | HC_HTXnFLTe_Linear;

     hwtex->regHTXnMPMD = HC_HTXnMPMD_Sclamp | HC_HTXnMPMD_Tclamp;

     UC_FIFO_PREPARE( fifo, 12 );
     UC_FIFO_ADD_HDR( fifo, (HC_ParaType_Tex << 16) | (HC_SubType_Tex0 << 24) );

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTB,       hwtex->regHTXnTB         );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnMPMD,     hwtex->regHTXnMPMD       );

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLCsat,  hwtex->regHTXnTBLCsat_0  );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLCop,   hwtex->regHTXnTBLCop_0   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLMPfog, hwtex->regHTXnTBLMPfog_0 );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLAsat,  hwtex->regHTXnTBLAsat_0  );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLRCb,   hwtex->regHTXnTBLRCb_0   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLRAa,   hwtex->regHTXnTBLRAa_0   );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnTBLRFog,  hwtex->regHTXnTBLRFog_0  );

     UC_FIFO_PAD_EVEN( fifo );

     UC_FIFO_CHECK( fifo );

     UC_VALIDATE( uc_texenv );
}

/// Set clipping rectangle (2D and 3D)
inline void
uc_set_clip( UcDriverData *ucdrv,
             UcDeviceData *ucdev,
             CardState    *state )
{
     struct uc_fifo *fifo = ucdrv->fifo;

     if (DFB_REGION_EQUAL( ucdev->clip, state->clip ))
          return;

     UC_FIFO_PREPARE( fifo, 8 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

#ifdef UC_ENABLE_3D

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HClipTB,
                      (RS12(state->clip.y1) << 12) | RS12(state->clip.y2+1) );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HClipLR,
                      (RS12(state->clip.x1) << 12) | RS12(state->clip.x2+1) );

#endif

     UC_FIFO_ADD_2D ( fifo, VIA_REG_CLIPTL,
                      (RS16(state->clip.y1) << 16) | RS16(state->clip.x1) );
     UC_FIFO_ADD_2D ( fifo, VIA_REG_CLIPBR,
                      (RS16(state->clip.y2) << 16) | RS16(state->clip.x2) );

     UC_FIFO_CHECK( fifo );

     ucdev->clip = state->clip;
}

/// Set destination (2D and 3D)
inline void
uc_set_destination( UcDriverData *ucdrv,
                    UcDeviceData *ucdev,
                    CardState    *state )
{
     struct uc_fifo        *fifo        = ucdrv->fifo;

     CoreSurface           *destination = state->destination;
     SurfaceBuffer         *buffer      = destination->back_buffer;

     DFBSurfacePixelFormat  dst_format  = destination->format;
     int                    dst_offset  = buffer->video.offset;
     int                    dst_pitch   = buffer->video.pitch;
     int                    dst_bpp     = DFB_BYTES_PER_PIXEL( dst_format );


     /* Save FIFO space and CPU cycles. */
     if (ucdev->dst_format == dst_format &&
         ucdev->dst_offset == dst_offset &&
         ucdev->dst_pitch  == dst_pitch)
          return;

     // 2D engine setting

     ucdev->pitch = (ucdev->pitch & 0x7fff) | (((dst_pitch >> 3) & 0x7fff) << 16);

     UC_FIFO_PREPARE( fifo, 12 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );


     UC_FIFO_ADD_2D ( fifo, VIA_REG_PITCH,   (VIA_PITCH_ENABLE | ucdev->pitch) );
     UC_FIFO_ADD_2D ( fifo, VIA_REG_DSTBASE, (dst_offset >> 3) );
     UC_FIFO_ADD_2D ( fifo, VIA_REG_GEMODE,  (dst_bpp - 1) << 8 );

#ifdef UC_ENABLE_3D
     // 3D engine setting

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HDBBasL, dst_offset & 0xffffff );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HDBBasH, dst_offset >> 24 );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HDBFM,   (uc_map_dst_format( dst_format ) |
                                              (dst_pitch & HC_HDBPit_MASK)    |
                                              HC_HDBLoc_Local) );

     UC_FIFO_PAD_EVEN(fifo);
#endif

     UC_FIFO_CHECK( fifo );

     ucdev->dst_format = dst_format;
     ucdev->dst_offset = dst_offset;
     ucdev->dst_pitch  = dst_pitch;
}

/// Set new source (2D)
inline void
uc_set_source_2d( UcDriverData *ucdrv,
                  UcDeviceData *ucdev,
                  CardState    *state )
{
     struct uc_fifo *fifo   = ucdrv->fifo;
     SurfaceBuffer  *buffer = state->source->front_buffer;

     if (UC_IS_VALID( uc_source2d ))
          return;

     ucdev->pitch &= 0x7fff0000;
     ucdev->pitch |= (buffer->video.pitch >> 3) & 0x7fff;

     UC_FIFO_PREPARE( fifo, 6 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

     UC_FIFO_ADD_2D ( fifo, VIA_REG_SRCBASE, buffer->video.offset >> 3 );
     UC_FIFO_ADD_2D ( fifo, VIA_REG_PITCH,   VIA_PITCH_ENABLE | ucdev->pitch );

     UC_FIFO_CHECK( fifo );

     UC_VALIDATE( uc_source2d );
}

/// Set new source (3D)
inline void
uc_set_source_3d( UcDriverData *ucdrv,
                  UcDeviceData *ucdev,
                  CardState    *state )
{
     struct uc_fifo       *fifo   = ucdrv->fifo;
     struct uc_hw_texture *hwtex  = &ucdev->hwtex;

     CoreSurface          *source = state->source;
     SurfaceBuffer        *buffer = source->front_buffer;

     int src_height, src_offset, src_pitch;

     if (UC_IS_VALID( uc_source3d ))
          return;

     src_height = source->height;
     src_offset = buffer->video.offset;
     src_pitch  = buffer->video.pitch;

     /*
      * TODO: Check if we can set the odd/even field as L1/L2 texture and select
      * between L0/L1/L2 upon blit. Otherwise we depend on SMF_BLITTINGFLAGS ;(
      */

     if (state->blittingflags & DSBLIT_DEINTERLACE) {
          if (source->field)
               src_offset += src_pitch;

          src_height >>= 1;
          src_pitch  <<= 1;
     }

     ucdev->field = source->field;

     // Round texture size up to nearest
     // value evenly divisible by 2^n

     ILOG2(source->width, hwtex->we);
     hwtex->l2w = 1 << hwtex->we;
     if (hwtex->l2w < source->width) {
          hwtex->we++;
          hwtex->l2w <<= 1;
     }

     ILOG2(src_height, hwtex->he);
     hwtex->l2h = 1 << hwtex->he;
     if (hwtex->l2h < src_height) {
          hwtex->he++;
          hwtex->l2h <<= 1;
     }

     hwtex->format = uc_map_src_format_3d( source->format );

     UC_FIFO_PREPARE( fifo, 10);

     UC_FIFO_ADD_HDR( fifo, (HC_ParaType_Tex << 16) | (HC_SubType_Tex0 << 24));

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnFM,       HC_HTXnLoc_Local | hwtex->format );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL0OS,     (0 << HC_HTXnLVmax_SHIFT) );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL0_5WE,   hwtex->we );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL0_5HE,   hwtex->he );

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL012BasH, (src_offset >> 24) & 0xff );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL0BasL,   (src_offset      ) & 0xffffff );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HTXnL0Pit,    (HC_HTXnEnPit_MASK | src_pitch) );

     UC_FIFO_PAD_EVEN( fifo );

     UC_FIFO_CHECK( fifo );

     // Upload the palette of a 256 color texture.

     if (hwtex->format == HC_HTXnFM_Index8) {
          int       i, num;
          DFBColor *colors;

          UC_FIFO_PREPARE( fifo, 258 );

          UC_FIFO_ADD_HDR( fifo, ((HC_ParaType_Palette    << 16) |
                                  (HC_SubType_TexPalette0 << 24)) );

          colors = source->palette->entries;
          num    = source->palette->num_entries;

          if (num > 256)
               num = 256;

          /* What about the last entry? -- dok */
          for (i = 0; i < num; i++)
               UC_FIFO_ADD( fifo, PIXEL_ARGB(colors[i].a, colors[i].r,
                                             colors[i].g, colors[i].b) );

          for (; i < 256; i++)
               UC_FIFO_ADD( fifo, 0 );

          UC_FIFO_CHECK( fifo );
     }

     UC_VALIDATE( uc_source3d );
}

/// Set either destination color key, or fill color, as needed. (2D)
inline void
uc_set_color_2d( UcDriverData *ucdrv,
                 UcDeviceData *ucdev,
                 CardState    *state )
{
     struct uc_fifo *fifo  = ucdrv->fifo;
     u32             color = 0;

     if (UC_IS_VALID( uc_color2d ))
          return;

     switch (state->destination->format) {
          case DSPF_ARGB1555:
               color = PIXEL_ARGB1555( state->color.a,
                                       state->color.r,
                                       state->color.g,
                                       state->color.b );
               color |= color << 16;
               break;

          case DSPF_RGB16:
               color = PIXEL_RGB16( state->color.r,
                                    state->color.g,
                                    state->color.b);
               color |= color << 16;
               break;

          case DSPF_RGB32:
          case DSPF_ARGB:
               color = PIXEL_ARGB( state->color.a,
                                   state->color.r,
                                   state->color.g,
                                   state->color.b );
               break;

          default:
               D_BUG( "unexpected pixel format" );
     }


     UC_FIFO_PREPARE( fifo, 8 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

     // Opaque line drawing needs this
     UC_FIFO_ADD_2D( fifo, VIA_REG_MONOPAT0,   0xff );

     UC_FIFO_ADD_2D( fifo, VIA_REG_KEYCONTROL, 0 );
     UC_FIFO_ADD_2D( fifo, VIA_REG_FGCOLOR,    color );

     UC_FIFO_CHECK( fifo );

     UC_VALIDATE( uc_color2d );
     UC_INVALIDATE( uc_colorkey2d );
}

inline void
uc_set_colorkey_2d( UcDriverData *ucdrv,
                    UcDeviceData *ucdev,
                    CardState    *state )
{
     struct uc_fifo *fifo = ucdrv->fifo;

     if (UC_IS_VALID( uc_colorkey2d ))
          return;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          UC_FIFO_PREPARE( fifo, 6 );
          UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

          UC_FIFO_ADD_2D ( fifo, VIA_REG_KEYCONTROL, VIA_KEY_ENABLE_SRCKEY );
          UC_FIFO_ADD_2D ( fifo, VIA_REG_BGCOLOR, state->src_colorkey );
     }
     else if (state->blittingflags & DSBLIT_DST_COLORKEY) {
          UC_FIFO_PREPARE( fifo, 6 );
          UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

          UC_FIFO_ADD_2D ( fifo, VIA_REG_KEYCONTROL,
                           VIA_KEY_ENABLE_DSTKEY | VIA_KEY_INVERT_KEY );
          UC_FIFO_ADD_2D ( fifo, VIA_REG_FGCOLOR, state->dst_colorkey );
     }
     else {
          UC_FIFO_PREPARE( fifo, 4 );
          UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

          UC_FIFO_ADD_2D ( fifo, VIA_REG_KEYCONTROL, 0 );
     }

     UC_FIFO_CHECK( fifo );

     UC_VALIDATE( uc_colorkey2d );
     UC_INVALIDATE( uc_color2d );
}

