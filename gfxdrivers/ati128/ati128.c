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
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( ati128 )


#include "regs.h"
#include "mmio.h"
#include "ati128_state.h"
#include "ati128.h"


/* driver capability flags */


#ifndef __powerpc__
#define ATI128_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)
#else
#define ATI128_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)
#endif

#define ATI128_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE)

#define ATI128_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY | DSBLIT_BLEND_ALPHACHANNEL)

#define ATI128_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


/* macro for S12.0 and S14.0 format */
#define S12(val) (((__u16)((__s16)(val)))&0x3fff)
#define S14(val) (((__u16)((__s16)(val)))&0x3fff)

/** CARD FUNCTIONS **/
static void ati128FillRectangle( void *drv, void *dev, DFBRectangle *rect );
static void ati128FillBlendRectangle( void *drv, void *dev, DFBRectangle *rect );
static void ati128DrawRectangle( void *drv, void *dev, DFBRectangle *rect );
static void ati128DrawBlendRectangle( void *drv, void *dev, DFBRectangle *rect );

/* required implementations */

static void ati128EngineSync( void *drv, void *dev )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;

     ati128_waitidle( adrv, adev );
}

static void ati128CheckState( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
          default:
               return;
     }

     /* check for the special drawing function that does not support
        the usually supported drawingflags */
     if (accel == DFXL_DRAWLINE  &&  state->drawingflags != DSDRAW_NOFX)
          return;

     /* if there are no other drawing flags than the supported */
     if (!(accel & ~ATI128_SUPPORTED_DRAWINGFUNCTIONS) &&
         !(state->drawingflags & ~ATI128_SUPPORTED_DRAWINGFLAGS))
          state->accel |= ATI128_SUPPORTED_DRAWINGFUNCTIONS;

     /* if there are no other blitting flags than the supported
        and the source has the minimum size */
     if (!(accel & ~ATI128_SUPPORTED_BLITTINGFUNCTIONS) &&
         !(state->blittingflags & ~ATI128_SUPPORTED_BLITTINGFLAGS)  &&
         state->source &&
         state->source->width  >= 8 &&
         state->source->height >= 8 )
     {
          switch (state->source->format) {
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_RGB24:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    state->accel |= ATI128_SUPPORTED_BLITTINGFUNCTIONS;
               default:
                    ;
          }
     }
}


static void ati128SetState( void *drv, void *dev,
                            GraphicsDeviceFuncs *funcs,
                            CardState *state, DFBAccelerationMask accel )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;

     if ((state->modified & SMF_SOURCE)  &&  state->source)
          ati128_set_source( adrv, adev, state );

     if (state->modified & SMF_DESTINATION)
          ati128_set_destination( adrv, adev, state );

     if (state->modified & SMF_CLIP)
          ati128_set_clip( adrv, adev, state );

     if (state->modified & SMF_COLOR)
          ati128_set_color( adrv, adev, state );

     if (state->modified & SMF_SRC_COLORKEY)
          ati128_out32( adrv->mmio_base, CLR_CMP_CLR_SRC, state->src_colorkey );

     if (state->modified & SMF_BLITTING_FLAGS)
          ati128_set_blittingflags( adrv, adev, state );

     if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
          ati128_set_blending_function( adrv, adev, state );

     if (state->modified & SMF_DRAWING_FLAGS) {
          if (state->drawingflags & DSDRAW_BLEND) {
               funcs->FillRectangle = ati128FillBlendRectangle;
               funcs->DrawRectangle = ati128DrawBlendRectangle;
          }
          else {
               funcs->FillRectangle = ati128FillRectangle;
               funcs->DrawRectangle = ati128DrawRectangle;
          }
     }
}

static void ati128FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     ati128_waitfifo( adrv, adev, 5 );
     /* set the destination datatype */
     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | BRUSH_SOLIDCOLOR );
     /* set direction */
     ati128_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM);
     /* set the drawing command */
     ati128_out32( mmio, DP_MIX, ROP3_PATCOPY | DP_SRC_RECT );
     /* set parameters */
     ati128_out32( mmio, DST_Y_X, (S14(rect->y) << 16) | S12(rect->x) );
     /* this executes the drawing command */
     ati128_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | rect->w );
}

static void ati128FillBlendRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     __u32 fts = adev->ATI_fake_texture_src + (adev->fake_texture_number & 7)*4;
     ati128_waitidle( adrv, adev );
     *((__u32*)  dfb_gfxcard_memory_virtual(fts)  ) = adev->fake_texture_color;
     ati128_waitidle( adrv, adev );

     ati128_out32( mmio, SCALE_3D_DATATYPE, DST_32BPP );
     ati128_out32( mmio, SCALE_PITCH,  1 );
     /* enable scaling with filtering */
     ati128_out32( mmio, SCALE_3D_CNTL, adev->ATI_blend_function );
     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | SRC_DSTCOLOR );
     ati128_out32( mmio, DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT );
     /* flush the texture cache */
     ati128_out32( mmio, TEX_CNTL, TEX_CNTL_ALPHA_EN_ON | TEX_CNTL_TEX_CACHE_FLUSH_ON);
     /* set source offset */
     ati128_out32( mmio, SCALE_OFFSET_0, fts ) ;
     /* set height and width of the source */
     ati128_out32( mmio, SCALE_SRC_HEIGHT_WIDTH, (8 << 16) | 8);
     /* set the scaling increment registers */
     ati128_out32( mmio, SCALE_X_INC, 0 );
     ati128_out32( mmio, SCALE_Y_INC, 0 );
     /* reset accumulator regs */
     ati128_out32( mmio, SCALE_HACC, 0x00000000 );
     ati128_out32( mmio, SCALE_VACC, 0x00000000 );
     /* set the destination coordinates */
     ati128_out32( mmio, SCALE_DST_X_Y, (S12(rect->x) << 16) | S14(rect->y) );
     /* set destination height and width and perform the blit */
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, (rect->h << 16) | rect->w );
     /*reset scaling and texture control register */
     ati128_out32( mmio, SCALE_3D_CNTL, 0x00000000 );
     ati128_out32( mmio, TEX_CNTL,  0);
     adev->fake_texture_number++;
}

static void ati128DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     ati128_waitfifo( adrv, adev, 3 );
     /* set the destination datatype */
     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | BRUSH_SOLIDCOLOR );
     /* set direction */
     ati128_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM);
     /* set the drawing command */
     ati128_out32( mmio, DP_MIX, ROP3_PATCOPY | DP_SRC_RECT );

     ati128_waitfifo( adrv, adev, 7 );
     /* first line */
     ati128_out32( mmio, DST_Y_X, (S14(rect->y) << 16) | S12(rect->x));
     ati128_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1);
     /* second line */
     ati128_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->w );
     /* third line */
     ati128_out32( mmio, DST_Y_X, (S14(rect->y+rect->h-1) << 16) | S12(rect->x));
     ati128_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->w );
     /* fourth line */
     ati128_out32( mmio, DST_Y_X, (S14(rect->y) << 16) | S12(rect->x+rect->w-1));
     ati128_out32( mmio, DST_HEIGHT_WIDTH, rect->h << 16 | 1);
}

static void ati128DrawBlendRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     __u32 fts = adev->ATI_fake_texture_src + (adev->fake_texture_number & 7)*4;

     ati128_waitidle( adrv, adev );
     *((__u32*)  dfb_gfxcard_memory_virtual(fts)  ) = adev->fake_texture_color;
     ati128_waitidle( adrv, adev );

     ati128_out32( mmio, SCALE_3D_DATATYPE, DST_32BPP );
     ati128_out32( mmio, SCALE_PITCH,  1 );

     /* enable scaling with filtering */
     ati128_out32( mmio, SCALE_3D_CNTL, adev->ATI_blend_function );
     ati128_out32( mmio, TEX_CNTL, TEX_CNTL_ALPHA_EN_ON | TEX_CNTL_TEX_CACHE_FLUSH_ON);
     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | SRC_DSTCOLOR );
     ati128_out32( mmio, DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT );
     /* set source offset */
     ati128_out32( mmio, SCALE_OFFSET_0, adev->ATI_fake_texture_src );
     /* set height and width of the source */
     ati128_out32( mmio, SCALE_SRC_HEIGHT_WIDTH, (8 << 16) | 8);
     /* set the scaling increment registers */
     ati128_out32( mmio, SCALE_X_INC, 0 );
     ati128_out32( mmio, SCALE_Y_INC, 0 );
     /* reset accumulator regs */
     ati128_out32( mmio, SCALE_HACC, 0x00000000 );
     ati128_out32( mmio, SCALE_VACC, 0x00000000 );
     /* set the destination coordinates */

     /*-----------------------*/
     /* first line */
     ati128_out32( mmio, SCALE_DST_X_Y, (S12(rect->x) << 16) | S14(rect->y) );
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, (rect->h << 16) | 1);
     /* second line */
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, (1 << 16) | rect->w );
     /* third line */
     ati128_out32( mmio, SCALE_DST_X_Y, (S12(rect->x) << 16) | S14(rect->y+rect->h-1));
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, (1 << 16) | rect->w );
     /* fourth line */
     ati128_out32( mmio, SCALE_DST_X_Y, (S12(rect->x+rect->w-1) << 16) | S14(rect->y));
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, rect->h << 16 | 1);
     /*-----------------------*/

     /* reset scaling and texture control register */
     ati128_out32( mmio, SCALE_3D_CNTL, 0x00000000 );
     ati128_out32( mmio, TEX_CNTL, 0 );
     adev->fake_texture_number++;
}


static void ati128DrawLine( void *drv, void *dev, DFBRegion *line )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     int dx, dy;
     int small, large;
     int x_dir, y_dir, y_major;
     int err, inc, dec;

     /* Determine x & y deltas and x & y direction bits. */
     if (line->x1 < line->x2) {
          dx = line->x2 - line->x1;
          x_dir = 1 << 31;
     }
     else {
          dx = line->x1 - line->x2;
          x_dir = 0 << 31;
     }

     if (line->y1 < line->y2) {
          dy = line->y2 - line->y1;
          y_dir = 1 << 15;
     }
     else {
          dy = line->y1 - line->y2;
          y_dir = 0 << 15;
     }

     /* Determine x & y min and max values; also determine y major bit. */
     if (dx < dy) {
          small = dx;
          large = dy;
          y_major = 1 << 2;
     }
     else {
          small = dy;
          large = dx;
          y_major = 0 << 2;
     }

     /* Calculate Bresenham parameters and draw line. */
     err = -large;
     inc = small * 2;
     dec = large *(-2);

     ati128_waitfifo( adrv, adev, 7 );
     /* set the destination datatype */
     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | BRUSH_SOLIDCOLOR | ROP3_SRCCOPY );
     /* set start coorinates */
     ati128_out32( mmio, DST_Y_X, (S14(line->y1) << 16) | S12(line->x1));
     /* allow setting of last pel bit and polygon
        outline bit for line drawing */
     ati128_out32( mmio, DP_CNTL_XDIR_YDIR_YMAJOR,
                   y_major | y_dir | x_dir );
     /* set bresenham registers and start drawing */
     ati128_out32( mmio, DST_BRES_ERR, err );
     ati128_out32( mmio, DST_BRES_INC, inc );
     ati128_out32( mmio, DST_BRES_DEC, dec );
     ati128_out32( mmio, DST_BRES_LNTH, large + 1 );
}

static void ati128StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     __u32 src = 0;

     __u32 scalex = (__u32)(((double)sr->w/(double)dr->w) * 65536);
     __u32 scaley = (__u32)(((double)sr->h/(double)dr->h) * 65536);

     ati128_waitfifo( adrv, adev, 9 );

     /* make sure that color compare register is restored to last state */

     ati128_out32( mmio, CLR_CMP_CNTL, adev->ATI_color_compare );

     switch (adev->source->format) {
          case DSPF_RGB15:
               ati128_out32( mmio, SCALE_3D_DATATYPE, DST_15BPP );

               ati128_out32( mmio, SCALE_PITCH,
                             adev->source->front_buffer->video.pitch >>4);

               src = adev->source->front_buffer->video.offset +
                     sr->y *
                     adev->source->front_buffer->video.pitch + sr->x*2;

               ati128_out32( mmio, TEX_CNTL, 0);


          case DSPF_RGB16:
               ati128_out32( mmio, SCALE_3D_DATATYPE, DST_16BPP );

               ati128_out32( mmio, SCALE_PITCH,
                             adev->source->front_buffer->video.pitch >>4);

               src = adev->source->front_buffer->video.offset +
                     sr->y *
                     adev->source->front_buffer->video.pitch + sr->x*2;

               ati128_out32( mmio, TEX_CNTL, 0);

               break;
          case DSPF_RGB24:
               ati128_out32( mmio, SCALE_3D_DATATYPE, DST_24BPP );

               ati128_out32( mmio, SCALE_PITCH,
                             adev->source->front_buffer->video.pitch >>3);

               src = adev->source->front_buffer->video.offset +
                     sr->y *
                     adev->source->front_buffer->video.pitch + sr->x*3;

               ati128_out32( mmio, TEX_CNTL, 0);

               break;
          case DSPF_RGB32:
               ati128_out32( mmio, SCALE_3D_DATATYPE, DST_32BPP );

               ati128_out32( mmio, SCALE_PITCH,
                             adev->source->front_buffer->video.pitch >>5);

               src = adev->source->front_buffer->video.offset +
                     sr->y *
                     adev->source->front_buffer->video.pitch + sr->x*4;

               ati128_out32( mmio, TEX_CNTL, 0);

               break;
          case DSPF_ARGB:
               ati128_out32( mmio, SCALE_3D_DATATYPE, DST_32BPP );

               ati128_out32( mmio, SCALE_PITCH,
                             adev->source->front_buffer->video.pitch >>5);

               src = adev->source->front_buffer->video.offset +
                     sr->y *
                     adev->source->front_buffer->video.pitch + sr->x*4;

               if (adev->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    ati128_out32( mmio, TEX_CNTL, TEX_CNTL_ALPHA_EN_ON );
               else
                    ati128_out32( mmio, TEX_CNTL, 0 );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               return;
     }

     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | SRC_DSTCOLOR );
     /* set the blend function */
     ati128_out32( mmio, SCALE_3D_CNTL, adev->ATI_blend_function );
     /* set up source data and copy type */
     ati128_out32( mmio, DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT );
     /* set source offset */
     ati128_out32( mmio, SCALE_OFFSET_0, src);
     /* set height and width of the source */
     ati128_out32( mmio, SCALE_SRC_HEIGHT_WIDTH, (sr->h << 16) | sr->w);

     ati128_waitfifo( adrv, adev, 9 );
     /* set the scaling increment registers */
     ati128_out32( mmio, SCALE_X_INC, scalex );
     ati128_out32( mmio, SCALE_Y_INC, scaley );
     /* reset accumulator regs */
     ati128_out32( mmio, SCALE_HACC, 0x00000000 );
     ati128_out32( mmio, SCALE_VACC, 0x00000000 );
     /* set the destination coordinates */
     ati128_out32( mmio, SCALE_DST_X_Y, (S12(dr->x) << 16) | S14(dr->y) );
     /* set destination height and width and perform the blit */
     ati128_out32( mmio, SCALE_DST_HEIGHT_WIDTH, (dr->h << 16) | dr->w );
     /*reset scaling and texture control register */
     ati128_out32( mmio, SCALE_3D_CNTL, 0x00000000 );
     ati128_out32( mmio, TEX_CNTL, 0x00000000 );

     /* set CLR_CMP_CNTL to zero, to insure that drawing funcions work corrently */
     if (adev->ATI_color_compare)
          ati128_out32( mmio, CLR_CMP_CNTL, 0 );
}

static void ati128Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     ATI128DriverData *adrv = (ATI128DriverData*) drv;
     ATI128DeviceData *adev = (ATI128DeviceData*) dev;
     volatile __u8    *mmio = adrv->mmio_base;

     __u32 dir_cmd = 0;

     if ((adev->source->format != adev->destination->format) ||
         adev->blittingflags == DSBLIT_BLEND_ALPHACHANNEL)
     {
          DFBRectangle sr = { rect->x, rect->y, rect->w, rect->h };
          DFBRectangle dr = { dx, dy, rect->w, rect->h };
          ati128StretchBlit( adrv, adev, &sr, &dr );
          return;
     }

     /* check which blitting direction should be used */
     if (rect->x <= dx) {
          dir_cmd |= DST_X_RIGHT_TO_LEFT;
          rect->x += rect->w-1;
          dx += rect->w-1;
     }
     else {
          dir_cmd |= DST_X_LEFT_TO_RIGHT;
     }
     if (rect->y <= dy) {
          dir_cmd |= DST_Y_BOTTOM_TO_TOP;
          rect->y += rect->h-1;
          dy += rect->h-1;
     }
     else {
          dir_cmd |= DST_Y_TOP_TO_BOTTOM;
     }

     ati128_waitfifo( adrv, adev, 9 );

     /* make sure that color compare register is restored to last state */
     ati128_out32( mmio, CLR_CMP_CNTL, adev->ATI_color_compare );

     /* set blitting direction */
     ati128_out32( mmio, DP_CNTL, dir_cmd );

     ati128_out32( mmio, DP_DATATYPE, adev->ATI_dst_bpp | SRC_DSTCOLOR );
     ati128_out32( mmio, DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT );

     ati128_out32( mmio, SRC_Y_X, (rect->y << 16) | rect->x);
     ati128_out32( mmio, DST_Y_X, (S14(dy) << 16) | S12(dx) );
     ati128_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | rect->w);

     /* set CLR_CMP_CNTL to zero, to insure that drawing funcions work corrently */
     if (adev->ATI_color_compare)
          ati128_out32( mmio, CLR_CMP_CNTL, 0 );

     if (dir_cmd != (DST_Y_TOP_TO_BOTTOM | DST_X_LEFT_TO_RIGHT)) {
          ati128_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT |
                                       DST_Y_TOP_TO_BOTTOM );
     }
}

/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_GRAPHICS_DRIVER_ABI_VERSION;
}

static int
driver_probe( GraphicsDevice *device )
{
#ifdef FB_ACCEL_ATI_RAGE128
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_ATI_RAGE128:          /* ATI Rage 128 */
               return 1;
     }
#endif
     return 0;
}


static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "ATI Rage 128 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof (ATI128DriverData);
     info->device_data_size = sizeof (ATI128DeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data )
{
     ATI128DriverData *adrv = (ATI128DriverData*) driver_data;

     adrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!adrv->mmio_base)
          return DFB_IO;

     funcs->CheckState    = ati128CheckState;
     funcs->SetState      = ati128SetState;
     funcs->EngineSync    = ati128EngineSync;

     funcs->FillRectangle = ati128FillRectangle;
     funcs->DrawRectangle = ati128DrawRectangle;
     funcs->DrawLine      = ati128DrawLine;
     funcs->Blit          = ati128Blit;
     funcs->StretchBlit   = ati128StretchBlit;

     /* overlay support */
     dfb_layers_register( device, driver_data, &atiOverlayFuncs );
     
     return DFB_OK;
}


static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     ATI128DriverData *adrv = (ATI128DriverData*) driver_data;
     ATI128DeviceData *adev = (ATI128DeviceData*) device_data;
     volatile __u8    *mmio = adrv->mmio_base;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Rage 128" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );


     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = ATI128_SUPPORTED_DRAWINGFUNCTIONS |
                                  ATI128_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = ATI128_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = ATI128_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;


     /* initialize card */
     ati128_waitfifo( adrv, adev, 6 );

     ati128_out32( mmio, DP_GUI_MASTER_CNTL,
                         GMC_SRC_PITCH_OFFSET_DEFAULT |
                         GMC_DST_PITCH_OFFSET_DEFAULT |
                         GMC_SRC_CLIP_DEFAULT         |
                         GMC_DST_CLIP_DEFAULT         |
                         GMC_BRUSH_SOLIDCOLOR         |
                         GMC_SRC_DSTCOLOR             |
                         GMC_BYTE_ORDER_MSB_TO_LSB    |
                         GMC_DP_CONVERSION_TEMP_6500  |
                         ROP3_PATCOPY                 |
                         GMC_DP_SRC_RECT              |
                         GMC_3D_FCN_EN_CLR            |
                         GMC_DST_CLR_CMP_FCN_CLEAR    |
                         GMC_AUX_CLIP_CLEAR           |
                         GMC_WRITE_MASK_SET);

     ati128_out32( mmio, SCALE_3D_CNTL, 0x00000000 );
     ati128_out32( mmio, TEX_CNTL, 0x00000000 );

     /* reserve 32bit pixel for fake texture at end of framebuffer */
     adev->ATI_fake_texture_src = dfb_gfxcard_reserve_memory( device, 4*32 );

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     ATI128DeviceData *adev = (ATI128DeviceData*) device_data;
     ATI128DriverData *adrv = (ATI128DriverData*) driver_data;
     volatile __u8    *mmio = adrv->mmio_base;

     DEBUGMSG( "DirectFB/ATI128: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/ATI128:  %9d ati128_waitfifo calls\n",
               adev->waitfifo_calls );
     DEBUGMSG( "DirectFB/ATI128:  %9d register writes (ati128_waitfifo sum)\n",
               adev->waitfifo_sum );
     DEBUGMSG( "DirectFB/ATI128:  %9d FIFO wait cycles (depends on CPU)\n",
               adev->fifo_waitcycles );
     DEBUGMSG( "DirectFB/ATI128:  %9d IDLE wait cycles (depends on CPU)\n",
               adev->idle_waitcycles );
     DEBUGMSG( "DirectFB/ATI128:  %9d FIFO space cache hits(depends on CPU)\n",
               adev->fifo_cache_hits );
     DEBUGMSG( "DirectFB/ATI128: Conclusion:\n" );
     DEBUGMSG( "DirectFB/ATI128:  Average register writes/ati128_waitfifo"
               "call:%.2f\n",
               adev->waitfifo_sum/(float)(adev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/ATI128:  Average wait cycles/ati128_waitfifo call:"
               " %.2f\n",
               adev->fifo_waitcycles/(float)(adev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/ATI128:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * adev->fifo_cache_hits/
               (float)(adev->waitfifo_calls)) );

     /* clean up, make sure that aty128fb does not hang in kernel space
        afterwards  */
     ati128_waitfifo( adrv, adev, 3 );

     ati128_out32( mmio, DP_GUI_MASTER_CNTL,
                         GMC_SRC_PITCH_OFFSET_DEFAULT |
                         GMC_DST_PITCH_OFFSET_DEFAULT |
                         GMC_SRC_CLIP_DEFAULT         |
                         GMC_DST_CLIP_DEFAULT         |
                         GMC_BRUSH_SOLIDCOLOR         |
                         GMC_SRC_DSTCOLOR             |
                         GMC_BYTE_ORDER_MSB_TO_LSB    |
                         GMC_DP_CONVERSION_TEMP_6500  |
                         ROP3_PATCOPY                 |
                         GMC_DP_SRC_RECT              |
                         GMC_3D_FCN_EN_CLR            |
                         GMC_DST_CLR_CMP_FCN_CLEAR    |
                         GMC_AUX_CLIP_CLEAR           |
                         GMC_WRITE_MASK_SET);

     ati128_out32( mmio, SCALE_3D_CNTL, 0x00000000 );
     ati128_out32( mmio, TEX_CNTL, 0x00000000 );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     ATI128DriverData *adrv = (ATI128DriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, adrv->mmio_base, -1 );
}

