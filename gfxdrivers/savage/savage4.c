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
#include "savage4.h"
#include "mmio.h"
#include "savage_bci.h"
/*
 * (comment stolen from xfree86 savage driver).
 * There are two different streams engines used in the Savage line.
 * The old engine is in the 3D, 4, Pro, and Twister.
 * The new engine is in the 2000, MX, IX, and Super.
 */
#include "savage_streams_old.h"

/* #define SAVAGE_DEBUG */
#ifdef SAVAGE_DEBUG
#define SVGDBG(x...) fprintf(stderr, "savage4:" x)
#else
#define SVGDBG(x...)
#endif

/* required implementations */

static void savage4EngineSync( void *drv, void *dev )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;
     
     SVGDBG("savage4enginesync\n");
     savage4_waitidle( sdrv, sdev );
}

#define SAVAGE4_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define SAVAGE4_DRAWING_FUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE)

#define SAVAGE4_BLITTING_FLAGS \
               (DSBLIT_SRC_COLORKEY)

#define SAVAGE4_BLITTING_FUNCTIONS \
               (DFXL_BLIT)


static inline void savage4_validate_gbd( Savage4DriverData *sdrv,
                                         Savage4DeviceData *sdev,
                                         CardState         *state )
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

     /* strange effects if we don't wait here for the Savage4 being idle */
     savage4_waitidle( sdrv, sdev );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_GBD1 );
     BCI_SEND( buffer->video.offset );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_GBD2 );
     BCI_SEND( BitmapDescriptor );

     sdev->v_gbd = 1;
}

static inline void savage4_validate_pbd( Savage4DriverData *sdrv,
                                         Savage4DeviceData *sdev,
                                         CardState         *state )
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

     
     savage4_waitidle( sdrv, sdev );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_PBD1 );
     BCI_SEND( buffer->video.offset );

     BCI_SEND( BCI_CMD_SETREG | (1 << 16) | BCI_PBD2 );
     BCI_SEND( BitmapDescriptor );
     
     sdev->v_pbd = 1;
}

static inline void savage4_validate_color( Savage4DeviceData *sdev,
                                           CardState         *state )
{
     if (sdev->v_color)
          return;
     
     switch (state->destination->format) {
          case DSPF_A8:
               sdev->Fill_Color = state->color.a;
               break;
          case DSPF_RGB15:
               sdev->Fill_Color = PIXEL_RGB15(state->color.r,
                                              state->color.g,
                                              state->color.b);
               break;
          case DSPF_RGB16:
               sdev->Fill_Color = PIXEL_RGB16(state->color.r,
                                              state->color.g,
                                              state->color.b);
               break;
          case DSPF_RGB32:
               sdev->Fill_Color = PIXEL_RGB32(state->color.r,
                                              state->color.g,
                                              state->color.b);
               break;
          case DSPF_ARGB:
               sdev->Fill_Color = PIXEL_ARGB(state->color.a,
                                             state->color.r,
                                             state->color.g,
                                             state->color.b);
               break;
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
               sdev->Fill_Color = PIXEL_RGB332(state->color.r,
                                               state->color.g,
                                               state->color.b);
               break;
#endif
          default:
               BUG( "unexpected destination format" );
               break;
     }

     sdev->v_color = 1;
}

static inline void savage4_set_clip( Savage4DriverData *sdrv,
                                     Savage4DeviceData *sdev,
                                     DFBRegion         *clip )
{
     SVGDBG("savage4_set_clip x1:%i y1:%i x2:%i y2:%i\n",
            clip->x1, clip->y1, clip->x2, clip->y2);
     savage4_waitfifo( sdrv, sdev, 3 );

     BCI_SEND( BCI_CMD_NOP | BCI_CMD_CLIP_NEW );

     BCI_SEND( BCI_CLIP_TL( clip->y1, clip->x1 ) );
     BCI_SEND( BCI_CLIP_BR( clip->y2, clip->x2 ) );
}

static void savage4CheckState( void *drv, void *dev,
                               CardState *state, DFBAccelerationMask accel )
{
     SVGDBG("savage4checkstate\n");
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
#endif
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~SAVAGE4_DRAWING_FLAGS)
               return;

          state->accel |= SAVAGE4_DRAWING_FUNCTIONS;
     }
     else {
          if (state->source->format != state->destination->format)
               return;

          if (state->blittingflags & ~SAVAGE4_BLITTING_FLAGS)
               return;

          state->accel |= SAVAGE4_BLITTING_FUNCTIONS;
     }
}

static void savage4SetState( void *drv, void *dev,
                             GraphicsDeviceFuncs *funcs,
                             CardState *state, DFBAccelerationMask accel )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;
     
     SVGDBG("savage4setstate\n");
     if (state->modified) {
          if (state->modified & SMF_DESTINATION)
               sdev->v_gbd = sdev->v_color = 0;
          else if (state->modified & SMF_COLOR)
               sdev->v_color = 0;

          if (state->modified & SMF_SOURCE)
               sdev->v_pbd = 0;
     }
     
     savage4_validate_gbd( sdrv, sdev, state );
     
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               savage4_validate_color( sdev, state );

               state->set |= SAVAGE4_DRAWING_FUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               savage4_validate_pbd( sdrv, sdev, state );
               
               state->set |= SAVAGE4_BLITTING_FUNCTIONS;
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
          savage4_set_clip( sdrv, sdev, &state->clip );
     
     if (state->modified & SMF_SRC_COLORKEY)
          sdev->src_colorkey = state->src_colorkey;

     state->modified = 0;
}


static void savage4FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;

     savage4_waitfifo( sdrv, sdev, 4 );

     BCI_SEND( BCI_CMD_RECT | BCI_CMD_SEND_COLOR |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( sdev->Fill_Color );
     
     BCI_SEND( BCI_X_Y(rect->x, rect->y) );
     BCI_SEND( BCI_W_H(rect->w, rect->h) );
}

static void savage4DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;
     
     savage4_waitfifo( sdrv, sdev, 13 );

     /* first line */
     BCI_SEND( BCI_CMD_RECT | BCI_CMD_SEND_COLOR |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
     
     BCI_SEND( sdev->Fill_Color );
     
     BCI_SEND( BCI_X_Y( rect->x, rect->y) );
     BCI_SEND( BCI_W_H( 1 , rect->h) );     

     /* second line */
     BCI_SEND( BCI_CMD_RECT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );

     BCI_SEND( BCI_X_Y( rect->x, rect->y) );
     BCI_SEND( BCI_W_H( rect->w , 1 ) );

     /* third line */
     BCI_SEND( BCI_CMD_RECT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );

     BCI_SEND( BCI_X_Y( rect->x, rect->y+rect->h-1 ) );
     BCI_SEND( BCI_W_H( rect->w , 1 ) );     
     
          
     /* fourth line */
     BCI_SEND( BCI_CMD_RECT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );

     BCI_SEND( BCI_X_Y( rect->x+rect->w-1, rect->y ) );
     BCI_SEND( BCI_W_H( 1 , rect->h ) );
}

static void savage4DrawLine( void *drv, void *dev, DFBRegion *line )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;
     
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

     savage4_waitfifo( sdrv, sdev, 5 );
    
     BCI_SEND( BCI_CMD_LINE_LAST_PIXEL | BCI_CMD_CLIP_CURRENT |
               BCI_CMD_RECT_XP | BCI_CMD_RECT_YP | BCI_CMD_SEND_COLOR |
               BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
    
     BCI_SEND( sdev->Fill_Color );
     
     BCI_SEND( BCI_LINE_X_Y( line->x1, line->y1 ) );
     BCI_SEND( BCI_LINE_STEPS( 2 * (min - max), 2 * min ) );
     BCI_SEND( BCI_LINE_MISC( max, ym, xp, yp, 2 * min - max ) );
}

static void savage4FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
}

static void savage4Blit( void *drv, void *dev,
                         DFBRectangle *rect, int dx, int dy )
{
     Savage4DriverData *sdrv = (Savage4DriverData*) drv;
     Savage4DeviceData *sdev = (Savage4DeviceData*) dev;
     
     __u32 cmd = ( BCI_CMD_RECT | sdev->Cmd_Src_Transparent |
                   BCI_CMD_CLIP_CURRENT | BCI_CMD_DEST_GBD | 
                   BCI_CMD_SRC_PBD_COLOR | (0xcc << 16) );
 
     SVGDBG("savage4Blit x:%i y:%i w:%i h:%i dx:%i dy:%i\n",
            rect->x, rect->y, rect->w, rect->h, dx, dy);

     if (dx < rect->x && dx >= 0) {
          cmd |= BCI_CMD_RECT_XP; /* left to right */
     } 
     else {
          dx      += rect->w - 1;
          rect->x += rect->w - 1;
     }

     if (dy < rect->y && dy >= 0) { 
          cmd |= BCI_CMD_RECT_YP; /* top to bottom */
     }
     else {
          dy      += rect->h - 1;
          rect->y += rect->h - 1;
     }

     savage4_waitfifo( sdrv, sdev, sdev->Cmd_Src_Transparent ? 5 : 4 );

     BCI_SEND( cmd );

     /* we always have to send the colorkey,
        but at least it does not clobber the fill color */
     if (sdev->Cmd_Src_Transparent)
          BCI_SEND( sdev->src_colorkey );

     BCI_SEND( BCI_X_Y( rect->x, rect->y ) );
     BCI_SEND( BCI_X_Y( dx, dy ) );
     BCI_SEND( BCI_W_H( rect->w, rect->h ) );
}

static void savage4StretchBlit( void *drv, void *dev,
                                DFBRectangle *sr, DFBRectangle *dr )
{
}

static void savage4AfterSetVar( void *drv, void *dev )
{
     SVGDBG("savage4aftersetvar\n");
}

/* exported symbols */

void
savage4_get_info( GraphicsDevice     *device,
                  GraphicsDriverInfo *info )
{
     SVGDBG("savage4getinfo\n");
     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (Savage4DriverData);
     info->device_data_size = sizeof (Savage4DeviceData);
}

DFBResult
savage4_init_driver( GraphicsDevice      *device,
                     GraphicsDeviceFuncs *funcs,
                     void                *driver_data )
{
     SVGDBG("savage4initdriver\n");
     funcs->CheckState    = savage4CheckState;
     funcs->SetState      = savage4SetState;
     funcs->EngineSync    = savage4EngineSync;          

     funcs->AfterSetVar   = savage4AfterSetVar;

     funcs->FillRectangle = savage4FillRectangle;
     funcs->DrawRectangle = savage4DrawRectangle;
     funcs->DrawLine      = savage4DrawLine;
     funcs->FillTriangle  = savage4FillTriangle;
     funcs->Blit          = savage4Blit;
     funcs->StretchBlit   = savage4StretchBlit;

     /* setup primary layer functions */
     dfb_layers_hook_primary(device, driver_data, &savagePrimaryFuncs,
                             &pfuncs, &pdriver_data);

     /* setup secondary layer functions */
     dfb_layers_register(device, driver_data, &savageSecondaryFuncs);

     return DFB_OK;
}

DFBResult
savage4_init_device( GraphicsDevice     *device,
                     GraphicsDeviceInfo *device_info,
                     void               *driver_data,
                     void               *device_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     SVGDBG("savage4initdevice\n");

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Savage4 Series" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "S3" );


     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = SAVAGE4_DRAWING_FUNCTIONS |
                                  SAVAGE4_BLITTING_FUNCTIONS;
     device_info->caps.drawing  = SAVAGE4_DRAWING_FLAGS;
     device_info->caps.blitting = SAVAGE4_BLITTING_FLAGS;

     device_info->limits.surface_byteoffset_alignment = 2048;
     device_info->limits.surface_pixelpitch_alignment = 32;

     
     vga_out8( mmio, 0x3d4, CR_SYSCONF );
     vga_out8( mmio, 0x3d5, CR_SYSCONF_ENABLE_2D_ENGINE_IO_ACCESS );

     vga_out8( mmio, 0x3d4, CR_MEMCONF );
     vga_out8( mmio, 0x3d5, CR_MEMCONF_ENABLE_VGA_16BIT_IO_ACCESS |
                            CR_MEMCONF_ENHANCED_MODE_MEMORY_MAPPING );


     /* Setup plane masks */
     savage_out32( mmio, SAVAGE_2D_WRITE_MASK, ~0 );
     savage_out32( mmio, SAVAGE_2D_READ_MASK, ~0 );
     savage_out16( mmio, SAVAGE_2D_BACKGROUND_MIX, 0x03 );
     savage_out16( mmio, SAVAGE_2D_FOREGROUND_MIX, 0x27 );

     /* Disable BCI */
     savage_out32( mmio, SAVAGE_COMMAND_OVERFLOW_BUFFER_POINTERS,
                              (savage_in32( mmio, 0x48C18)) & 0x3FF0);

     /* Program shadow status update */
     savage_out32( mmio, 0x48C10, 0x00700040);

     savage_out32( mmio, 0x48C0C, 0);

     /* Enable BCI without the COB */
     savage_out32( mmio, SAVAGE_COMMAND_OVERFLOW_BUFFER_POINTERS,
                              (savage_in32( mmio, 0x48C18)) | 0x08);
     
     return DFB_OK;
}

void
savage4_close_device( GraphicsDevice *device,
                      void           *driver_data,
                      void           *device_data )
{
     SVGDBG("savage4closedevice\n");
}

void
savage4_close_driver( GraphicsDevice *device,
                      void           *driver_data )
{
     SVGDBG("savage4closedriver\n");
}
/* end of code */
