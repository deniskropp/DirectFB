/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// Hardware setting functions ------------------------------------------------

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
void uc_set_blending_fn(struct uc_fifo* fifo, UcDeviceData *ucdev, CardState *state)
{
    if (ucdev->v_blending_fn)
        return;
    
    uc_map_blending_fn(&(ucdev->hwalpha), state->src_blend,
              state->dst_blend, DFB_BITS_PER_PIXEL(state->destination->format));

    UC_FIFO_PREPARE(fifo, 14);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLCsat, ucdev->hwalpha.regHABLCsat);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLCop,  ucdev->hwalpha.regHABLCop);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLAsat, ucdev->hwalpha.regHABLAsat);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLAop,  ucdev->hwalpha.regHABLAop);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRCa,  ucdev->hwalpha.regHABLRCa);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRFCa, ucdev->hwalpha.regHABLRFCa);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRCbias, ucdev->hwalpha.regHABLRCbias);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRCb,  ucdev->hwalpha.regHABLRCb);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRFCb, ucdev->hwalpha.regHABLRFCb);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRAa,  ucdev->hwalpha.regHABLRAa);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HABLRAb,  ucdev->hwalpha.regHABLRAb);

    UC_FIFO_PAD_EVEN(fifo);

    UC_FIFO_CHECK(fifo);
    
    ucdev->v_blending_fn = 1;
}

/// Set texture environment (3D)
void uc_set_texenv(struct uc_fifo* fifo, UcDeviceData* ucdev, CardState* state)
{
    struct uc_hw_texture* tex;

    if (ucdev->v_texenv)
        return;

    uc_map_blitflags(&(ucdev->hwtex), state->blittingflags,                      
                     state->source->format);

    tex = &(ucdev->hwtex);


    // Texture mapping method
    tex->regHTXnTB = HC_HTXnFLSs_Linear | HC_HTXnFLTs_Linear |
        HC_HTXnFLSe_Linear | HC_HTXnFLTe_Linear;
    //tex->regHTXnTB = HC_HTXnFLSs_Nearest | HC_HTXnFLTs_Nearest |
    //HC_HTXnFLSe_Nearest | HC_HTXnFLTe_Nearest;

    tex->regHTXnMPMD = HC_HTXnMPMD_Sclamp | HC_HTXnMPMD_Tclamp;

    UC_FIFO_PREPARE(fifo, 12);

    UC_FIFO_ADD_HDR(fifo, (HC_ParaType_Tex << 16) | (HC_SubType_Tex0 << 24));

    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTB, tex->regHTXnTB);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnMPMD, tex->regHTXnMPMD);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLCsat, tex->regHTXnTBLCsat_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLCop, tex->regHTXnTBLCop_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLMPfog, tex->regHTXnTBLMPfog_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLAsat, tex->regHTXnTBLAsat_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLRCb, tex->regHTXnTBLRCb_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLRAa, tex->regHTXnTBLRAa_0);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnTBLRFog, tex->regHTXnTBLRFog_0);

    UC_FIFO_PAD_EVEN(fifo);

    UC_FIFO_CHECK(fifo);
    
    ucdev->v_texenv = 1;
}

/// Set clipping rectangle (2D and 3D)
void uc_set_clip(struct uc_fifo* fifo, CardState* state)
{
    UC_FIFO_PREPARE(fifo, 8);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HClipTB,
        (RS12(state->clip.y1) << 12) | RS12(state->clip.y2+1));
    UC_FIFO_ADD_3D(fifo, HC_SubA_HClipLR,
        (RS12(state->clip.x1) << 12) | RS12(state->clip.x2+1));

    UC_FIFO_ADD_2D(fifo, VIA_REG_CLIPTL,
        ((RS16(state->clip.y1) << 16) | RS16(state->clip.x1)));
    UC_FIFO_ADD_2D(fifo, VIA_REG_CLIPBR,
        ((RS16(state->clip.y2) << 16) | RS16(state->clip.x2)));

    UC_FIFO_CHECK(fifo);
}

/// Set destination (2D and 3D)
void uc_set_destination(struct uc_fifo* fifo, UcDeviceData *ucdev,
                        CardState* state)
{
    const int gemodes[4] = {
        VIA_GEM_8bpp, VIA_GEM_16bpp, VIA_GEM_32bpp, VIA_GEM_32bpp
    };
    int dbuf = state->destination->back_buffer->video.offset;
    int dpitch = state->destination->back_buffer->video.pitch;
    int format;
    int bpp; // Number of bytes per pixel.

    UC_FIFO_PREPARE(fifo, 12);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    // 3D engine setting

    format = uc_map_dst_format(state->destination->format,
        &(ucdev->colormask), &(ucdev->alphamask));
    if (format == -1) {
        BUG("Unexpected pixelformat!");
        format = HC_HDBFM_ARGB8888;
    }

    UC_FIFO_ADD_3D(fifo, HC_SubA_HDBBasL, dbuf & 0xffffff);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HDBBasH, dbuf >> 24);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HDBFM,
        format | (dpitch & HC_HDBPit_MASK) | HC_HDBLoc_Local);

    // 2D engine setting

    bpp = DFB_BYTES_PER_PIXEL(state->destination->format);
    if ((bpp != 1) && (bpp != 2) && (bpp != 4)) {
        BUG("Unexpected pixelformat!");
        bpp = 4;
    }

    ucdev->pitch = (ucdev->pitch & 0xffff) | (((dpitch >> 3) & 0xffff) << 16);
    UC_FIFO_ADD_2D(fifo, VIA_REG_PITCH, VIA_PITCH_ENABLE | ucdev->pitch);
    UC_FIFO_ADD_2D(fifo, VIA_REG_DSTBASE, dbuf >> 3);
    UC_FIFO_ADD_2D(fifo, VIA_REG_GEMODE, gemodes[bpp >> 1]);

    UC_FIFO_PAD_EVEN(fifo);

    UC_FIFO_CHECK(fifo);
}

/// Set new source (2D)
void uc_set_source_2d(struct uc_fifo* fifo, UcDeviceData *ucdev,
                      CardState* state)
{
    SurfaceBuffer* buf = state->source->front_buffer;

    if (ucdev->v_source2d)
        return;

    ucdev->pitch = (ucdev->pitch & 0xffff0000)
        | ((buf->video.pitch >> 3) & 0xffff) | VIA_PITCH_ENABLE;

    UC_FIFO_PREPARE(fifo, 6);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    UC_FIFO_ADD_2D(fifo, VIA_REG_SRCBASE, buf->video.offset >> 3);
    UC_FIFO_ADD_2D(fifo, VIA_REG_PITCH, ucdev->pitch);

    UC_FIFO_CHECK(fifo);
    
    ucdev->v_source2d = 1;
}

/// Set new source (3D)
void uc_set_source_3d(struct uc_fifo* fifo, UcDeviceData *ucdev,
                      CardState* state)
{
    struct uc_hw_texture* tex;
    CoreSurface* src;

    if (ucdev->v_source3d)
        return;

    tex = &(ucdev->hwtex);
    src = state->source;
    tex->surface = src;

    // Round texture size up to nearest
    // value evenly divisible by 2^n

    ILOG2(src->width, tex->we);
    tex->l2w = 1 << tex->we;
    if (tex->l2w < src->width) {
        tex->we++;
        tex->l2w <<= 1;
    }

    ILOG2(src->height, tex->he);
    tex->l2h = 1 << tex->he;
    if (tex->l2h < src->height) {
        tex->he++;
        tex->l2h <<= 1;
    }

    tex->format = uc_map_src_format_3d(src->format);

    if (tex->format == 0xffffffff) {
        BUG("Unexpected pixelformat!");
        tex->format = HC_HTXnFM_ARGB8888;
    }

    UC_FIFO_PREPARE(fifo, 14);

    UC_FIFO_ADD_HDR(fifo, (HC_ParaType_Tex << 16) | (HC_SubType_TexGeneral << 24));
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXSMD, 1);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXSMD, 0);

    UC_FIFO_ADD_HDR(fifo, (HC_ParaType_Tex << 16) | (HC_SubType_Tex0 << 24));

    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnFM, HC_HTXnLoc_Local | tex->format);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL0OS, (0 << HC_HTXnLVmax_SHIFT));
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL0_5WE, tex->we);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL0_5HE, tex->he);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL012BasH,
        (src->front_buffer->video.offset >> 24) & 0xff);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL0BasL,
        src->front_buffer->video.offset & 0xffffff);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HTXnL0Pit,
        HC_HTXnEnPit_MASK | src->front_buffer->video.pitch);

    UC_FIFO_PAD_EVEN(fifo);

    UC_FIFO_CHECK(fifo);

    // Upload the palette of a 256 color texture.

    if (tex->format == HC_HTXnFM_Index8) {

        int i, n;
        DFBColor* c;

        UC_FIFO_PREPARE(fifo, 258);

        UC_FIFO_ADD_HDR(fifo, (HC_ParaType_Palette << 16)
            | (HC_SubType_TexPalette0 << 24));

        c = src->palette->entries;
        n = src->palette->num_entries;
        if (n > 256) n = 256;

        for (i = 0; i < n-1; i++) {
            UC_FIFO_ADD(fifo, PIXEL_ARGB(c[i].a, c[i].r, c[i].g, c[i].b));
        }
        for (; i < 255; i++) {
            UC_FIFO_ADD(fifo, 0);
        }

        UC_FIFO_CHECK(fifo);
    }
    
    ucdev->v_source3d = 1;
}

/// Set either destination color key, or fill color, as needed. (2D)
void uc_set_drawing_color_2d(struct uc_fifo* fifo, CardState* state,
                             UcDeviceData* ucdev)
{
    UC_FIFO_PREPARE(fifo, 8);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    // Opaque line drawing needs this
    UC_FIFO_ADD_2D(fifo, VIA_REG_MONOPAT0, 0xff);

    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
        UC_FIFO_ADD_2D(fifo, VIA_REG_KEYCONTROL,
            VIA_KEY_ENABLE_DSTKEY | VIA_KEY_INVERT_KEY);
        UC_FIFO_ADD_2D(fifo, VIA_REG_FGCOLOR, state->dst_colorkey);
    }
    else {
        UC_FIFO_ADD_2D(fifo, VIA_REG_KEYCONTROL, 0);
        UC_FIFO_ADD_2D(fifo, VIA_REG_FGCOLOR, ucdev->color);
    }

    UC_FIFO_CHECK(fifo);
}

void uc_set_blitting_colorkey_2d(struct uc_fifo* fifo, CardState* state,
                                 UcDeviceData* ucdev)
{
    UC_FIFO_PREPARE(fifo, 6);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
        UC_FIFO_ADD_2D(fifo, VIA_REG_KEYCONTROL, VIA_KEY_ENABLE_SRCKEY);
        UC_FIFO_ADD_2D(fifo, VIA_REG_BGCOLOR, state->src_colorkey);
    }
    else if (state->blittingflags & DSBLIT_DST_COLORKEY) {
        UC_FIFO_ADD_2D(fifo, VIA_REG_KEYCONTROL,
            VIA_KEY_ENABLE_DSTKEY | VIA_KEY_INVERT_KEY);
        UC_FIFO_ADD_2D(fifo, VIA_REG_FGCOLOR, state->dst_colorkey);
    }
    else {
        UC_FIFO_ADD_2D(fifo, VIA_REG_KEYCONTROL, 0);
        UC_FIFO_ADD_2D(fifo, VIA_REG_FGCOLOR, ucdev->color);
    }

    UC_FIFO_CHECK(fifo);
}
