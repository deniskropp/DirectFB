/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
	      Michel Dänzer <michel@daenzer.net>.

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

#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon.h"

#include "radeon_state.h"


void radeon_set_destination( RADEONDriverData *adrv,
                             RADEONDeviceData *adev,
                             CardState        *state )
{
    if ( adev->v_destination )
	return;

    switch ( state->destination->format ) {
    case DSPF_RGB332:		
	adev->RADEON_dp_gui_master_cntl = GMC_DST_8BPP;
	break;
    case DSPF_ARGB1555:		
	adev->RADEON_dp_gui_master_cntl = GMC_DST_15BPP;
	break;
    case DSPF_RGB16:
	adev->RADEON_dp_gui_master_cntl = GMC_DST_16BPP;
	break;
    case DSPF_RGB32:
    case DSPF_ARGB:
	adev->RADEON_dp_gui_master_cntl = GMC_DST_32BPP;
	break;
    default:
	D_BUG( "unexpected pixelformat!" );
	break;
    }

    adev->RADEON_dp_gui_master_cntl |= GMC_WRITE_MASK_DIS
				     | GMC_SRC_PITCH_OFFSET_NODEFAULT
				     | GMC_DST_PITCH_OFFSET_NODEFAULT
				     | GMC_DST_CLIP_NODEFAULT;

    radeon_waitfifo( adrv, adev, 2 );

    radeon_out32( adrv->mmio_base, DST_PITCH,
		  state->destination->back_buffer->video.pitch );

    radeon_out32( adrv->mmio_base, DST_OFFSET,
		  state->destination->back_buffer->video.offset );

    adev->destination = state->destination;
    adev->v_destination = 1;
}

void radeon_set_source( RADEONDriverData *adrv,
                        RADEONDeviceData *adev,
                        CardState        *state )
{
    if ( adev->v_source )
	return;

    radeon_waitfifo( adrv, adev, 3 );

    switch ( state->source->format ) {
    case DSPF_RGB332:
	radeon_out32( adrv->mmio_base, CLR_CMP_MASK, 0x000000FF );
	break;
    case DSPF_ARGB1555:
	radeon_out32( adrv->mmio_base, CLR_CMP_MASK, 0x00007FFF );
	break;
    case DSPF_RGB16:
	radeon_out32( adrv->mmio_base, CLR_CMP_MASK, 0x0000FFFF );
	break;
    case DSPF_RGB32:
    case DSPF_ARGB:
	radeon_out32( adrv->mmio_base, CLR_CMP_MASK, 0xFFFFFFFF );
	break;
    default:
	D_BUG( "unexpected pixelformat!" );
	break;
    }

    radeon_out32( adrv->mmio_base, SRC_PITCH,
		  state->source->front_buffer->video.pitch );

    radeon_out32( adrv->mmio_base, SRC_OFFSET,
		   state->source->front_buffer->video.offset );

    adev->source = state->source;
    adev->v_source = 1;
}

void radeon_set_clip( RADEONDriverData *adrv,
                      RADEONDeviceData *adev,
                      CardState        *state )
{
    radeon_waitfifo( adrv, adev, 2 );

    radeon_out32( adrv->mmio_base, SC_TOP_LEFT,
		  ( state->clip.y1 << 16 ) | state->clip.x1 );

    radeon_out32( adrv->mmio_base, SC_BOTTOM_RIGHT,
		  ( ( state->clip.y2 + 1 ) << 16) | ( state->clip.x2 + 1 ) );
}

void radeon_set_color( RADEONDriverData *adrv,
                       RADEONDeviceData *adev,
                       CardState        *state )
{
    __u32 fill_color    = 0;

    if ( adev->v_color )
	return;

    switch ( state->destination->format ) {
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

    radeon_waitfifo( adrv, adev, 3 );

    radeon_out32( adrv->mmio_base, DP_BRUSH_FRGD_CLR, fill_color);

    radeon_out32( adrv->mmio_base, DP_GUI_MASTER_CNTL, adev->RADEON_dp_gui_master_cntl
						     | GMC_SRC_FG
						     | GMC_BRUSH_SOLIDCOLOR
						     | GMC_ROP3_PATCOPY
						     | GMC_DP_SRC_RECT
						     | GMC_DST_CLR_CMP_FCN_CLEAR );

    radeon_out32( adrv->mmio_base, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );

    adev->v_color = 1;
    adev->v_blittingflags = 0;
}

void radeon_set_src_colorkey( RADEONDriverData *adrv,
                              RADEONDeviceData *adev,
                              CardState        *state )
{
    if ( adev->v_src_colorkey )
	return;

    radeon_waitfifo( adrv, adev, 1 );
    radeon_out32( adrv->mmio_base, CLR_CMP_CLR_SRC, state->src_colorkey );

    adev->v_src_colorkey = 1;
}

void radeon_set_blittingflags( RADEONDriverData *adrv,
                               RADEONDeviceData *adev,
                               CardState        *state )
{
    if ( adev->v_blittingflags )
	return;

    radeon_waitfifo( adrv, adev, 2 );

    if ( state->blittingflags & DSBLIT_SRC_COLORKEY ) {
	radeon_out32( adrv->mmio_base, CLR_CMP_CNTL, ( 1 << 24 ) | 4 );
    } else {
	radeon_out32( adrv->mmio_base, CLR_CMP_CNTL, 0 );
    }

    radeon_out32( adrv->mmio_base, DP_GUI_MASTER_CNTL, adev->RADEON_dp_gui_master_cntl
						     | GMC_BRUSH_NONE
						     | GMC_SRC_DSTCOLOR
						     | GMC_ROP3_SRCCOPY
						     | GMC_DP_SRC_RECT );

    adev->blittingflags = state->blittingflags;
    adev->v_blittingflags = 1;
    adev->v_color = 0;
}
