/*
 * $Workfile: $
 * $Revision: 1.1 $
 *
 * File Contents: This file contains the main functions of the NSC DFB.
 *
 * Project:       NSC Direct Frame buffer device driver
 *
 */

/* 
 * NSC_LIC_COPYRIGHT
 *
 * Copyright (c) 2001-2003 National Semiconductor Corporation ("NSC").
 *
 * All Rights Reserved.  Unpublished rights reserved under the copyright 
 * laws of the United States of America, other countries and international 
 * treaties.  The software is provided without fee.  Permission to use, 
 * copy, store, modify, disclose, transmit or distribute the software is 
 * granted, provided that this copyright notice must appear in any copy, 
 * modification, disclosure, transmission or distribution of the software.
 *  
 * NSC retains all ownership, copyright, trade secret and proprietary rights 
 * in the software. 
 * THIS SOFTWARE HAS BEEN PROVIDED "AS IS," WITHOUT EXPRESS OR IMPLIED 
 * WARRANTY INCLUDING, WITHOUT LIMITATION, IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR USE AND NON-INFRINGEMENT.
 *
 * NSC does not assume or authorize any other person to assume for it any 
 * liability in connection with the Software. NSC SHALL NOT BE LIABLE TO 
 * COMPANY, OR ANY THIRD PARTY, IN CONTRACT, TORT, WARRANTY, STRICT 
 * LIABILITY, OR OTHERWISE FOR ANY DIRECT DAMAGES, OR FOR ANY SPECIAL, 
 * INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING BUT NOT 
 * LIMITED TO, BUSINESS INTERRUPTION, LOST PROFITS OR GOODWILL, OR LOSS 
 * OF INFORMATION EVEN IF NSC IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * END_NSC_LIC_COPYRIGHT */

#include <asm/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <directfb.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <core/graphics_driver.h>

#include "nsc_galproto.h"

#define NSC_ACCEL 1

#define GP_VECTOR_DEST_DATA       0x8
#define GP_VECTOR_MINOR_AXIS_POS  0x4
#define GP_VECTOR_MAJOR_AXIS_POS  0x2
#define GP_VECTOR_Y_MAJOR         0x1

#define GX_BC0_DST_Y_DEC       0x00000001
#define GX_BC0_X_DEC           0x00000002
#define GX_BC0_SRC_TRANS       0x00000004
#define GX_BC0_SRC_IS_FG       0x00000008

#define  GFX_CPU_REDCLOUD 3
#define GX_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

#define GX_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | \
                DFXL_DRAWRECTANGLE | \
                DFXL_DRAWLINE)

#define GX_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)

#define GX_SUPPORTED_BLITTINGFLAGS \
                                (DSBLIT_SRC_COLORKEY)

DFB_GRAPHICS_DRIVER(nsc)

typedef struct
{

  unsigned long Color;
  unsigned long src_offset;
  unsigned long dst_offset;
  unsigned long src_pitch;
  unsigned long dst_pitch;
  unsigned long src_colorkey;
  int v_srcColorkey;

}NSCDeviceData;

typedef struct
{
  unsigned int cpu_version;
  int cpu;
}NSCDriverData;


static bool nscDrawLine(void *drv, void *dev, DFBRegion *line);
static bool nscFillRectangle(void *drv, void *dev, DFBRectangle *rect);
static bool nscDrawRectangle(void *drv, void *dev, DFBRectangle *rect);
static bool nscBlit(void *drv, void *dev, DFBRectangle *rect, int dx, int dy);

static void gxEngineSync(void *drv, void *dev)
{
   Gal_wait_until_idle();
}

static inline void
nsc_validate_srcColorkey(NSCDriverData *gxdrv,
                         NSCDeviceData *gxdev, CardState *state)
{
   if (gxdev->v_srcColorkey)
      return;
   gxdev->src_colorkey = state->src_colorkey;
   gxdev->v_srcColorkey = 1;
}

static void
gxCheckState(void *drv,
             void *dev, CardState *state, DFBAccelerationMask accel)
{
#if NSC_ACCEL
   NSCDriverData *gxdrv = (NSCDriverData *) drv;

   if(state->destination->format != DSPF_RGB16)
      return;

   if (DFB_BLITTING_FUNCTION(accel)) {

	   if(state->source->format != DSPF_RGB16)
		  return;
      /* if there are no other blitting flags than the supported
       * and the source and destination formats are the same 
       */
      if (gxdrv->cpu) {
         if (!(state->blittingflags & ~GX_SUPPORTED_BLITTINGFLAGS) &&
             state->source && state->source->format != DSPF_RGB24) {
            state->accel |= GX_SUPPORTED_BLITTINGFUNCTIONS;
         }
     }
   } else {
      /* if there are no other drawing flags than the supported */
      if (!(state->drawingflags & ~GX_SUPPORTED_DRAWINGFLAGS)) {
         state->accel |= GX_SUPPORTED_DRAWINGFUNCTIONS;
      }
   }
#endif /* NSC_ACCEL */
}

static void
gxSetState(void *drv, void *dev,
           GraphicsDeviceFuncs *funcs,
           CardState *state, DFBAccelerationMask accel)
{
   NSCDriverData *gxdrv = (NSCDriverData *) drv;
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;

   if (state->modified & SMF_SRC_COLORKEY)
      gxdev->v_srcColorkey = 0;

   switch (accel) {
   case DFXL_BLIT:
         state->set |= DFXL_BLIT;
         if (state->blittingflags & DSBLIT_SRC_COLORKEY)
            nsc_validate_srcColorkey(gxdrv, gxdev, state);
         break;

   case DFXL_FILLRECTANGLE:
   case DFXL_DRAWRECTANGLE:
   case DFXL_DRAWLINE:
      state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWLINE | DFXL_DRAWRECTANGLE;
      break;

   default:
      BUG("unexpected drawing/blitting function");
      break;
   }

   if (state->modified & SMF_DESTINATION) {

      /* set offset & pitch */

      gxdev->dst_offset = state->destination->back_buffer->video.offset;
      gxdev->dst_pitch = state->destination->back_buffer->video.pitch;
   }

   if (state->modified & SMF_SOURCE && state->source) {

      gxdev->src_offset = state->source->front_buffer->video.offset;
      gxdev->src_pitch = state->source->front_buffer->video.pitch;
   }

   if (state->modified & (SMF_DESTINATION | SMF_COLOR)) {
      switch (state->destination->format) {
      case DSPF_A8:
         gxdev->Color = state->color.a;
         break;
      case DSPF_RGB15:
         gxdev->Color =
               PIXEL_RGB15(state->color.r, state->color.g, state->color.b);
         break;
      case DSPF_RGB16:
         gxdev->Color =
               PIXEL_RGB16(state->color.r, state->color.g, state->color.b);
         break;

      default:
         BUG("unexpected pixelformat");
         break;
      }
   }

   state->modified = 0;
}

static bool
nscDrawLine(void *drv, void *dev, DFBRegion *line)
{
   long dx, dy, adx, ady;
   short majorErr;
   unsigned short destData;
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;

#if 0
   destData = GP_VECTOR_DEST_DATA;      /*  Value will be 0x8 (or) 0 */
#else
   destData = 0;                        /*  Value will be 0x8 (or) 0 */
#endif
   dx = line->x2 - line->x1;            /*  delta values */
   dy = line->y2 - line->y1;
   adx = ABS(dx);
   ady = ABS(dy);

   /* Canonical Bresenham stepper.
    * * We use hardware to draw the pixels to take care of alu modes 
    * * and whatnot.
    */
   Gal_set_raster_operation(0xF0);
   Gal_set_solid_pattern(gxdev->Color);
   if (adx >= ady) {
      unsigned short vectorMode;

      vectorMode = destData;
      if (dy >= 0)
         vectorMode |= GP_VECTOR_MINOR_AXIS_POS;
      if (dx >= 0)
         vectorMode |= GP_VECTOR_MAJOR_AXIS_POS;
      majorErr = (short)(ady << 1);

      Gal_bresenham_line((short)line->x1,
                         (short)line->y1,
                         (short)adx,
                         (short)(majorErr - adx),
                         (short)majorErr,
                         (short)(majorErr - (adx << 1)), vectorMode);
   } else {
      unsigned short vectorMode;

      vectorMode = destData | GP_VECTOR_Y_MAJOR;

      if (dx >= 0)
         vectorMode |= GP_VECTOR_MINOR_AXIS_POS;
      if (dy >= 0)
         vectorMode |= GP_VECTOR_MAJOR_AXIS_POS;
      majorErr = (short)(adx << 1);
      Gal_bresenham_line((short)line->x1,
                         (short)line->y1,
                         (short)ady,
                         (short)(majorErr - ady),
                         (short)majorErr,
                         (short)(majorErr - (ady << 1)), vectorMode);
   }

   return true;
}

static bool
nscFillRectangle(void *drv, void *dev, DFBRectangle *rect)
{
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;

   Gal_set_raster_operation(0xF0);
   Gal_set_solid_pattern(gxdev->Color);
   Gal_pattern_fill(rect->x, rect->y, rect->w, rect->h);

   return true;
}

static bool
nscDrawRectangle(void *drv, void *dev, DFBRectangle *rect)
{
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;

   Gal_set_raster_operation(0xF0);
   Gal_set_solid_pattern(gxdev->Color);
   Gal_pattern_fill(rect->x, rect->y, rect->w, 1);
   Gal_pattern_fill(rect->x, ((rect->y + rect->h) - 1), rect->w, 1);
   Gal_pattern_fill(rect->x, (rect->y + 1), 1, (rect->h - 2));
   Gal_pattern_fill(((rect->x + rect->w) - 1),
                    (rect->y + 1), 1, (rect->h - 2));

   return true;
}

static bool
nscBlit(void *drv, void *dev, DFBRectangle * rect, int dx, int dy)
{
   NSCDeviceData *nscdev = (NSCDeviceData *) dev;
   unsigned long soffset = (rect->x * nscdev->src_pitch) + (rect->y * 2);
   unsigned long doffset = (dy * nscdev->dst_pitch) + (dx * 2);

   Gal_set_solid_pattern(nscdev->Color);
   if (nscdev->v_srcColorkey) {
      Gal2_set_source_transparency(nscdev->src_colorkey, 0xFFFF);
   }
   Gal_set_raster_operation(0xCC);
   Gal2_set_source_stride((unsigned short)nscdev->src_pitch);
   Gal2_set_destination_stride(nscdev->dst_pitch);
   Gal2_screen_to_screen_blt(nscdev->src_offset + soffset,
                             nscdev->dst_offset + doffset,
                             (unsigned short)rect->w,
                             (unsigned short)rect->h, 1);

   return true;
}

/* exported symbols */

static int
driver_probe(GraphicsDevice *device)
{
#ifdef FB_ACCEL_NSC_GEODE
   if(dfb_gfxcard_get_accelerator(device) == FB_ACCEL_NSC_GEODE)
      return 1;
#endif
   return 0;
}

static void
driver_get_info(GraphicsDevice *device, GraphicsDriverInfo *info)
{
   /* fill driver info structure */
   snprintf(info->name,
            DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "NSC GX1 and GX2 Driver");
   snprintf(info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "NSC");

   info->version.major = 0;
   info->version.minor = 2;
   info->driver_data_size = sizeof(NSCDriverData);
   info->device_data_size = sizeof(NSCDeviceData);
}

static DFBResult
driver_init_driver(GraphicsDevice *device,
                   GraphicsDeviceFuncs *funcs, void *driver_data)
{
   NSCDriverData *gxdrv = (NSCDriverData *) driver_data;
   GAL_ADAPTERINFO sAdapterInfo;

   if (!Gal_initialize_interface())
      return DFB_IO;

   Gal_get_adapter_info(&sAdapterInfo);
   gxdrv->cpu_version = sAdapterInfo.dwCPUVersion;
   gxdrv->cpu = 0;
   if ((gxdrv->cpu_version & 0xFF) == GFX_CPU_REDCLOUD) {
      gxdrv->cpu = 1;
   }
   DEBUGMSG("CPU is GX%d", gxdrv->cpu);

#if NSC_ACCEL
   funcs->CheckState = gxCheckState;
   funcs->SetState = gxSetState;
   funcs->EngineSync = gxEngineSync;
   funcs->FillRectangle = nscFillRectangle;
   funcs->DrawLine = nscDrawLine;
   funcs->DrawRectangle = nscDrawRectangle;
   funcs->DrawLine = nscDrawLine;
   if (gxdrv->cpu) {
      funcs->Blit = nscBlit;
   }
#endif /* NSC_ACCEL */
   return DFB_OK;
}

static DFBResult
driver_init_device(GraphicsDevice *device,
                   GraphicsDeviceInfo *device_info,
                   void *driver_data, void *device_data)
{
   NSCDriverData *gxdrv = (NSCDriverData *) driver_data;

   snprintf(device_info->name,
            DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "GEODE GX1");
   snprintf(device_info->vendor,
            DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nsc");

   device_info->caps.flags = CCF_NOTRIEMU;
   device_info->caps.accel = GX_SUPPORTED_DRAWINGFUNCTIONS;
   device_info->caps.drawing = GX_SUPPORTED_DRAWINGFLAGS;
   if (gxdrv->cpu) {
      device_info->caps.accel |= GX_SUPPORTED_BLITTINGFUNCTIONS;
      device_info->caps.blitting = GX_SUPPORTED_BLITTINGFLAGS;
   }
   return DFB_OK;
}

static void
driver_close_device(GraphicsDevice * device,
                    void *driver_data, void *device_data)
{
   NSCDeviceData *gxdev = (NSCDeviceData *) device_data;

   (void)gxdev;
   DEBUGMSG("DirectFB/nsc: 5");
}

static void
driver_close_driver(GraphicsDevice *device, void *driver_data)
{
   DEBUGMSG("DirectFB/nsc: 6");
}
