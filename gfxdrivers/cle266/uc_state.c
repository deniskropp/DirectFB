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

static inline bool
uc_has_dst_format( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               return true;

          default:
               break;
     }

     return false;
}

static inline bool
uc_has_src_format_3d( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
          case DSPF_LUT8:
               return true;

          default:
               break;
     }

     return false;
}

static inline enum uc_state_type
uc_select_drawtype( CardState* state,
                    DFBAccelerationMask accel )
{
     if (!(state->drawingflags & ~UC_DRAWING_FLAGS_2D) &&
         !(accel & DFXL_FILLTRIANGLE))
          return UC_TYPE_2D;

     if (!(state->drawingflags & ~UC_DRAWING_FLAGS_3D))
          return UC_TYPE_3D;

     return UC_TYPE_UNSUPPORTED;
}

static inline enum uc_state_type
uc_select_blittype( CardState* state,
                    DFBAccelerationMask accel )
{
     if (!(state->blittingflags & ~UC_BLITTING_FLAGS_2D)) {
          if ((state->source->format == state->destination->format) &&
              !((state->blittingflags & DSBLIT_SRC_COLORKEY) &&
                (state->blittingflags & DSBLIT_DST_COLORKEY)) &&
              !(accel & DFXL_STRETCHBLIT))
               return UC_TYPE_2D;
     }

     if (!(state->blittingflags & ~UC_BLITTING_FLAGS_3D)) {
          if (uc_has_src_format_3d( state->source->format ))
               return UC_TYPE_3D;
     }

     return UC_TYPE_UNSUPPORTED;
}

// DirectFB interfacing functions --------------------------------------------

void uc_check_state(void *drv, void *dev,
                    CardState *state, DFBAccelerationMask accel)
{
     /* Check destination format. */
     if (!uc_has_dst_format( state->destination->format ))
          return;

     if (DFB_DRAWING_FUNCTION(accel)) {
          /* Check drawing parameters. */
          switch (uc_select_drawtype(state, accel)) {
               case UC_TYPE_2D:
                    state->accel |= UC_DRAWING_FUNCTIONS_2D;
                    break;
               case UC_TYPE_3D:
                    state->accel |= UC_DRAWING_FUNCTIONS_3D;
                    break;
               default:
                    return;
          }
     }
     else {
          /* Check blitting parameters. */
          switch (uc_select_blittype(state, accel)) {
               case UC_TYPE_2D:
                    state->accel |= UC_BLITTING_FUNCTIONS_2D;
                    break;
               case UC_TYPE_3D:
                    state->accel |= UC_BLITTING_FUNCTIONS_3D;
                    break;
               default:
                    return;
          }
     }
}

void uc_set_state(void *drv, void *dev, GraphicsDeviceFuncs *funcs,
                  CardState *state, DFBAccelerationMask accel)
{
     UcDriverData   *ucdrv = (UcDriverData*) drv;
     UcDeviceData   *ucdev = (UcDeviceData*) dev;
     struct uc_fifo *fifo  = ucdev->fifo;

     __u32 rop3d     = HC_HROP_P;
     __u32 regEnable = HC_HenCW_MASK;

     StateModificationFlags modified = state->modified;

     // Check modified states and update hw

     if (modified & SMF_SOURCE)
          UC_INVALIDATE( uc_source3d | uc_texenv | uc_source2d );
     else if (modified & SMF_BLITTING_FLAGS)
          UC_INVALIDATE( uc_source3d | uc_texenv );

     if (modified & (SMF_BLITTING_FLAGS | SMF_SRC_COLORKEY | SMF_DST_COLORKEY))
          UC_INVALIDATE( uc_colorkey2d );

     if (modified & (SMF_COLOR | SMF_DESTINATION | SMF_DRAWING_FLAGS))
          UC_INVALIDATE( uc_color2d );

     if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
          UC_INVALIDATE( uc_blending_fn );


     if (modified & SMF_COLOR)
          ucdev->color3d = PIXEL_ARGB( state->color.a, state->color.r,
                                       state->color.g, state->color.b );

     if (modified & SMF_DRAWING_FLAGS) {
          if (state->drawingflags & DSDRAW_XOR) {
               ucdev->draw_rop3d = HC_HROP_DPx;
               ucdev->draw_rop2d = VIA_ROP_DPx;
          }
          else {
               ucdev->draw_rop3d = HC_HROP_P;
               ucdev->draw_rop2d = VIA_ROP_P;
          }
     }

     ucdev->bflags = state->blittingflags;

     if (modified & SMF_DESTINATION)
          uc_set_destination(ucdrv, ucdev, state);

     if (modified & SMF_CLIP)
          uc_set_clip(ucdrv, ucdev, state);


     // Select GPU and check remaining states

     if (DFB_DRAWING_FUNCTION(accel)) {

          switch (uc_select_drawtype(state, accel)) {
               case UC_TYPE_2D:
                    funcs->FillRectangle = uc_fill_rectangle;
                    funcs->DrawRectangle = uc_draw_rectangle;
                    funcs->DrawLine = uc_draw_line;

                    uc_set_color_2d(ucdrv, ucdev, state);

                    state->set = UC_DRAWING_FUNCTIONS_2D;
                    break;

               case UC_TYPE_3D:
                    funcs->FillRectangle = uc_fill_rectangle_3d;
                    funcs->DrawRectangle = uc_draw_rectangle_3d;
                    funcs->DrawLine = uc_draw_line_3d;

                    if (state->drawingflags & DSDRAW_BLEND) {
                         uc_set_blending_fn(ucdrv, ucdev, state);
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
          switch (uc_select_blittype(state, accel)) {
               case UC_TYPE_2D:
                    uc_set_source_2d(ucdrv, ucdev, state);
                    funcs->Blit = uc_blit;

                    uc_set_colorkey_2d(ucdrv, ucdev, state);
                    state->set = UC_BLITTING_FUNCTIONS_2D;
                    break;

               case UC_TYPE_3D:
                    funcs->Blit = uc_blit_3d;
                    uc_set_source_3d(ucdrv, ucdev, state);
                    uc_set_texenv(ucdrv, ucdev, state);
                    uc_set_blending_fn(ucdrv, ucdev, state);

                    regEnable |= HC_HenTXMP_MASK | HC_HenTXCH_MASK | HC_HenTXPP_MASK;

                    if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                DSBLIT_BLEND_COLORALPHA))
                         regEnable |= HC_HenABL_MASK;

                    state->set = UC_BLITTING_FUNCTIONS_3D;
                    break;

               case UC_TYPE_UNSUPPORTED:
                    BUG("Unsupported drawing function!");
                    break;
          }
     }

#ifdef UC_ENABLE_3D
     UC_FIFO_PREPARE( fifo, 6 );
     UC_FIFO_ADD_HDR( fifo, HC_ParaType_NotTex << 16 );

     /* Don't know what this does. DRI code always clears it. */
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HPixGC,   0 );

     UC_FIFO_ADD_3D ( fifo, HC_SubA_HEnable,  regEnable );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HFBBMSKL, 0xffffff );
     UC_FIFO_ADD_3D ( fifo, HC_SubA_HROP,     rop3d | 0xff );
#endif

     UC_FIFO_CHECK(fifo);

     state->modified = 0;
}

