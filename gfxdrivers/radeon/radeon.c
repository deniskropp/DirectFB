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

#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <core/fbdev/fbdev.h>

#include <gfx/convert.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( radeon )


#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon_state.h"
#include "radeon.h"


/* driver capability flags */

#define RADEON_SUPPORTED_DRAWINGFLAGS \
	( DSDRAW_NOFX )

#define RADEON_SUPPORTED_DRAWINGFUNCTIONS \
	( DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE )

#define RADEON_SUPPORTED_BLITTINGFLAGS \
	( DSBLIT_SRC_COLORKEY )

#define RADEON_SUPPORTED_BLITTINGFUNCTIONS \
	( DFXL_BLIT )


/* macro for S14.0 format */
#define S14( val )	( ( ( __u16 )( ( __s16 )( val ) ) ) &0x3fff )


/** CARD FUNCTIONS **/
static bool radeonFillRectangle( void *drv, void *dev, DFBRectangle *rect );
static bool radeonDrawRectangle( void *drv, void *dev, DFBRectangle *rect );


/* required implementations */

static void radeonEngineSync( void *drv, void *dev )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) drv;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) dev;

    radeon_waitidle( adrv, adev );
}

static void radeonCheckState( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel )
{
    switch ( state->destination->format ) {
    case DSPF_RGB332:
    case DSPF_ARGB1555:
    case DSPF_RGB16:
    case DSPF_RGB32:
    case DSPF_ARGB:
	break;
    default:
	return;
    }

    /* check for the special drawing function that does not support
       the usually supported drawingflags */
    if ( accel == DFXL_DRAWLINE && state->drawingflags != DSDRAW_NOFX )
	return;

    /* if there are no other drawing flags than the supported */
    if ( !( accel & ~RADEON_SUPPORTED_DRAWINGFUNCTIONS )
      && !( state->drawingflags & ~RADEON_SUPPORTED_DRAWINGFLAGS ) )
	state->accel |= RADEON_SUPPORTED_DRAWINGFUNCTIONS;

    /* if there are no other blitting flags than the supported
       and the source has the minimum size */
    if ( !( accel & ~RADEON_SUPPORTED_BLITTINGFUNCTIONS )
      && !( state->blittingflags & ~RADEON_SUPPORTED_BLITTINGFLAGS )
      && state->source && state->source->format == state->destination->format
      && state->source->width >= 8 && state->source->height >= 8 )
    {
	switch ( state->source->format ) {
	case DSPF_RGB332:
	case DSPF_ARGB1555:
	case DSPF_RGB16:
	case DSPF_RGB32:
	case DSPF_ARGB:
	    state->accel |= RADEON_SUPPORTED_BLITTINGFUNCTIONS;
	default:
	    ;
	}
    }
}


static void radeonSetState( void *drv, void *dev,
                            GraphicsDeviceFuncs *funcs,
                            CardState *state, DFBAccelerationMask accel )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) drv;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) dev;

    if ( state->modified & SMF_SOURCE )
	adev->v_source = 0;

    if ( state->modified & SMF_DESTINATION )
	adev->v_destination = adev->v_color = 0;

    if ( state->modified & SMF_COLOR )
	adev->v_color = 0;

    if ( state->modified & SMF_SRC_COLORKEY )
	adev->v_src_colorkey = 0;

    if ( state->modified & SMF_BLITTING_FLAGS )
	adev->v_blittingflags = 0;

    radeon_set_destination( adrv, adev, state );

    switch ( accel ) {
    case DFXL_FILLRECTANGLE:
    case DFXL_DRAWRECTANGLE:
    case DFXL_DRAWLINE:
	radeon_set_color( adrv, adev, state );
	state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWLINE | DFXL_DRAWRECTANGLE ;
	break;

    case DFXL_BLIT:
    case DFXL_STRETCHBLIT:
	radeon_set_source( adrv, adev, state );
	if ( state->blittingflags & DSBLIT_SRC_COLORKEY )
	    radeon_set_src_colorkey( adrv, adev, state );
	radeon_set_blittingflags( adrv, adev, state );
	state->set |= DFXL_BLIT | DFXL_STRETCHBLIT;
	break;

    default:
	BUG( "unexpected drawing/blitting function" );
	break;
    }

    if ( state->modified & SMF_CLIP )
	radeon_set_clip( adrv, adev, state );

    state->modified = 0;
}


/* acceleration functions */

static bool radeonFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) drv;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) dev;
    volatile __u8    *mmio = adrv->mmio_base;

    radeon_waitfifo( adrv, adev, 2 );

    /* set coordinates */
    radeon_out32( mmio, DST_Y_X, ( rect->y << 16 ) | S14( rect->x ) );
    /* this executes the drawing command */
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( rect->h << 16 ) | S14( rect->w ) );

    return true;
}

static bool radeonDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) drv;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) dev;
    volatile __u8    *mmio = adrv->mmio_base;

    radeon_waitfifo( adrv, adev, 7 );

    /* first line */
    radeon_out32( mmio, DST_Y_X, ( rect->y << 16 ) | S14( rect->x ) );
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( rect->h << 16 ) | 1 );

    /* second line */
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( 1 << 16 ) | rect->w );

    /* third line */
    radeon_out32( mmio, DST_Y_X, ( ( rect->y+rect->h-1 ) << 16 ) | S14( rect->x ) );
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( 1 << 16 ) | rect->w );

    /* fourth line */
    radeon_out32( mmio, DST_Y_X, ( rect->y << 16 ) | S14( rect->x+rect->w-1 ) );
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( rect->h << 16 ) | 1 );

    return true;
}

static bool radeonDrawLine( void *drv, void *dev, DFBRegion *line )
{
     RADEONDriverData *adrv = (RADEONDriverData*) drv;
     RADEONDeviceData *adev = (RADEONDeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     radeon_waitfifo( adrv, adev, 2 );

     radeon_out32( mmio, DST_LINE_START, ( line->y1 << 16 ) | line->x1 );
     radeon_out32( mmio, DST_LINE_END,   ( line->y2 << 16 ) | line->x2 );

     return true;
}

static bool radeonBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) drv;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) dev;
    volatile __u8    *mmio = adrv->mmio_base;

    __u32 dir_cmd = 0;

    if ( adev->source->format != adev->destination->format ) {
	BUG( "blitting source/destination format mismatch" );
    }

    /* check which blitting direction should be used */
    if ( rect->x <= dx ) {
	dir_cmd |= DST_X_RIGHT_TO_LEFT;
	rect->x += rect->w-1;
	dx += rect->w-1;
    } else {
	dir_cmd |= DST_X_LEFT_TO_RIGHT;
    }

    if ( rect->y <= dy ) {
	dir_cmd |= DST_Y_BOTTOM_TO_TOP;
	rect->y += rect->h-1;
	dy += rect->h-1;
    } else {
	dir_cmd |= DST_Y_TOP_TO_BOTTOM;
    }

    radeon_waitfifo( adrv, adev, 4 );

    /* set blitting direction */
    radeon_out32( mmio, DP_CNTL, dir_cmd );

    /* set coordinates and initiate blit */
    radeon_out32( mmio, SRC_Y_X, ( rect->y << 16 ) | S14( rect->x ) );
    radeon_out32( mmio, DST_Y_X, ( dy << 16 ) | S14( dx ) );
    radeon_out32( mmio, DST_HEIGHT_WIDTH, ( rect->h << 16 ) | S14( rect->w ) );

    return true;
}


static DFBResult
radeonWaitVSync( CoreLayer *layer,
		 void      *driver_data,
		 void      *layer_data )
{
    RADEONDriverData	*rdrv	= ( RADEONDriverData* ) driver_data;
    int			i;

    if ( dfb_config->pollvsync_none )
	return DFB_OK;

    /* Clear the CRTC_VBLANK_SAVE bit */
    radeon_out32( rdrv->mmio_base, CRTC_STATUS, CRTC_VBLANK_SAVE_CLEAR );

    /* Wait for it to go back up */
    for ( i = 0; i < 1000; i++ ) {
	if ( radeon_in32( rdrv->mmio_base, CRTC_STATUS ) & CRTC_VBLANK_SAVE )
	    break;
	usleep(1);
    }

    return DFB_OK;
}

DisplayLayerFuncs radeonPrimaryFuncs = {
     WaitVSync:          radeonWaitVSync
};


/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
    switch ( dfb_gfxcard_get_accelerator( device ) ) {
    case FB_ACCEL_ATI_RADEON:          /* ATI Radeon */
	return 1;
    }

    return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
    /* fill driver info structure */
    snprintf( info->name,
	      DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
	      "ATI Radeon Driver" );

    snprintf( info->vendor,
	      DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
	      "Michel Dänzer" );

    info->version.major = 0;
    info->version.minor = 1;

    info->driver_data_size = sizeof (RADEONDriverData);
    info->device_data_size = sizeof (RADEONDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) driver_data;
#ifdef FBIO_WAITFORVSYNC
    static const int zero = 0;
    FBDev *dfb_fbdev = dfb_system_data();
#endif

    /* gain access to memory mapped registers */
    adrv->mmio_base = ( volatile __u8* ) dfb_gfxcard_map_mmio( device, 0, -1 );
    if (!adrv->mmio_base)
	return DFB_IO;

    /* fill acceleration function table */
    funcs->CheckState		= radeonCheckState;
    funcs->SetState		= radeonSetState;
    funcs->EngineSync		= radeonEngineSync;

    funcs->FillRectangle	= radeonFillRectangle;
    funcs->DrawRectangle	= radeonDrawRectangle;
    funcs->DrawLine		= radeonDrawLine;
    funcs->Blit			= radeonBlit;

    /* provide our own WaitVSync function if the ioctl for it isn't available
     * in order to avoid the non-portable VGA ports
     */
#ifdef FBIO_WAITFORVSYNC
    if ( ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, &zero ) )
#endif
	dfb_layers_hook_primary( device, driver_data, &radeonPrimaryFuncs, NULL, NULL );

    return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) driver_data;
    RADEONDeviceData *adev = ( RADEONDeviceData* ) device_data;
    volatile __u8    *mmio = adrv->mmio_base;

    /* fill device info */
    snprintf( device_info->name,
	      DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Radeon" );

    snprintf( device_info->vendor,
	      DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

    device_info->caps.flags    = CCF_CLIPPING;
    device_info->caps.accel    = RADEON_SUPPORTED_DRAWINGFUNCTIONS
			       | RADEON_SUPPORTED_BLITTINGFUNCTIONS;
    device_info->caps.drawing  = RADEON_SUPPORTED_DRAWINGFLAGS;
    device_info->caps.blitting = RADEON_SUPPORTED_BLITTINGFLAGS;

    device_info->limits.surface_byteoffset_alignment = 32 * 4;
    device_info->limits.surface_pixelpitch_alignment = 32;

    dfb_config->pollvsync_after = 1;

    /* initialize card */
    radeon_waitfifo( adrv, adev, 1 );

    radeon_out32( mmio, DP_GUI_MASTER_CNTL,
			GMC_BRUSH_SOLIDCOLOR         |
			GMC_SRC_DSTCOLOR             |
			GMC_ROP3_PATCOPY             |
			GMC_DP_SRC_RECT              |
			GMC_DST_CLR_CMP_FCN_CLEAR    |
			GMC_WRITE_MASK_DIS );

    return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
    RADEONDeviceData *adev = ( RADEONDeviceData* ) device_data;
    RADEONDriverData *adrv = ( RADEONDriverData* ) driver_data;
    volatile __u8    *mmio = adrv->mmio_base;

    DEBUGMSG( "DirectFB/RADEON: FIFO Performance Monitoring:\n" );
    DEBUGMSG( "DirectFB/RADEON:  %9d radeon_waitfifo calls\n",
               adev->waitfifo_calls );
    DEBUGMSG( "DirectFB/RADEON:  %9d register writes (radeon_waitfifo sum)\n",
	      adev->waitfifo_sum );
    DEBUGMSG( "DirectFB/RADEON:  %9d FIFO wait cycles (depends on CPU)\n",
	      adev->fifo_waitcycles );
    DEBUGMSG( "DirectFB/RADEON:  %9d IDLE wait cycles (depends on CPU)\n",
	      adev->idle_waitcycles );
    DEBUGMSG( "DirectFB/RADEON:  %9d FIFO space cache hits(depends on CPU)\n",
	      adev->fifo_cache_hits );
    DEBUGMSG( "DirectFB/RADEON: Conclusion:\n" );
    DEBUGMSG( "DirectFB/RADEON:  Average register writes/radeon_waitfifo"
	      "call:%.2f\n",
	      adev->waitfifo_sum / ( float )( adev->waitfifo_calls ) );
    DEBUGMSG( "DirectFB/RADEON:  Average wait cycles/radeon_waitfifo call:"
	      " %.2f\n",
	      adev->fifo_waitcycles / ( float )( adev->waitfifo_calls ) );
    DEBUGMSG( "DirectFB/RADEON:  Average fifo space cache hits: %02d%%\n",
	      ( int )( 100 * adev->fifo_cache_hits /
		       ( float )( adev->waitfifo_calls ) ) );

    /* clean up, make sure that radeonfb does not hang in kernel space
       afterwards  */
    radeon_waitfifo( adrv, adev, 1 );

    radeon_out32( mmio, DP_GUI_MASTER_CNTL,
			GMC_BRUSH_SOLIDCOLOR         |
			GMC_SRC_DSTCOLOR             |
			GMC_ROP3_PATCOPY             |
			GMC_DP_SRC_RECT              |
			GMC_DST_CLR_CMP_FCN_CLEAR    |
			GMC_WRITE_MASK_DIS);
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
    RADEONDriverData *adrv = ( RADEONDriverData* ) driver_data;

    dfb_gfxcard_unmap_mmio( device, adrv->mmio_base, -1 );
}
