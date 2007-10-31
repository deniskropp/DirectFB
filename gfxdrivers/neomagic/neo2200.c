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

#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/screens.h>
#include <core/gfxcard.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include "neomagic.h"

typedef volatile struct {
  u32 bltStat;
  u32 bltCntl;
  u32 xpColor;
  u32 fgColor;
  u32 bgColor;
  u32 pitch;
  u32 clipLT;
  u32 clipRB;
  u32 srcBitOffset;
  u32 srcStart;
  u32 reserved0;
  u32 dstStart;
  u32 xyExt;

  u32 reserved1[19];

  u32 pageCntl;
  u32 pageBase;
  u32 postBase;
  u32 postPtr;
  u32 dataPtr;
} Neo2200;

typedef struct {
     NeoDeviceData neo;

     int dstOrg;
     int dstPitch;
     int dstPixelWidth;

     int srcOrg;
     int srcPitch;
     int srcPixelWidth;

     u32 bltCntl;

     bool src_dst_equal;

     /* state validation */
     int n_bltMode_dst;
     int n_src;
     int n_fgColor;
     int n_xpColor;
} Neo2200DeviceData;

typedef struct {
     NeoDriverData neo;

     Neo2200 *neo2200;
} Neo2200DriverData;


static inline void neo2200_waitidle( Neo2200DriverData *ndrv,
                                     Neo2200DeviceData *ndev )
{
     while (ndrv->neo2200->bltStat & 1)
          ndev->neo.idle_waitcycles++;
}

static inline void neo2200_waitfifo( Neo2200DriverData *ndrv,
                                     Neo2200DeviceData *ndev,
                                     int requested_fifo_space )
{
  ndev->neo.waitfifo_calls++;
  ndev->neo.waitfifo_sum += requested_fifo_space;

  /* FIXME: does not work
  if (neo_fifo_space < requested_fifo_space)
    {
      neo_fifo_waitcycles++;

      while (1)
    {
      neo_fifo_space = (neo2200->bltStat >> 8);
      if (neo_fifo_space >= requested_fifo_space)
        break;
    }
    }
  else
    {
      neo_fifo_cache_hits++;
    }

  neo_fifo_space -= requested_fifo_space;
  */

  neo2200_waitidle( ndrv, ndev );
}



static inline void neo2200_validate_bltMode_dst( Neo2200DriverData     *ndrv,
                                                 Neo2200DeviceData     *ndev,
                                                 CoreSurface           *dst,
                                                 CoreSurfaceBufferLock *lock )
{
  int bltMode = 0;

  if (ndev->n_bltMode_dst)
    return;

  switch (dst->config.format)
    {
    case DSPF_A8:
    case DSPF_LUT8:
    case DSPF_RGB332:
      bltMode |= NEO_MODE1_DEPTH8;
      break;
    case DSPF_ARGB1555:
    case DSPF_RGB16:
    case DSPF_YUY2:
      bltMode |= NEO_MODE1_DEPTH16;
      break;
    default:
      D_BUG( "unexpected pixelformat!" );
      break;
    }

  ndev->dstOrg = lock->offset;
  ndev->dstPitch = lock->pitch;
  ndev->dstPixelWidth = DFB_BYTES_PER_PIXEL(dst->config.format);


  neo2200_waitfifo( ndrv, ndev, 2 );

  ndrv->neo2200->bltStat = bltMode << 16;
  ndrv->neo2200->pitch = (ndev->dstPitch << 16) | (ndev->srcPitch & 0xffff);


  ndev->n_bltMode_dst = 1;
}

static inline void neo2200_validate_src( Neo2200DriverData     *ndrv,
                                         Neo2200DeviceData     *ndev,
                                         CoreSurface           *src,
                                         CoreSurfaceBufferLock *lock )
{
  if (ndev->n_src)
    return;

  ndev->srcOrg = lock->offset;
  ndev->srcPitch = lock->pitch;
  ndev->srcPixelWidth = DFB_BYTES_PER_PIXEL(src->config.format);

  neo2200_waitfifo( ndrv, ndev, 1 );
  ndrv->neo2200->pitch = (ndev->dstPitch << 16) | (ndev->srcPitch & 0xffff);

  ndev->n_src = 1;
}

static inline void neo2200_validate_fgColor( Neo2200DriverData *ndrv,
                                             Neo2200DeviceData *ndev,
                                             CardState         *state )
{
  if (ndev->n_fgColor)
    return;

  neo2200_waitfifo( ndrv, ndev, 1 );

  switch (state->destination->config.format)
    {
    case DSPF_A8:
      ndrv->neo2200->fgColor = state->color.a;
      break;
    case DSPF_LUT8:
      ndrv->neo2200->fgColor = state->color_index;
      break;
    case DSPF_RGB332:
      ndrv->neo2200->fgColor = PIXEL_RGB332( state->color.r,
                                             state->color.g,
                                             state->color.b );
      break;
    case DSPF_ARGB1555:
      ndrv->neo2200->fgColor = PIXEL_ARGB1555( state->color.a,
                                               state->color.r,
                                               state->color.g,
                                               state->color.b );
      break;
    case DSPF_RGB16:
      ndrv->neo2200->fgColor = PIXEL_RGB16( state->color.r,
                                            state->color.g,
                                            state->color.b );
      break;
    default:
      D_BUG( "unexpected pixelformat!" );
      break;
    }

  ndev->n_fgColor = 1;
}

static inline void neo2200_validate_xpColor( Neo2200DriverData *ndrv,
                                             Neo2200DeviceData *ndev,
                                             CardState         *state )
{
  if (ndev->n_xpColor)
    return;

  neo2200_waitfifo( ndrv, ndev, 1 );

  ndrv->neo2200->xpColor = state->src_colorkey;

  ndev->n_xpColor = 1;
}


/* required implementations */

static DFBResult neo2200EngineSync( void *drv, void *dev )
{
     Neo2200DriverData *ndrv = (Neo2200DriverData*) drv;
     Neo2200DeviceData *ndev = (Neo2200DeviceData*) dev;

     neo2200_waitidle( ndrv, ndev );

     return DFB_OK;
}

#define NEO_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

#define NEO_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE)

#define NEO_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY)

#define NEO_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)

static void neo2200CheckState( void *drv, void *dev,
                               CardState *state, DFBAccelerationMask accel )
{
  switch (state->destination->config.format)
    {
    case DSPF_A8:
    case DSPF_LUT8:
    case DSPF_RGB332:
    case DSPF_ARGB1555:
    case DSPF_RGB16:
      break;
    case DSPF_YUY2:
      if (accel == DFXL_BLIT)
        break;
    default:
      return;
    }

  if (DFB_DRAWING_FUNCTION(accel))
    {
      /* if the function is supported and there are no other
         drawing flags than the supported */
      if (!(accel & ~NEO_SUPPORTED_DRAWINGFUNCTIONS) &&
          !(state->drawingflags & ~NEO_SUPPORTED_DRAWINGFLAGS))
        state->accel |= accel;
    }
  else
    {
      /* if the function is supported, there are no other
         blitting flags than the supported, the source and
         destination formats are the same and the source and dest.
         are different due to a blitting bug */
      if (!(accel & ~NEO_SUPPORTED_BLITTINGFUNCTIONS)             &&
          !(state->blittingflags & ~NEO_SUPPORTED_BLITTINGFLAGS)  &&
          state->source->config.format == state->destination->config.format)
        state->accel |= accel;
    }
}

static void neo2200SetState( void *drv, void *dev,
                             GraphicsDeviceFuncs *funcs,
                             CardState *state, DFBAccelerationMask accel )
{
     Neo2200DriverData *ndrv = (Neo2200DriverData*) drv;
     Neo2200DeviceData *ndev = (Neo2200DeviceData*) dev;

     if (state->mod_hw & SMF_DESTINATION)
          ndev->n_fgColor = ndev->n_bltMode_dst = 0;
     else if (state->mod_hw & SMF_COLOR)
          ndev->n_fgColor = 0;

     if (state->mod_hw & SMF_SOURCE)
          ndev->n_src = 0;

     if (state->mod_hw & SMF_SRC_COLORKEY)
          ndev->n_xpColor = 0;

     neo2200_validate_bltMode_dst( ndrv, ndev, state->destination, &state->dst );

     switch (accel) {
          case DFXL_BLIT:
               neo2200_validate_src( ndrv, ndev, state->source, &state->src );

               ndev->src_dst_equal = (state->src.buffer ==
                                      state->dst.buffer);

               if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                    ndev->bltCntl = NEO_BC0_SRC_TRANS;
                    neo2200_validate_xpColor( ndrv, ndev, state );
               }
               else
                    ndev->bltCntl = 0;

               state->set |= DFXL_BLIT;
               break;

          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
               neo2200_validate_fgColor( ndrv, ndev, state );

               state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function!" );
               break;
     }

     state->mod_hw = 0;
}

static bool neo2200FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Neo2200DriverData *ndrv    = (Neo2200DriverData*) drv;
     Neo2200DeviceData *ndev    = (Neo2200DeviceData*) dev;
     Neo2200           *neo2200 = ndrv->neo2200;

     neo2200_waitfifo( ndrv, ndev, 3 );

     /* set blt control */
     neo2200->bltCntl = NEO_BC3_FIFO_EN      |
                     NEO_BC0_SRC_IS_FG    |
                     NEO_BC3_SKIP_MAPPING |  0x0c0000;

     neo2200->dstStart = ndev->dstOrg +
           (rect->y * ndev->dstPitch) +
           (rect->x * ndev->dstPixelWidth);

     neo2200->xyExt    = (rect->h << 16) | (rect->w & 0xffff);

     return true;
}

static bool neo2200DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Neo2200DriverData *ndrv    = (Neo2200DriverData*) drv;
     Neo2200DeviceData *ndev    = (Neo2200DeviceData*) dev;
     Neo2200           *neo2200 = ndrv->neo2200;

     u32 dst = ndev->dstOrg +
              (rect->y * ndev->dstPitch) +
              (rect->x * ndev->dstPixelWidth);

     neo2200_waitfifo( ndrv, ndev, 3 );

     /* set blt control */
     neo2200->bltCntl = NEO_BC3_FIFO_EN      |
                        NEO_BC0_SRC_IS_FG    |
                        NEO_BC3_SKIP_MAPPING | 0x0c0000;

     neo2200->dstStart = dst;
     neo2200->xyExt    = (1 << 16) | (rect->w & 0xffff);


     dst += (rect->h - 1) * ndev->dstPitch;
     neo2200_waitfifo( ndrv, ndev, 2 );
     neo2200->dstStart = dst;
     neo2200->xyExt    = (1 << 16) | (rect->w & 0xffff);


     dst -= (rect->h - 2) * ndev->dstPitch;
     neo2200_waitfifo( ndrv, ndev, 2 );
     neo2200->dstStart = dst;
     neo2200->xyExt    = ((rect->h - 2) << 16) | 1;


     dst += (rect->w - 1) * ndev->dstPixelWidth;
     neo2200_waitfifo( ndrv, ndev, 2 );
     neo2200->dstStart = dst;
     neo2200->xyExt    = ((rect->h - 2) << 16) | 1;

     return true;
}

static bool neo2200Blit( void *drv, void *dev,
                         DFBRectangle *rect, int dx, int dy )
{
     Neo2200DriverData *ndrv    = (Neo2200DriverData*) drv;
     Neo2200DeviceData *ndev    = (Neo2200DeviceData*) dev;
     Neo2200           *neo2200 = ndrv->neo2200;

     u32 src_start, dst_start;
     u32 bltCntl = ndev->bltCntl;

//     fprintf(stderr, "blit: %d, %d (%dx%d) -> %d, %d\n",
//             rect->x, rect->y, rect->w, rect->h, dx, dy);

/*     if (rect->x < dx) {
          //rect->x += rect->w - 1;
          //dx      += rect->w - 1;

          bltCntl |= NEO_BC0_X_DEC;
     }

     if (rect->y < dy) {
          //rect->y += rect->h - 1;
          //dy      += rect->h - 1;

          bltCntl |= NEO_BC0_DST_Y_DEC | NEO_BC0_SRC_Y_DEC;
     }
*/
     /* ARGH, the above code for the blitting direction doesn't work. */
     if (ndev->src_dst_equal && (rect->x < dx || rect->y < dy))
          return false;

     src_start = rect->y * ndev->srcPitch + rect->x * ndev->srcPixelWidth;
     dst_start = dy * ndev->dstPitch + dx * ndev->dstPixelWidth;

     neo2200_waitfifo( ndrv, ndev, 4 );

     /* set blt control */
     neo2200->bltCntl  = bltCntl |
                         NEO_BC3_FIFO_EN      |
                         NEO_BC3_SKIP_MAPPING |  0x0c0000;

     /* set start addresses */
     neo2200->srcStart = ndev->srcOrg + src_start;
     neo2200->dstStart = ndev->dstOrg + dst_start;

     /* set size */
     neo2200->xyExt    = (rect->h << 16) | (rect->w & 0xffff);

     return true;
}



void
neo2200_get_info( CoreGraphicsDevice *device,
                  GraphicsDriverInfo *info )
{
     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (Neo2200DriverData);
     info->device_data_size = sizeof (Neo2200DeviceData);
}

DFBResult
neo2200_init_driver( CoreGraphicsDevice  *device,
                     GraphicsDeviceFuncs *funcs,
                     void                *driver_data )
{
     Neo2200DriverData *ndrv = (Neo2200DriverData*) driver_data;

     ndrv->neo2200 = (Neo2200*) ndrv->neo.mmio_base;

     funcs->CheckState = neo2200CheckState;
     funcs->SetState = neo2200SetState;
     funcs->EngineSync = neo2200EngineSync;

     funcs->FillRectangle = neo2200FillRectangle;
     funcs->DrawRectangle = neo2200DrawRectangle;
     //     funcs->DrawLine = neoDrawLine2D;
     funcs->Blit = neo2200Blit;
     //     funcs->StretchBlit = neoStretchBlit;

     /* overlay support */
     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &neoOverlayFuncs );
     return DFB_OK;
}

DFBResult
neo2200_init_device( CoreGraphicsDevice *device,
                     GraphicsDeviceInfo *device_info,
                     void               *driver_data,
                     void               *device_data )
{
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "2200/2230/2360/2380" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "NeoMagic" );


     device_info->caps.flags    = 0;
     device_info->caps.accel    = NEO_SUPPORTED_DRAWINGFUNCTIONS |
                                  NEO_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = NEO_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = NEO_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     return DFB_OK;
}

void
neo2200_close_device( CoreGraphicsDevice *device,
                      void               *driver_data,
                      void               *device_data )
{
}

void
neo2200_close_driver( CoreGraphicsDevice *device,
                      void               *driver_data )
{
}

