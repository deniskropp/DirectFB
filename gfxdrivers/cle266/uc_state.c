/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include <gfx/convert.h>
#include "unichrome.h"
#include "uc_state.h"
#include "uc_accel.h"
#include "uc_hw.h"

enum uc_state_type {
    UC_TYPE_UNSUPPORTED,
    UC_TYPE_2D,
    UC_TYPE_3D
};

/// GPU selecting functions --------------------------------------------------

inline bool uc_is_destination_supported(DFBSurfacePixelFormat format)
{
    switch (format)
    {
        //case DSPF_RGB15:
    case DSPF_RGB16:
    case DSPF_RGB32:
    case DSPF_LUT8:
    case DSPF_ARGB:
    case DSPF_ARGB1555:
        return true;
    default:
        return false;
    }
}

inline enum uc_state_type uc_select_drawtype(CardState* state,
                                             DFBAccelerationMask accel)
{
    if (!(state->drawingflags & ~UC_DRAWING_FLAGS_2D) &&
        !(accel & DFXL_FILLTRIANGLE)) return UC_TYPE_2D;

    if (!(state->drawingflags & ~UC_DRAWING_FLAGS_3D)) return UC_TYPE_3D;

    return UC_TYPE_UNSUPPORTED;
}

inline enum uc_state_type uc_select_blittype(CardState* state,
                                             DFBAccelerationMask accel)
{
    __u32 tmp;

    if (!(state->blittingflags & ~UC_BLITTING_FLAGS_2D))
    {
        if ((state->source->format == state->destination->format) &&
            !((state->blittingflags & DSBLIT_SRC_COLORKEY) &&
            (state->blittingflags & DSBLIT_DST_COLORKEY)) &&
            !(accel & DFXL_STRETCHBLIT)) return UC_TYPE_2D;
    }

// 3d blitting is broken

    if (!(state->blittingflags & ~UC_BLITTING_FLAGS_3D)) {
        if ((uc_map_src_format_3d(state->source->format) >= 0) &&
            (uc_map_dst_format(state->destination->format, &tmp, &tmp) >= 0))
            return UC_TYPE_3D;
    }

    return UC_TYPE_UNSUPPORTED;
}

// DirectFB interfacing functions --------------------------------------------

void uc_check_state(void *drv, void *dev,
                    CardState *state, DFBAccelerationMask accel)
{
    if (!uc_is_destination_supported(state->destination->format)) return;


    if (DFB_DRAWING_FUNCTION(accel)) {

        switch (uc_select_drawtype(state, accel))
        {
        case UC_TYPE_2D:
            state->accel |= UC_DRAWING_FUNCTIONS_2D;
            break;
        case UC_TYPE_3D:
            state->accel |= UC_DRAWING_FUNCTIONS_3D;
            break;
        case UC_TYPE_UNSUPPORTED:
            break;
        }
        return;
    }
    else { // DFB_BLITTING_FUNCTION(accel)

        switch (uc_select_blittype(state, accel))
        {
        case UC_TYPE_2D:
            state->accel |= UC_BLITTING_FUNCTIONS_2D;
            break;
        case UC_TYPE_3D:
            state->accel |= UC_BLITTING_FUNCTIONS_3D;
            break;
        case UC_TYPE_UNSUPPORTED:
            break;
        }
        return;
    }
}

void uc_set_state(void *drv, void *dev, GraphicsDeviceFuncs *funcs,
                  CardState *state, DFBAccelerationMask accel)
{
    UcDriverData* ucdrv = (UcDriverData*) drv;
    UcDeviceData* ucdev = (UcDeviceData*) dev;
    struct uc_fifo* fifo = ucdrv->fifo;

    __u32 rop3d = HC_HROP_P;
    __u32 mask3d;
    __u32 regEnable = HC_HenCW_MASK;

    // Check modified states and update hw

    if (state->modified & SMF_SOURCE) {
        ucdev->v_source2d = 0;
        ucdev->v_source3d = 0;
    }

    if (state->modified & (SMF_COLOR | SMF_DESTINATION)) {
        ucdev->color = uc_map_color(state->destination->format,
            state->color);
        
        ucdev->color3d = PIXEL_ARGB( state->color.a , state->color.r,
                                     state->color.g , state->color.b );    
    }

    if (state->modified & SMF_DRAWING_FLAGS)
    {
        if (state->drawingflags & DSDRAW_XOR) {
            ucdev->draw_rop3d = HC_HROP_DPx;
            ucdev->draw_rop2d = VIA_ROP_DPx;
        }
        else {
            ucdev->draw_rop3d = HC_HROP_P;
            ucdev->draw_rop2d = VIA_ROP_P;
        }
    }

    if (state->modified & SMF_CLIP) {
        uc_set_clip(fifo, state);
    }

    if (state->modified & SMF_DESTINATION) {
        uc_set_destination(fifo, ucdev, state);
    }

    // if (state->modified & SMF_SRC_COLORKEY) { }
    // if (state->modified & SMF_DST_COLORKEY) { }

    if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND)) {
        uc_map_blending_fn(&(ucdev->hwalpha), state->src_blend,
            state->dst_blend, DFB_BITS_PER_PIXEL(state->destination->format));
        uc_set_blending_fn(fifo, ucdev);
    }

    // Select GPU and check remaining states

    if (DFB_DRAWING_FUNCTION(accel)) {

        switch (uc_select_drawtype(state, accel))
        {
        case UC_TYPE_2D:
            funcs->FillRectangle = uc_fill_rectangle;
            funcs->DrawRectangle = uc_draw_rectangle;
            funcs->DrawLine = uc_draw_line;

            uc_set_drawing_color_2d(fifo, state, ucdev);

            state->set = UC_DRAWING_FUNCTIONS_2D;
            break;

        case UC_TYPE_3D:
            funcs->FillRectangle = uc_fill_rectangle_3d;
            funcs->DrawRectangle = uc_draw_rectangle_3d;
            funcs->DrawLine = uc_draw_line_3d;

            if (state->drawingflags & DSDRAW_BLEND) {
                regEnable |= HC_HenABL_MASK;
            }

            rop3d = ucdev->draw_rop3d;

            state->set = UC_DRAWING_FUNCTIONS_3D;
            break;

        case UC_TYPE_UNSUPPORTED:
            BUG("Unsupported drawing function!");
            break;
        }
    }
    else { // DFB_BLITTING_FUNCTION(accel)
        switch (uc_select_blittype(state, accel))
        {
        case UC_TYPE_2D:
            uc_set_source_2d(fifo, ucdev, state);
            funcs->Blit = uc_blit;

            uc_set_blitting_colorkey_2d(fifo, state, ucdev);
            state->set = UC_BLITTING_FUNCTIONS_2D;
            break;

        case UC_TYPE_3D:
            uc_set_source_3d(fifo, ucdev, state);
            funcs->Blit = uc_blit_3d;

            if (state->modified & SMF_BLITTING_FLAGS) {
                uc_map_blitflags(&(ucdev->hwtex), state->blittingflags,
                    state->source->format);
                uc_set_texenv(fifo, ucdev);
            }

            if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                DSBLIT_BLEND_COLORALPHA)) {
                    regEnable |= HC_HenABL_MASK;
                }
                regEnable |=
                    HC_HenTXMP_MASK | HC_HenTXCH_MASK | HC_HenTXPP_MASK;
                state->set = UC_BLITTING_FUNCTIONS_3D;
                break;

        case UC_TYPE_UNSUPPORTED:
            BUG("Unsupported drawing function!");
            break;
        }
    }

    mask3d = ucdev->colormask | ucdev->alphamask;

    UC_FIFO_PREPARE(fifo, 6);
    UC_FIFO_ADD_HDR(fifo, HC_ParaType_NotTex << 16);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HEnable, regEnable);

    UC_FIFO_ADD_3D(fifo, HC_SubA_HFBBMSKL, mask3d & 0xffffff);
    UC_FIFO_ADD_3D(fifo, HC_SubA_HROP, rop3d | ((mask3d >> 24) & 0xff));
    UC_FIFO_ADD_3D(fifo, HC_SubA_HPixGC, 0);  // Don't know what this does...
                                              // ...DRI code always clears it.
    UC_FIFO_PAD_EVEN(fifo);

    UC_FIFO_CHECK(fifo);
    UC_FIFO_FLUSH(fifo);

    state->modified = 0;
}
