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

#include "regs.h"
#include "mmio.h"
#include "ati128.h"

#include "ati128_state.h"


static __u32 ati128SourceBlend[] = {
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

static __u32 ati128DestBlend[] = {
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

     ati128_waitfifo( adrv, adev, 1 );

     switch (destination->format) {
          case DSPF_RGB15:		
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((destination->back_buffer->video.pitch >> 4) << 21) |
                             (destination->back_buffer->video.offset >> 5));               

               adev->ATI_dst_bpp = DST_15BPP;
               break;          
          case DSPF_RGB16:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((destination->back_buffer->video.pitch >> 4) << 21) |
                             (destination->back_buffer->video.offset >> 5));               

               adev->ATI_dst_bpp = DST_16BPP;
               break;
          case DSPF_RGB24:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((destination->back_buffer->video.pitch >> 3) << 21) |
                             (destination->back_buffer->video.offset >> 5));

               adev->ATI_dst_bpp = DST_24BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               ati128_out32( adrv->mmio_base, DST_PITCH_OFFSET,
                             ((destination->back_buffer->video.pitch >> 5) << 21) |
                             (destination->back_buffer->video.offset >> 5));

               adev->ATI_dst_bpp = DST_32BPP;
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }
     adev->destination = destination;
     state->modified &= ~SMF_DESTINATION;
}

void ati128_set_source( ATI128DriverData *adrv,
                        ATI128DeviceData *adev,
                        CardState        *state )
{
     ati128_waitfifo( adrv, adev, 3 );
     
     switch (state->source->format) {
          case DSPF_RGB15:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->source->front_buffer->video.pitch >>4);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00007FFF );
               break;
          case DSPF_RGB16:
               ati128_out32( adrv->mmio_base, SRC_PITCH,
                             state->source->front_buffer->video.pitch >>4);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x0000FFFF );
               break;
          case DSPF_RGB24:
               ati128_out32( adrv->mmio_base, SRC_PITCH, 
                             state->source->front_buffer->video.pitch >>3);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00FFFFFF );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:                             
               ati128_out32( adrv->mmio_base, SRC_PITCH, 
                             state->source->front_buffer->video.pitch >>5);

               ati128_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00FFFFFF );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }
     
     ati128_out32( adrv->mmio_base, SRC_OFFSET,
                   state->source->front_buffer->video.offset );

     adev->source = state->source;
     state->modified &= ~SMF_SOURCE;
}

void ati128_set_clip( ATI128DriverData *adrv,
                      ATI128DeviceData *adev,
                      CardState        *state )
{     
     ati128_waitfifo( adrv, adev, 2 );

     /* 24bpp needs special treatment */
     if (state->destination->format == DSPF_RGB24) {
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

     state->modified &= ~SMF_CLIP;
}

void ati128_set_color( ATI128DriverData *adrv,
                       ATI128DeviceData *adev,
                       CardState        *state )
{
     __u32 fill_color = 0; 
     
     switch (state->destination->format) {
          case DSPF_RGB15:
               fill_color = PIXEL_RGB15( state->color.r,
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
               BUG( "unexpected pixelformat!" );
               break;
     }
     
     ati128_waitfifo( adrv, adev, 1 );
     ati128_out32( adrv->mmio_base, DP_BRUSH_FRGD_CLR, fill_color);
     
     
     adev->fake_texture_color = PIXEL_ARGB( state->color.a,
                                            state->color.r,
                                            state->color.g,
                                            state->color.b );
     
     state->modified &= ~SMF_COLOR;
}

void ati128_set_blittingflags( ATI128DriverData *adrv,
                               ATI128DeviceData *adev,
                               CardState        *state )
{     
     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {     
          adev->ATI_color_compare = (1 << 24) | 5;
     }
     else {
          adev->ATI_color_compare = 0;
     }
                            
     adev->blittingflags = state->blittingflags;
     state->modified &= ~SMF_BLITTING_FLAGS;
}

void ati128_set_blending_function( ATI128DriverData *adrv,
                                   ATI128DeviceData *adev,
                                   CardState        *state )
{     
     adev->ATI_blend_function = SCALE_3D_CNTL_SCALE_3D_FN_SCALE |
                                ati128SourceBlend[state->src_blend - 1] |
                                ati128DestBlend  [state->dst_blend - 1] |
                                SCALE_3D_CNTL_TEX_MAP_AEN_ON;
     
     state->modified &= ~(SMF_DST_BLEND | SMF_SRC_BLEND);
}

