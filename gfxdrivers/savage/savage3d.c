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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include "savage.h"
#include "savage_bci.h"
#include "savage3d.h"
#include "mmio.h"
#include "savage_streams_old.h"



/* required implementations */

static void savage3DEngineSync( void *drv, void *dev )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;

     savage3D_waitidle( sdrv, sdev );
}

#define SAVAGE3D_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define SAVAGE3D_DRAWING_FUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE)

#define SAVAGE3D_BLITTING_FLAGS \
               (DSBLIT_SRC_COLORKEY)

#define SAVAGE3D_BLITTING_FUNCTIONS \
               (DFXL_BLIT)


static inline void savage3D_validate_gbd( Savage3DDriverData *sdrv,
                                          Savage3DDeviceData *sdev,
                                          CardState          *state )
{
     __u32 BitmapDescriptor;
     CoreSurface *destination;
     SurfaceBuffer *buffer;
     int bpp;


     if (sdev->v_gbd)
          return;

     destination = state->destination;
     buffer = destination->back_buffer;
     bpp = DFB_BYTES_PER_PIXEL(destination->format);
     
     BitmapDescriptor = BCI_BD_BW_DISABLE | 8 | 1;
     BCI_BD_SET_BPP( BitmapDescriptor, bpp * 8 );
     BCI_BD_SET_STRIDE( BitmapDescriptor, buffer->video.pitch / bpp );

     /* strange effects if we don't wait here for the Savage3D being idle */
     savage3D_waitidle( sdrv, sdev );
     
     savage3D_waitfifo( sdrv, sdev, 4 );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_GBD1 );
     BCI_SEND( buffer->video.offset );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_GBD2 );
     BCI_SEND( BitmapDescriptor );

     sdev->v_gbd = 1;
}

static inline void savage3D_validate_pbd( Savage3DDriverData *sdrv,
                                          Savage3DDeviceData *sdev,
                                          CardState          *state )
{
     __u32 BitmapDescriptor;
     CoreSurface *source;
     SurfaceBuffer *buffer;
     int bpp;


     if (sdev->v_pbd)
          return;

     source = state->source;
     buffer = source->front_buffer;
     bpp = DFB_BYTES_PER_PIXEL(source->format);
     
     BitmapDescriptor = BCI_BD_BW_DISABLE;
     BCI_BD_SET_BPP( BitmapDescriptor, bpp * 8 );
     BCI_BD_SET_STRIDE( BitmapDescriptor, buffer->video.pitch / bpp );

     
     savage3D_waitfifo( sdrv, sdev, 4 );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_PBD1 );
     BCI_SEND( buffer->video.offset );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_PBD2 );
     BCI_SEND( BitmapDescriptor );
     
     sdev->v_pbd = 1;
}

static inline void savage3D_validate_color( Savage3DDriverData *sdrv,
                                            Savage3DDeviceData *sdev,
                                            CardState          *state )
{
     if (sdev->v_color)
          return;

     savage3D_waitfifo( sdrv, sdev, 2 );

     BCI_SEND( BCI_CMD_NOP | BCI_CMD_SEND_COLOR );
     
     switch (state->destination->format) {
          case DSPF_A8:
               BCI_SEND( state->color.a );
               break;
          case DSPF_RGB15:
               BCI_SEND( PIXEL_RGB15(state->color.r,
                                     state->color.g,
                                     state->color.b) );
               break;
          case DSPF_RGB16:
               BCI_SEND( PIXEL_RGB16(state->color.r,
                                     state->color.g,
                                     state->color.b) );
               break;
          case DSPF_RGB32:
               BCI_SEND( PIXEL_RGB32(state->color.r,
                                     state->color.g,
                                     state->color.b) );
               break;
          case DSPF_ARGB:
               BCI_SEND( PIXEL_ARGB(state->color.a,
                                    state->color.r,
                                    state->color.g,
                                    state->color.b) );
               break;
          default:
               ONCE( "unsupported destination format" );
               break;
     }

     sdev->v_color = 1;
}

static inline void savage3D_set_clip( Savage3DDriverData *sdrv,
                                      Savage3DDeviceData *sdev,
                                      DFBRegion          *clip )
{
     savage3D_waitfifo( sdrv, sdev, 3 );

     BCI_SEND( BCI_CMD_NOP | BCI_CMD_CLIP_NEW );

     BCI_SEND( BCI_CLIP_TL( clip->y1, clip->x1 ) );
     BCI_SEND( BCI_CLIP_BR( clip->y2, clip->x2 ) );
}

static void savage3DCheckState( void *drv, void *dev,
                                CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~SAVAGE3D_DRAWING_FLAGS)
               return;

          state->accel |= SAVAGE3D_DRAWING_FUNCTIONS;
     }
     else {
          if (state->source->format != state->destination->format)
               return;

          if (state->blittingflags & ~SAVAGE3D_BLITTING_FLAGS)
               return;

          state->accel |= SAVAGE3D_BLITTING_FUNCTIONS;
     }
}


static void savage3DSetState( void *drv, void *dev,
                              GraphicsDeviceFuncs *funcs,
                              CardState *state, DFBAccelerationMask accel )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;
     
     if (state->modified) {
          if (state->modified & SMF_DESTINATION)
               sdev->v_gbd = sdev->v_color = 0;
          else if (state->modified & SMF_COLOR)
               sdev->v_color = 0;

          if (state->modified & SMF_SOURCE)
               sdev->v_pbd = 0;
     }
          
     savage3D_validate_gbd( sdrv, sdev, state );
     
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               savage3D_validate_color( sdrv, sdev, state );

               state->set |= SAVAGE3D_DRAWING_FUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               savage3D_validate_pbd( sdrv, sdev, state );

               state->set |= SAVAGE3D_BLITTING_FUNCTIONS;
               break;

          default:
               BUG( "unexpected drawing/blitting function!" );
               return;
     }

     if (state->modified & SMF_BLITTING_FLAGS) {
          if (state->blittingflags & DSBLIT_SRC_COLORKEY)
               sdev->Cmd_Src_Transparent = BCI_CMD_SRC_TRANSPARENT |
                                           BCI_CMD_SEND_COLOR;
          else
               sdev->Cmd_Src_Transparent = 0;
     }

     if (state->modified & SMF_CLIP)
          savage3D_set_clip( sdrv, sdev, &state->clip );

     if (state->modified & SMF_SRC_COLORKEY)
          sdev->src_colorkey = state->src_colorkey;
     
     state->modified = 0;
}

static void savage3DFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;
     
     savage3D_waitfifo( sdrv, sdev, 3 );

     BCI_SEND( BCI_CMD_RECT | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );

     BCI_SEND( BCI_X_Y(rect->x, rect->y) );
     BCI_SEND( BCI_W_H(rect->w, rect->h) );
}

static void savage3DDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;
     
     savage3D_waitfifo( sdrv, sdev, 12 );

     /* first line */
     BCI_SEND( BCI_CMD_RECT | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( BCI_X_Y( rect->x, rect->y) );
     BCI_SEND( BCI_W_H( 1 , rect->h) );     

     /* second line */
     BCI_SEND( BCI_CMD_RECT | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( BCI_X_Y( rect->x, rect->y) );
     BCI_SEND( BCI_W_H( rect->w , 1 ) );
     
     /* third line */
     BCI_SEND( BCI_CMD_RECT | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( BCI_X_Y( rect->x, rect->y+rect->h-1 ) );
     BCI_SEND( BCI_W_H( rect->w , 1 ) );     
     
     /* fourth line */
     BCI_SEND( BCI_CMD_RECT | BCI_CMD_CLIP_CURRENT |
          BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
          BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( BCI_X_Y( rect->x+rect->w-1, rect->y ) );
     BCI_SEND( BCI_W_H( 1 , rect->h ) );
}

static void savage3DDrawLine( void *drv, void *dev, DFBRegion *line )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;
     
     int dx, dy;
     int min, max, xp, yp, ym;
     

     dx = line->x2 - line->x1;
     dy = line->y2 - line->y1;

     xp = (dx >= 0);
     if (!xp)
          dx = -dx;

     yp = (dy >= 0);
     if (!yp)
          dy = -dy;

     ym = (dy > dx);
     if (ym) {
          max = dy + 1;
          min = dx;
     }
     else {
          max = dx + 1;
          min = dy;
     }

     savage3D_waitfifo( sdrv, sdev, 4 );
    
     BCI_SEND( BCI_CMD_LINE_LAST_PIXEL | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
    
     BCI_SEND( BCI_LINE_X_Y( line->x1, line->y1 ) );
     BCI_SEND( BCI_LINE_STEPS( 2 * (min - max), 2 * min ) );
     BCI_SEND( BCI_LINE_MISC( max, ym, xp, yp, 2 * min - max ) );
}

static void savage3DFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
}

static void savage3DBlit( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy )
{
     Savage3DDriverData *sdrv = (Savage3DDriverData*) drv;
     Savage3DDeviceData *sdev = (Savage3DDeviceData*) dev;

     __u32 cmd = ( BCI_CMD_RECT | sdev->Cmd_Src_Transparent |
                   BCI_CMD_CLIP_CURRENT | BCI_CMD_DEST_GBD | 
                   BCI_CMD_SRC_PBD_COLOR | (0xcc << 16) );
     
     if (dx < rect->x) {
          cmd |= BCI_CMD_RECT_XP;
     } 
     else {
          dx      += rect->w - 1;
          rect->x += rect->w - 1;
     }

     if (dy < rect->y) { 
          cmd |= BCI_CMD_RECT_YP;
     }
     else {
          dy      += rect->h - 1;
          rect->y += rect->h - 1;
     }

     savage3D_waitfifo( sdrv, sdev, sdev->Cmd_Src_Transparent ? 5 : 4 );

     BCI_SEND( cmd );

     /* we always have to send the colorkey,
        but at least it does not clobber the fill color */
     if (sdev->Cmd_Src_Transparent)
          BCI_SEND( sdev->src_colorkey );

     BCI_SEND( BCI_X_Y( rect->x, rect->y ) );
     BCI_SEND( BCI_X_Y( dx, dy ) );
     BCI_SEND( BCI_W_H( rect->w, rect->h ) );
}

static void savage3DStretchBlit( void *drv, void *dev,
                                 DFBRectangle *sr, DFBRectangle *dr )
{
}

void savage3DAfterSetVar( void *drv, void *dev )
{
}

/* exported symbols */

void
savage3d_get_info( GraphicsDevice     *device,
                   GraphicsDriverInfo *info )
{
     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (Savage3DDriverData);
     info->device_data_size = sizeof (Savage3DDeviceData);
}

DFBResult
savage3d_init_driver( GraphicsDevice      *device,
                      GraphicsDeviceFuncs *funcs,
                      void                *driver_data )
{
     funcs->CheckState    = savage3DCheckState;
     funcs->SetState      = savage3DSetState;
     funcs->EngineSync    = savage3DEngineSync;          
     funcs->AfterSetVar   = savage3DAfterSetVar;

     funcs->FillRectangle = savage3DFillRectangle;
     funcs->DrawRectangle = savage3DDrawRectangle;
     funcs->DrawLine      = savage3DDrawLine;
     funcs->FillTriangle  = savage3DFillTriangle;
     funcs->Blit          = savage3DBlit;
     funcs->StretchBlit   = savage3DStretchBlit;

     return DFB_OK;
}

DFBResult
savage3d_init_device( GraphicsDevice     *device,
                      GraphicsDeviceInfo *device_info,
                      void               *driver_data,
                      void               *device_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     unsigned long	cobIndex;	/* size index */
     unsigned long	cobSize;	/* size in bytes */
     unsigned long	cobOffset;	/* offset in frame buffer */
     
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Savage3D Series" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "S3" );


     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = SAVAGE3D_DRAWING_FUNCTIONS |
                                  SAVAGE3D_BLITTING_FUNCTIONS;
     device_info->caps.drawing  = SAVAGE3D_DRAWING_FLAGS;
     device_info->caps.blitting = SAVAGE3D_BLITTING_FLAGS;

     device_info->limits.surface_byteoffset_alignment = 2048;
     device_info->limits.surface_pixelpitch_alignment = 32;

     
  	 cobIndex = 7;
	 cobSize = 0x400 << cobIndex;
	 cobOffset = dfb_gfxcard_reserve_memory( device, cobSize );
     
     
         /*  savage_out32( 0x8504, 0x00008000 );  */
     
     /* Disable BCI */
	 savage_out32( mmio, 0x48C18,
                   savage_in32( mmio, 0x48C18 ) & 0x3FF0);

	 /* Setup BCI command overflow buffer */
	 savage_out32( mmio, 0x48C14, (cobOffset >> 11) | (cobIndex << 29));

	 /* Program shadow status update. */
	 savage_out32( mmio, 0x48C10, 0x78207220);
	 savage_out32( mmio, 0x48C0C, 0);
	    
     /* Enable BCI and command overflow buffer */
	 savage_out32( mmio, 0x48C18, savage_in32( mmio, 0x48C18 ) | 0x0C);
     
     return DFB_OK;
}

void
savage3d_close_device( GraphicsDevice *device,
                       void           *driver_data,
                       void           *device_data )
{
}

void
savage3d_close_driver( GraphicsDevice *device,
                       void           *driver_data )
{
}

