/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <core/surface.h>

#include <gfx/convert.h>

#include "regs.h"
#include "mmio.h"
#include "ati128.h"

#include "ati128_state.h"


static u32 ati128SourceBlend[] = {
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_ZERO,        /* DSBF_ZERO         */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_ONE,         /* DSBF_ONE          */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_SRCCOLOR,    /* DSBF_SRCCOLOR     */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVSRCCOLOR, /* DSBF_INVSRCCOLOR  */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_SRCALPHA,    /* DSBF_SRCALPHA     */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVSRCALPHA, /* DSBF_INVSRCALPHA  */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_DSTALPHA,    /* DSBF_DESTALPHA    */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVDSTALPHA, /* DSBF_INVDESTALPHA */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_DSTCOLOR,    /* DSBF_DESTCOLOR    */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVDSTCOLOR, /* DSBF_INVDESTCOLOR */
     SCALE_3D_CNTL_ALPHA_BLEND_SRC_SAT          /* DSBF_SRCALPHASAT  */
};

static u32 ati128DestBlend[] = {
     SCALE_3D_CNTL_ALPHA_BLEND_DST_ZERO,        /* DSBF_ZERO         */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_ONE,         /* DSBF_ONE          */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_SRCCOLOR,    /* DSBF_SRCCOLOR     */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_INVSRCCOLOR, /* DSBF_INVSRCCOLOR  */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_SRCALPHA,    /* DSBF_SRCALPHA     */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_INVSRCALPHA, /* DSBF_INVSRCALPHA  */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_DSTALPHA,    /* DSBF_DESTALPHA    */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_INVDSTALPHA, /* DSBF_INVDESTALPHA */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_DSTCOLOR,    /* DSBF_DESTCOLOR    */
     SCALE_3D_CNTL_ALPHA_BLEND_DST_INVDSTCOLOR, /* DSBF_INVDESTCOLOR */
     0                                          /* DSBF_SRCALPHASAT  */
};

void ati128_set_destination( ATI128DriverData *adrv,
                             ATI128DeviceData *adev,
                             CardState        *state )
{
     CoreSurface *destination = state->destination;

     if (adev->v_destination)
          return;

     ati128_waitfifo( adrv, adev, 1 );

     switch (destination->config.format) {
          case DSPF_RGB332:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((state->dst.pitch >> 3) << 21) |
                             (state->dst.offset >> 5));

               adev->ATI_dst_bpp = DST_8BPP_RGB332;
               break;
          case DSPF_ARGB1555:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((state->dst.pitch >> 4) << 21) |
                             (state->dst.offset >> 5));

               adev->ATI_dst_bpp = DST_15BPP;
               break;
          case DSPF_RGB16:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((state->dst.pitch >> 4) << 21) |
                             (state->dst.offset >> 5));

               adev->ATI_dst_bpp = DST_16BPP;
               break;
          case DSPF_RGB24:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((state->dst.pitch >> 3) << 21) |
                             (state->dst.offset >> 5));

               adev->ATI_dst_bpp = DST_24BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((state->dst.pitch >> 5) << 21) |
                             (state->dst.offset >> 5));

               adev->ATI_dst_bpp = DST_32BPP;
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }
     adev->destination = destination;

     adev->v_destination = 1;
}

void ati128_set_source( ATI128DriverData *adrv,
                        ATI128DeviceData *adev,
                        CardState        *state )
{

     if (adev->v_source)
          return;

     ati128_waitfifo( adrv, adev, 3 );

     switch (state->source->config.format) {
          case DSPF_RGB332:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->src.pitch >> 3);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x000000FF );
               break;
          case DSPF_ARGB1555:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->src.pitch >> 4);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00007FFF );
               break;
          case DSPF_RGB16:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->src.pitch >> 4);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x0000FFFF );
               break;
          case DSPF_RGB24:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->src.pitch >> 3);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00FFFFFF );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->src.pitch >> 5);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00FFFFFF );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     ati128_out32( adrv->mmio_base, SRC_OFFSET,
                   state->src.offset );

     adev->source = state->source;
     adev->src = &state->src;
     adev->v_source = 1;
}

void ati128_set_clip( ATI128DriverData *adrv,
                      ATI128DeviceData *adev,
                      CardState        *state )
{

     ati128_waitfifo( adrv, adev, 2 );

     /* 24bpp needs special treatment */
     if (state->destination->config.format == DSPF_RGB24) {
          ati128_out32( adrv->mmio_base, SC_TOP_LEFT,
                        (state->clip.y1 << 16) | (state->clip.x1*3) );

          ati128_out32( adrv->mmio_base, SC_BOTTOM_RIGHT,
                        (state->clip.y2 << 16) | ((state->clip.x2*3) + 3));
     }
     else {
          ati128_out32( adrv->mmio_base, SC_TOP_LEFT,
                        (state->clip.y1 << 16) | state->clip.x1 );

          ati128_out32( adrv->mmio_base, SC_BOTTOM_RIGHT,
                        (state->clip.y2 << 16) | state->clip.x2 );
     }
}

void ati128_set_color( ATI128DriverData *adrv,
                       ATI128DeviceData *adev,
                       CardState        *state )
{
     u32 fill_color = 0;

     if (adev->v_color)
          return;

     switch (state->destination->config.format) {
          case DSPF_RGB332:
               fill_color = PIXEL_RGB332( state->color.r,
                                          state->color.g,
                                          state->color.b );
               break;
          case DSPF_ARGB1555:
               fill_color = PIXEL_ARGB1555( state->color.a,
                                            state->color.r,
                                            state->color.g,
                                            state->color.b );
               break;
          case DSPF_RGB16:
               fill_color = PIXEL_RGB16( state->color.r,
                                         state->color.g,
                                         state->color.b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               fill_color = PIXEL_RGB32( state->color.r,
                                         state->color.g,
                                         state->color.b );
               break;
          case DSPF_ARGB:
               fill_color = PIXEL_ARGB( state->color.a,
                                        state->color.r,
                                        state->color.g,
                                        state->color.b );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     ati128_waitfifo( adrv, adev, 1 );
     ati128_out32( adrv->mmio_base, DP_BRUSH_FRGD_CLR, fill_color);


     adev->fake_texture_color = PIXEL_ARGB( state->color.a,
                                            state->color.r,
                                            state->color.g,
                                            state->color.b );

     adev->v_color = 1;
}

void ati128_set_src_colorkey( ATI128DriverData *adrv,
                              ATI128DeviceData *adev,
                              CardState        *state )
{
     if (adev->v_src_colorkey)
          return;

     ati128_waitfifo( adrv, adev, 1 );
     ati128_out32( adrv->mmio_base, CLR_CMP_CLR_SRC, state->src_colorkey );

     adev->v_src_colorkey = 1;
}

void ati128_set_blittingflags( ATI128DriverData *adrv,
                               ATI128DeviceData *adev,
                               CardState        *state )
{
     if (adev->v_blittingflags)
          return;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          adev->ATI_color_compare = (1 << 24) | 5;
     }
     else {
          adev->ATI_color_compare = 0;
     }

     adev->blittingflags = state->blittingflags;
     adev->v_blittingflags = 1;
}

void ati128_set_blending_function( ATI128DriverData *adrv,
                                   ATI128DeviceData *adev,
                                   CardState        *state )
{
     if (adev->v_blending_function)
          return;

     adev->ATI_blend_function = SCALE_3D_CNTL_SCALE_3D_FN_SCALE |
                                ati128SourceBlend[state->src_blend - 1] |
                                ati128DestBlend  [state->dst_blend - 1] |
                                SCALE_3D_CNTL_TEX_MAP_AEN_ON;

     adev->v_blending_function = 1;
}
