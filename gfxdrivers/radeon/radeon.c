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
#include <core/screens.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <fbdev/fbdev.h>


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
	D_BUG( "unexpected drawing/blitting function" );
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
	D_BUG( "blitting source/destination format mismatch" );
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
radeonInitScreen( CoreScreen           *screen,
                  GraphicsDevice       *device,
                  void                 *driver_data,
                  void                 *screen_data,
                  DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "Radeon Primary Screen" );

     return DFB_OK;
}

static DFBResult
radeonSetPowerMode( CoreScreen         *screen,
                    void               *driver_data,
                    void               *screen_data,
                    DFBScreenPowerMode  mode )
{
     int    level;
     FBDev *fbdev = dfb_system_data();

     switch (mode) {
          case DSPM_OFF:
               level = 4;
               break;
          case DSPM_SUSPEND:
               level = 3;
               break;
          case DSPM_STANDBY:
               level = 2;
               break;
          case DSPM_ON:
               level = 0;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     if (ioctl( fbdev->fd, FBIOBLANK, level ) < 0) {
          D_PERROR( "DirectFB/matrox: Display blanking failed!\n" );

          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
radeonWaitVSync( CoreScreen *screen,
		 void       *driver_data,
		 void       *layer_data )
{
    RADEONDriverData *rdrv = (RADEONDriverData*) driver_data;
    int               i;

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

static DFBResult
radeonGetScreenSize( CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data,
                     int        *ret_width,
                     int        *ret_height )
{
     FBDev       *fbdev = dfb_system_data();
     FBDevShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (shared->current_mode) {
          *ret_width  = shared->current_mode->xres;
          *ret_height = shared->current_mode->yres;
     }
     else if (shared->modes) {
          *ret_width  = shared->modes->xres;
          *ret_height = shared->modes->yres;
     }
     else {
          D_WARN( "no current and no default mode" );
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static ScreenFuncs radeonScreenFuncs = {
     InitScreen:         radeonInitScreen,
     SetPowerMode:       radeonSetPowerMode,
     WaitVSync:          radeonWaitVSync,
     GetScreenSize:      radeonGetScreenSize
};


/* probe functions */

static const struct {
     __u16       id;
     __u32       chip;
     const char *name;
} dev_table[] = {
    { 0x5144, R_100, "Radeon R100 QD [Radeon 7200]" },
    { 0x5145, R_100, "Radeon R100 QE" },
    { 0x5146, R_100, "Radeon R100 QF" },
    { 0x5147, R_100, "Radeon R100 QG" },
    { 0x5159, R_120, "Radeon RV100 QY [Radeon 7000/VE]" },
    { 0x515a, R_120, "Radeon RV100 QZ [Radeon 7000/VE]" },
    { 0x4c59, R_120, "Radeon Mobility M6 LY" },
    { 0x4c5a, R_120, "Radeon Mobility M6 LZ" },
    { 0x4136, R_150, "Radeon IGP 320 M" },
    { 0x4336, R_150, "Radeon Mobility U1" },
    { 0x4c57, R_150, "Radeon Mobility M7 LW [Radeon Mobility 7500]" },
    { 0x4c58, R_150, "Radeon RV200 LX [Mobility FireGL 7800 M7]" },
    { 0x5157, R_150, "Radeon RV200 QW [Radeon 7500]" },
    { 0x5158, R_150, "Radeon RV200 QX [Radeon 7500]" },
    { 0x4242, R_200, "R200 BB [Radeon All in Wonder 8500DV]" },
    { 0x4243, R_200, "R200 BC [Radeon All in Wonder 8500]" },
    { 0x5148, R_200, "Radeon R200 QH [Radeon 8500]" },
    { 0x5149, R_200, "Radeon R200 QI" },
    { 0x514a, R_200, "Radeon R200 QJ" },
    { 0x514b, R_200, "Radeon R200 QK" },
    { 0x514c, R_200, "Radeon R200 QL [Radeon 8500 LE]" },
    { 0x514d, R_200, "Radeon R200 QM [Radeon 9100]" },
    { 0x514e, R_200, "Radeon R200 QN [Radeon 8500LE]" },
    { 0x514f, R_200, "Radeon R200 QO [Radeon 8500LE]" },
    { 0x5168, R_200, "Radeon R200 Qh" },
    { 0x5169, R_200, "Radeon R200 Qi" },
    { 0x516a, R_200, "Radeon R200 Qj" },
    { 0x516b, R_200, "Radeon R200 Qk" },
    { 0x516c, R_200, "Radeon R200 Ql" },
    { 0x4137, R_200, "Radeon IGP330/340/350" }, 
    { 0x4337, R_200, "Radeon IGP 330M/340M/350M" },
    { 0x4967, R_250, "Radeon RV250 Ig [Radeon 9000]" },
    { 0x4237, R_250, "Radeon 7000 IGP" },
    { 0x4437, R_250, "Radeon Mobility 7000 IGP" },
    { 0x4964, R_250, "Radeon RV250 Id [Radeon 9000]" },
    { 0x4965, R_250, "Radeon RV250 Ie [Radeon 9000]" },
    { 0x4966, R_250, "Radeon RV250 If [Radeon 9000]" },
    { 0x4967, R_250, "Radeon RV250 Ig [Radeon 9000]" },
    { 0x4c64, R_250, "Radeon R250 Ld [Radeon Mobility 9000 M9]" },
    { 0x4c65, R_250, "Radeon R250 Le [Radeon Mobility 9000 M9]" },
    { 0x4c66, R_250, "Radeon R250 Lf [FireGL 9000]" },
    { 0x4c67, R_250, "Radeon R250 Lg [Radeon Mobility 9000 M9]" },
    { 0x5960, R_280, "RV280 [Radeon 9200 PRO]" },
    { 0x5961, R_280, "RV280 [Radeon 9200]" },
    { 0x5962, R_280, "RV280 [Radeon 9200]" },
    { 0x4147, R_300, "R300 AG [FireGL Z1/X1]" },
    { 0x4e44, R_300, "Radeon R300 ND [Radeon 9700 Pro]" },
    { 0x4e45, R_300, "Radeon R300 NE [Radeon 9500 Pro]" },
    { 0x4e47, R_300, "Radeon R300 NG [FireGL X1]" },
    { 0x4144, R_300, "R300 AD [Radeon 9500 Pro]" },
    { 0x4145, R_300, "R300 AE [Radeon 9700 Pro]" },
    { 0x4146, R_300, "R300 AF [Radeon 9700 Pro]" },
    { 0x5834, R_300, "Radeon 9100 IGP" },
    { 0x5835, R_300, "RS300M AGP [Radeon Mobility 9100IGP]" },
    { 0x4e46, R_300, "RV350 NF [Radeon 9600]" },
    { 0x4e4a, R_300, "RV350 NJ [Radeon 9800 XT]" },
    { 0x4148, R_350, "R350 AH [Radeon 9800]" },
    { 0x4149, R_350, "R350 AI [Radeon 9800]" },
    { 0x414a, R_350, "R350 AJ [Radeon 9800]" },
    { 0x414b, R_350, "R350 AK [Fire GL X2]" },
    { 0x4e48, R_350, "Radeon R350 [Radeon 9800 Pro]" },
    { 0x4e49, R_350, "Radeon R350 [Radeon 9800]" },
    { 0x4e4a, R_350, "RV350 NJ [Radeon 9800 XT]" },
    { 0x4e4b, R_350, "R350 NK [Fire GL X2]" },
    { 0x4150, R_350, "RV350 AP [Radeon 9600]" },
    { 0x4151, R_350, "RV350 AQ [Radeon 9600]" },
    { 0x4152, R_350, "RV350 AR [Radeon 9600]" },
    { 0x4153, R_350, "RV350 AS [Radeon 9600 AS]" },
    { 0x4154, R_350, "RV350 AT [Fire GL T2]" },
    { 0x4155, R_350, "RV350 AU [Fire GL T2]" },
    { 0x4156, R_350, "RV350 AV [Fire GL T2]" },
    { 0x4157, R_350, "RV350 AW [Fire GL T2]" },
    { 0x4e50, R_350, "RV350 [Mobility Radeon 9600 M10]" },
    { 0x4e51, R_350, "M10 NQ [Radeon Mobility 9600]" },
    { 0x4e52, R_350, "RV350 [Mobility Radeon 9600 M10]" },
    { 0x4e53, R_350, "M10 NS [Radeon Mobility 9600]" },
    { 0x4e54, R_350, "M10 NT [FireGL Mobility T2]" },
    { 0x4e56, R_350, "M11 NV [FireGL Mobility T2e]" },
    { 0x5b60, R_370, "RV370 5B60 [Radeon X300 (PCIE)]" },
    { 0x5b62, R_370, "RV370 5B62 [Radeon X600 (PCIE)]" },
    { 0x5b64, R_370, "RV370 5B64 [FireGL V3100 (PCIE)]" },
    { 0x5b65, R_370, "RV370 5B65 [FireGL D1100 (PCIE)]" },
    { 0x3e50, R_380, "RV380 0x3e50 [Radeon X600]" },
    { 0x3e54, R_380, "RV380 0x3e54 [FireGL V3200]" },
    { 0x3e70, R_380, "RV380 [Radeon X600] Secondary" },
    { 0x4a48, R_420, "R420 JH [Radeon X800]" },
    { 0x4a49, R_420, "R420 JI [Radeon X800PRO]" },
    { 0x4a4a, R_420, "R420 JJ [Radeon X800SE]" },
    { 0x4a4b, R_420, "R420 JK [Radeon X800]" },
    { 0x4a4c, R_420, "R420 JL [Radeon X800]" },
    { 0x4a4d, R_420, "R420 JM [FireGL X3]" },
    { 0x4a4e, R_420, "M18 JN [Radeon Mobility 9800]" },
    { 0x4a50, R_420, "R420 JP [Radeon X800XT]" }
};
    
static int 
radeon_probe_chipset( int *ret_index )
{
     FBDev *fbdev = dfb_system_data();
     int    i;

     if (fbdev && fbdev->shared->device.model == 0x1002) {
          for (i = 0; i < sizeof(dev_table)/sizeof(dev_table[0]); i++) {
               if (dev_table[i].id == fbdev->shared->device.model) {
                    if (ret_index)
                         *ret_index = i;
                    return 1;
               }
          }
     }

     return 0;
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     return radeon_probe_chipset( NULL );
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
    info->version.minor = 2;

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
	
    adrv->device_data = (RADEONDeviceData*) device_data;

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
	dfb_screens_register_primary( device, driver_data, &radeonScreenFuncs );

    /* overlay support */
    dfb_layers_register( dfb_screens_at(DSCID_PRIMARY), driver_data, &RadeonOverlayFuncs );

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

    int id = 0;
     
    if (!radeon_probe_chipset( &id )) {
	D_ERROR( "DirectFB/RADEON: no supported device found!\n" );
	return DFB_FAILURE;
    }

    D_INFO( "DirectFB/RADEON: found %s\n",  dev_table[id].name);
     
    adev->chipset = dev_table[id].chip;

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

    D_DEBUG( "DirectFB/RADEON: FIFO Performance Monitoring:\n" );
    D_DEBUG( "DirectFB/RADEON:  %9d radeon_waitfifo calls\n",
               adev->waitfifo_calls );
    D_DEBUG( "DirectFB/RADEON:  %9d register writes (radeon_waitfifo sum)\n",
	      adev->waitfifo_sum );
    D_DEBUG( "DirectFB/RADEON:  %9d FIFO wait cycles (depends on CPU)\n",
	      adev->fifo_waitcycles );
    D_DEBUG( "DirectFB/RADEON:  %9d IDLE wait cycles (depends on CPU)\n",
	      adev->idle_waitcycles );
    D_DEBUG( "DirectFB/RADEON:  %9d FIFO space cache hits(depends on CPU)\n",
	      adev->fifo_cache_hits );
    D_DEBUG( "DirectFB/RADEON: Conclusion:\n" );
    D_DEBUG( "DirectFB/RADEON:  Average register writes/radeon_waitfifo"
	      "call:%.2f\n",
	      adev->waitfifo_sum / ( float )( adev->waitfifo_calls ) );
    D_DEBUG( "DirectFB/RADEON:  Average wait cycles/radeon_waitfifo call:"
	      " %.2f\n",
	      adev->fifo_waitcycles / ( float )( adev->waitfifo_calls ) );
    D_DEBUG( "DirectFB/RADEON:  Average fifo space cache hits: %02d%%\n",
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
