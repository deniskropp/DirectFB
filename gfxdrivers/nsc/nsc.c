/*
 * $Workfile: $
 * $Revision: 1.11 $
 *
 * File Contents: This file contains the main functions of the NSC DFB.
 *
 * Project:       NSC Direct Frame buffer device driver
 *
 */

/* NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software
 * ("Software"):
 *
 * National Xfree frame buffer driver
 *
 * under one of the two following licenses, depending on how the
 * Software is received by the Licensee.
 *
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated
 * NSC_LIC_GPL applies to this Software; in all other circumstances
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for
 *
 * National Xfree frame buffer driver
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of the National Semiconductor Corporation nor
 *     the names of its contributors may be used to endorse or promote
 *     products derived from this software without specific prior
 *     written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF
 * YOUR JURISDICTION. It is licensee's responsibility to comply with
 * any export regulations applicable in licensee's jurisdiction. Under
 * CURRENT (2001) U.S. export regulations this software
 * is eligible for export from the U.S. and can be downloaded by or
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran,
 * Syria, Sudan, Afghanistan and any other country to which the U.S.
 * has embargoed goods and services.
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for
 *
 * National Xfree frame buffer driver
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted under the terms of the GNU General
 * Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * In addition to the terms of the GNU General Public License, neither
 * the name of the National Semiconductor Corporation nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE. See the GNU General Public License for more details.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF
 * YOUR JURISDICTION. It is licensee's responsibility to comply with
 * any export regulations applicable in licensee's jurisdiction. Under
 * CURRENT (2001) U.S. export regulations this software
 * is eligible for export from the U.S. and can be downloaded by or
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran,
 * Syria, Sudan, Afghanistan and any other country to which the U.S.
 * has embargoed goods and services.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * END_NSC_LIC_GPL */

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
#include <misc/util.h>
#include <core/graphics_driver.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "nsc_galproto.h"

#define NSC_ACCEL 1

#define GP_VECTOR_DEST_DATA       0x8
#define GP_VECTOR_MINOR_AXIS_POS  0x4
#define GP_VECTOR_MAJOR_AXIS_POS  0x2
#define GP_VECTOR_Y_MAJOR         0x1

#define  GFX_CPU_REDCLOUD 3

#define GX_SUPPORTED_DRAWINGFLAGS DSDRAW_NOFX

#define GX_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | \
                DFXL_DRAWRECTANGLE | \
                DFXL_DRAWLINE)

#define GX_SUPPORTED_BLITTINGFUNCTIONS DFXL_BLIT

#define GX_SUPPORTED_BLITTINGFLAGS DSBLIT_SRC_COLORKEY

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

static GAL_ADAPTERINFO sAdapterInfo;

static bool nscDrawLine(void *drv, void *dev, DFBRegion *line);
static bool nscFillRectangle(void *drv, void *dev, DFBRectangle *rect);
static bool nscDrawRectangle(void *drv, void *dev, DFBRectangle *rect);
static bool nscBlit(void *drv, void *dev, DFBRectangle *rect, int dx, int dy);
static bool nscBlitGu1(void *drv, void *dev, DFBRectangle *rect, int dx, int dy);

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
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;

   if(state->destination->format != DSPF_RGB16)
      return;

   if (DFB_BLITTING_FUNCTION(accel)) {

	   if(state->source->format != DSPF_RGB16)
		  return;
      if (gxdrv->cpu) {
         /* GU2 - if there are no other blitting flags than the supported
          * and the source and destination formats are the same 
          */
         if (!(state->blittingflags & ~GX_SUPPORTED_BLITTINGFLAGS) &&
             state->source && state->source->format != DSPF_RGB24) {
            state->accel |= GX_SUPPORTED_BLITTINGFUNCTIONS;
         }
      } else{
         /* GU1 - source width must match frame buffer strid
          */
         if(state->source) {
            int src_pitch = 0;
            int dst_pitch = 0;

            if(state->source) {
               src_pitch = state->source->width * DFB_BYTES_PER_PIXEL(state->source->format);
            }

            if (state->modified & SMF_DESTINATION) {
               if(state->destination && state->destination->front_buffer)
                  dst_pitch = state->destination->back_buffer->video.pitch;
            }
            if(dst_pitch == 0) {
               dst_pitch = gxdev->dst_pitch;
            }
       
            if(src_pitch == dst_pitch && state->source) {
               state->accel |= GX_SUPPORTED_BLITTINGFUNCTIONS;
            }
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
      D_BUG("unexpected drawing/blitting function");
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
      case DSPF_ARGB1555:
         gxdev->Color =
               PIXEL_ARGB1555(state->color.a, state->color.r,
                              state->color.g, state->color.b);
         break;
      case DSPF_RGB16:
         gxdev->Color =
               PIXEL_RGB16(state->color.r, state->color.g, state->color.b);
         break;

      default:
         D_BUG("unexpected pixelformat");
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
   int yoffset;

   destData = 0;                        /*  Value will be 0x8 (or) 0 */
   dx = line->x2 - line->x1;            /*  delta values */
   dy = line->y2 - line->y1;
   adx = ABS(dx);
   ady = ABS(dy);
   yoffset = gxdev->dst_offset / gxdev->dst_pitch;

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
                         (short)line->y1 + yoffset,
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
                         (short)line->y1 + yoffset,
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
   int yoffset;

   Gal_set_raster_operation(0xF0);
   Gal_set_solid_pattern(gxdev->Color);

   yoffset = gxdev->dst_offset / gxdev->dst_pitch;
   Gal_pattern_fill(rect->x, rect->y + yoffset, rect->w, rect->h);

   return true;
}

static bool
nscDrawRectangle(void *drv, void *dev, DFBRectangle *rect)
{
   NSCDeviceData *gxdev = (NSCDeviceData *) dev;
   int yoffset;

   Gal_set_raster_operation(0xF0);
   Gal_set_solid_pattern(gxdev->Color);

   yoffset = gxdev->dst_offset / gxdev->dst_pitch;

   Gal_pattern_fill(rect->x, rect->y + yoffset, rect->w, 1);
   Gal_pattern_fill(rect->x, ((rect->y + yoffset + rect->h) - 1), rect->w, 1);
   Gal_pattern_fill(rect->x, (rect->y + yoffset + 1), 1, (rect->h - 2));
   Gal_pattern_fill(((rect->x + rect->w) - 1),
                    (rect->y + yoffset + 1), 1, (rect->h - 2));

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

static bool
nscBlitGu1(void *drv, void *dev, DFBRectangle * rect, int dx, int dy)
{ 
   int result, yoff;

   NSCDeviceData *nscdev = (NSCDeviceData *) dev;

   Gal_set_solid_pattern(nscdev->Color);
   if (nscdev->v_srcColorkey) {
//FIXME     Gal_set_source_transparency(nscdev->src_colorkey, 0xFFFF);
   }
#if 0
   printf("rect x %d y %d w %d h %d dx %d dy %d src_off %x dst_off %x src pitch %x dst pitch %x\n",
		rect->x, rect->y, rect->w, rect->h, dx, dy,
		nscdev->src_offset, nscdev->dst_offset,
		nscdev->src_pitch, nscdev->dst_pitch);
#endif

   Gal_set_raster_operation(0xCC);
   
   yoff = nscdev->src_offset / nscdev->src_pitch;
   result = Gal_screen_to_screen_blt(rect->x, rect->y + yoff, dx, dy,
                             (unsigned short)rect->w,
                             (unsigned short)rect->h);

   return true;
}

/* exported symbols */

static int
driver_probe(GraphicsDevice *device)
{
   Gal_initialize_interface();
   if(Gal_get_adapter_info(&sAdapterInfo))
      return 1;
   return 0;
}

static void
driver_get_info(GraphicsDevice *device, GraphicsDriverInfo *info)
{
   /* fill driver info structure */
   snprintf(info->name,
            DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "NSC GX1 and GX2 Driver");
   snprintf(info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "NSC");

   info->version.major = 1;
   info->version.minor = 1;
   info->driver_data_size = sizeof(NSCDriverData);
   info->device_data_size = sizeof(NSCDeviceData);
}

static DFBResult
driver_init_driver(GraphicsDevice      *device,
                   GraphicsDeviceFuncs *funcs,
                   void                *driver_data,
                   void                *device_data)
{
   NSCDriverData *gxdrv = (NSCDriverData *) driver_data;

   Gal_set_compression_enable(0);

   gxdrv->cpu_version = sAdapterInfo.dwCPUVersion;
   gxdrv->cpu = 0;
   if ((gxdrv->cpu_version & 0xFF) == GFX_CPU_REDCLOUD) {
      gxdrv->cpu = 1;
   }
   D_DEBUG("CPU is GX%d", gxdrv->cpu);

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
   } else {
      funcs->Blit = nscBlitGu1;
   }
#endif /* NSC_ACCEL */

    /*dfb_config->pollvsync_after = 1;*/

   return DFB_OK;
}

static DFBResult
driver_init_device(GraphicsDevice *device,
                   GraphicsDeviceInfo *device_info,
                   void *driver_data, void *device_data)
{
   snprintf(device_info->name,
            DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "NSC GX1/GX2 driver version");
   snprintf(device_info->vendor,
            DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nsc");
   printf("Dependent NSC Kernel FrameBuffer driver version is 2.7.7 or later\n");
   device_info->caps.flags = CCF_NOTRIEMU;
   device_info->caps.accel = GX_SUPPORTED_DRAWINGFUNCTIONS;
   device_info->caps.drawing = GX_SUPPORTED_DRAWINGFLAGS;
   device_info->caps.accel |= GX_SUPPORTED_BLITTINGFUNCTIONS;
   device_info->caps.blitting = GX_SUPPORTED_BLITTINGFLAGS;
   return DFB_OK;
}

static void
driver_close_device(GraphicsDevice * device,
                    void *driver_data, void *device_data)
{
   NSCDeviceData *gxdev = (NSCDeviceData *) device_data;

   (void)gxdev;
   D_DEBUG("DirectFB/nsc: 5");
}

static void
driver_close_driver(GraphicsDevice *device, void *driver_data)
{
   D_DEBUG("DirectFB/nsc: 6");
}
