/*
   Intel i830 DirectFB graphics driver

   (c) Copyright 2005       Servision Ltd.
                            http://www.servision.net/

   All rights reserved.

   Based on i810 driver written by Antonino Daplas <adaplas@pol.net>

   Video Overlay Support based partly on XFree86's "i830_video.c"

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

/*
 * i830_video.c: i830/i845 Xv driver.
 *
 * Copyright © 2002 by Alan Hourihane and David Dawes
 *
 * Authors:
 *      Alan Hourihane <alanh@tungstengraphics.com>
 *      David Dawes <dawes@xfree86.org>
 *
 * Derived from i830 Xv driver:
 *
 * Authors of i830 code:
 *      Jonathan Bian <jonathan.bian@intel.com>
 *      Offscreen Images:
 *        Matt Sottek <matthew.j.sottek@intel.com>
 */

#include <math.h>
#include <string.h>

#include <sys/ioctl.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/screens.h>
#include <core/screen.h>

#include <fbdev/fbdev.h>

#include <direct/mem.h>

#include <gfx/convert.h>

#include "i830.h"



#define I830_OVERLAY_SUPPORTED_OPTIONS (DLOP_DST_COLORKEY)

static void ovl_calc_regs( I830DriverData        *idrv,
                           I830DeviceData        *idev,
                           I830OverlayLayerData  *iovl,
                           CoreLayer             *layer,
                           CoreSurface           *surface,
                           CoreLayerRegionConfig *config,
                           bool                   buffers_only );


/*
 * This is more or less the correct way to initalise, update, and shut down
 * the overlay.  Note OVERLAY_OFF should be used only after disabling the
 * overlay in OCMD and calling OVERLAY_UPDATE.
 *
 * XXX Need to make sure that the overlay engine is cleanly shutdown in
 * all modes of server exit.
 */

static void
update_overlay( I830DriverData *idrv,
                I830DeviceData *idev )
{
     I830RingBlock block;

     i830_begin_lp_ring( idrv, idev, 6, &block );

     i830_out_ring( &block, MI_FLUSH | MI_WRITE_DIRTY_STATE );
     i830_out_ring( &block, MI_NOOP );

     if (!idev->overlayOn) {
          idev->overlayOn = true;

          i830_out_ring( &block, MI_NOOP );
          i830_out_ring( &block, MI_NOOP );
          i830_out_ring( &block, MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_ON );
     }
     else {
          i830_out_ring( &block, MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP );
          i830_out_ring( &block, MI_NOOP );
          i830_out_ring( &block, MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_CONTINUE );
     }

     i830_out_ring( &block, idev->ovl_mem.physical | 1 );

     i830_advance_lp_ring( idrv, idev, &block );
}

static void
disable_overlay( I830DriverData *idrv,
                 I830DeviceData *idev )
{
     I830RingBlock block;

     if (!idev->overlayOn)
          return;

     i830_begin_lp_ring( idrv, idev, 8, &block );

     i830_out_ring( &block, MI_FLUSH | MI_WRITE_DIRTY_STATE );
     i830_out_ring( &block, MI_NOOP );
     i830_out_ring( &block, MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP );
     i830_out_ring( &block, MI_NOOP );
     i830_out_ring( &block, MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_OFF );
     i830_out_ring( &block, idev->ovl_mem.physical | 1 );
     i830_out_ring( &block, MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP );
     i830_out_ring( &block, MI_NOOP );

     i830_advance_lp_ring( idrv, idev, &block );

     idev->overlayOn = false;
}

void
i830ovlOnOff( I830DriverData *idrv,
              I830DeviceData *idev,
              bool            on )
{
     if (on)
          idrv->oregs->OCMD |= OVERLAY_ENABLE;
     else
          idrv->oregs->OCMD &= ~OVERLAY_ENABLE;

     update_overlay( idrv, idev );

     if (!on)
          disable_overlay( idrv, idev );
}

static int
ovlLayerDataSize()
{
     return sizeof(I830OverlayLayerData);
}

static DFBResult
ovlInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     I830DriverData       *idrv = driver_data;
     I830DeviceData       *idev = idrv->idev;
     I830OverlayLayerData *iovl = layer_data;

     idev->iovl = iovl;

     idrv->oregs = (I830OverlayRegs*) idrv->ovl_base;

     memset( (void*) idrv->oregs, 0, sizeof(I830OverlayRegs) );

     /* set_capabilities */
     description->caps = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION |
                         DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST | DLCAPS_SATURATION |
                         DLCAPS_DST_COLORKEY;

     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Intel 830/845/855/865 Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;


     idrv->oregs->OCLRC0 = 64 << 18;
     idrv->oregs->OCLRC1 = 0x80;

     return DFB_OK;
}

static DFBResult
ovlTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = CLRCF_NONE;

     if (config->options & ~I830_OVERLAY_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_YUY2:
          case DSPF_UYVY:
               break;
          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width > 1440 || config->width < 1)
          fail |= CLRCF_WIDTH;

     if (config->height > 1023 || config->height < 1)
          fail |= CLRCF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
ovlSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     I830DriverData       *idrv = driver_data;
     I830DeviceData       *idev = idrv->idev;
     I830OverlayLayerData *iovl = layer_data;

     iovl->config = *config;

     ovl_calc_regs ( idrv, idev, iovl, layer, surface, config, false );

     i830ovlOnOff( idrv, idev, true );

     return DFB_OK;
}

static DFBResult
ovlRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     I830DriverData *idrv = driver_data;
     I830DeviceData *idev = idrv->idev;

     /* disable overlay */
     i830ovlOnOff( idrv, idev, false );

     return DFB_OK;
}

static DFBResult
ovlFlipRegion(  CoreLayer           *layer,
                void                *driver_data,
                void                *layer_data,
                void                *region_data,
                CoreSurface         *surface,
                DFBSurfaceFlipFlags  flags )
{
     I830DriverData       *idrv = driver_data;
     I830DeviceData       *idev = idrv->idev;
     I830OverlayLayerData *iovl = layer_data;

     dfb_surface_flip_buffers( surface, false );

     ovl_calc_regs ( idrv, idev, iovl, layer, surface, &iovl->config, true );

     update_overlay( idrv, idev );

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

     return DFB_OK;
}

static DFBResult
ovlSetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     I830DriverData *idrv = driver_data;
     I830DeviceData *idev = idrv->idev;
     __u16 b, c, s;

     if (adj->flags & DCAF_BRIGHTNESS)
	 b = ((adj->brightness >> 8) - 128) & 0xFF;
     else
	 b = idrv->oregs->OCLRC0 & 0xFF;

     if (adj->flags & DCAF_CONTRAST)
	 c = (adj->contrast >> 8) & 0xFF;
     else
	 c = (idrv->oregs->OCLRC0 >> 18) & 0xFF;

     if (adj->flags & DCAF_SATURATION)
	 s = (adj->saturation >> 8) & 0xFF;
     else
	 s = idrv->oregs->OCLRC1 & 0xFF;

     idrv->oregs->OCLRC0 = b | (c << 18);
     idrv->oregs->OCLRC1 = s;

     update_overlay( idrv, idev );

     return DFB_OK;
}

static DFBResult
ovlSetInputField( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data,
                  int        field )
{
     I830DriverData *idrv = driver_data;
     I830DeviceData *idev = idrv->idev;

     idrv->oregs->OCMD &= ~FIELD_SELECT;
     idrv->oregs->OCMD |= (field) ? FIELD1 : FIELD0;

     update_overlay( idrv, idev );

     return DFB_OK;
}

DisplayLayerFuncs i830OverlayFuncs = {
     LayerDataSize:      ovlLayerDataSize,
     InitLayer:          ovlInitLayer,
     TestRegion:         ovlTestRegion,
     SetRegion:          ovlSetRegion,
     RemoveRegion:       ovlRemoveRegion,
     FlipRegion:         ovlFlipRegion,
     SetColorAdjustment: ovlSetColorAdjustment,
     SetInputField:      ovlSetInputField

};


typedef struct {
     __u8 sign;
     __u16 mantissa;
     __u8 exponent;
} coeffRec, *coeffPtr;

static bool
SetCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos)
{
     int maxVal, icoeff, res;
     int sign;
     double c;

     sign = 0;
     maxVal = 1 << mantSize;
     c = *coeff;
     if (c < 0.0) {
          sign = 1;
          c = -c;
     }

     res = 12 - mantSize;
     if ((icoeff = (int)(c * 4 * maxVal + 0.5)) < maxVal) {
          pCoeff[pos].exponent = 3;
          pCoeff[pos].mantissa = icoeff << res;
          *coeff = (double)icoeff / (double)(4 * maxVal);
     }
     else if ((icoeff = (int)(c * 2 * maxVal + 0.5)) < maxVal) {
          pCoeff[pos].exponent = 2;
          pCoeff[pos].mantissa = icoeff << res;
          *coeff = (double)icoeff / (double)(2 * maxVal);
     }
     else if ((icoeff = (int)(c * maxVal + 0.5)) < maxVal) {
          pCoeff[pos].exponent = 1;
          pCoeff[pos].mantissa = icoeff << res;
          *coeff = (double)icoeff / (double)(maxVal);
     }
     else if ((icoeff = (int)(c * maxVal * 0.5 + 0.5)) < maxVal) {
          pCoeff[pos].exponent = 0;
          pCoeff[pos].mantissa = icoeff << res;
          *coeff = (double)icoeff / (double)(maxVal / 2);
     }
     else {
          /* Coeff out of range */
          return false;
     }

     pCoeff[pos].sign = sign;
     if (sign)
          *coeff = -(*coeff);
     return true;
}

static void
UpdateCoeff(int taps, double fCutoff, bool isHoriz, bool isY, coeffPtr pCoeff)
{
     int i, j, j1, num, pos, mantSize;
     double pi = 3.1415926535, val, sinc, window, sum;
     double rawCoeff[MAX_TAPS * 32], coeffs[N_PHASES][MAX_TAPS];
     double diff;
     int tapAdjust[MAX_TAPS], tap2Fix;
     bool isVertAndUV;

     if (isHoriz)
          mantSize = 7;
     else
          mantSize = 6;

     isVertAndUV = !isHoriz && !isY;
     num = taps * 16;
     for (i = 0; i < num  * 2; i++) {
          val = (1.0 / fCutoff) * taps * pi * (i - num) / (2 * num);
          if (val == 0.0)
               sinc = 1.0;
          else
               sinc = sin(val) / val;

          /* Hamming window */
          window = (0.5 - 0.5 * cos(i * pi / num));
          rawCoeff[i] = sinc * window;
     }

     for (i = 0; i < N_PHASES; i++) {
          /* Normalise the coefficients. */
          sum = 0.0;
          for (j = 0; j < taps; j++) {
               pos = i + j * 32;
               sum += rawCoeff[pos];
          }
          for (j = 0; j < taps; j++) {
               pos = i + j * 32;
               coeffs[i][j] = rawCoeff[pos] / sum;
          }

          /* Set the register values. */
          for (j = 0; j < taps; j++) {
               pos = j + i * taps;
               if ((j == (taps - 1) / 2) && !isVertAndUV)
                    SetCoeffRegs(&coeffs[i][j], mantSize + 2, pCoeff, pos);
               else
                    SetCoeffRegs(&coeffs[i][j], mantSize, pCoeff, pos);
          }

          tapAdjust[0] = (taps - 1) / 2;
          for (j = 1, j1 = 1; j <= tapAdjust[0]; j++, j1++) {
               tapAdjust[j1] = tapAdjust[0] - j;
               tapAdjust[++j1] = tapAdjust[0] + j;
          }

          /* Adjust the coefficients. */
          sum = 0.0;
          for (j = 0; j < taps; j++)
               sum += coeffs[i][j];
          if (sum != 1.0) {
               for (j1 = 0; j1 < taps; j1++) {
                    tap2Fix = tapAdjust[j1];
                    diff = 1.0 - sum;
                    coeffs[i][tap2Fix] += diff;
                    pos = tap2Fix + i * taps;
                    if ((tap2Fix == (taps - 1) / 2) && !isVertAndUV)
                         SetCoeffRegs(&coeffs[i][tap2Fix], mantSize + 2, pCoeff, pos);
                    else
                         SetCoeffRegs(&coeffs[i][tap2Fix], mantSize, pCoeff, pos);

                    sum = 0.0;
                    for (j = 0; j < taps; j++)
                         sum += coeffs[i][j];
                    if (sum == 1.0)
                         break;
               }
          }
     }
}

static void
ovl_calc_regs( I830DriverData        *idrv,
               I830DeviceData        *idev,
               I830OverlayLayerData  *iovl,
               CoreLayer             *layer,
               CoreSurface           *surface,
               CoreLayerRegionConfig *config,
               bool                   buffers_only )
{
     I830OverlayRegs *regs   = idrv->oregs;
     SurfaceBuffer   *front  = surface->front_buffer;
     int              width  = config->width;
     int              height = config->height;

     DFBSurfacePixelFormat primary_format;

     int y_offset, u_offset = 0, v_offset = 0;

     unsigned int swidth;


     /* Set buffer pointers */
     y_offset = dfb_gfxcard_memory_physical( NULL, front->video.offset );

     switch (config->format) {
          case DSPF_I420:
               u_offset = y_offset + height * front->video.pitch;
               v_offset = u_offset + ((height >> 1) * (front->video.pitch >> 1));
               break;

          case DSPF_YV12:
               v_offset = y_offset + height * front->video.pitch;
               u_offset = v_offset + ((height >> 1) * (front->video.pitch >> 1));
               break;

          case DSPF_UYVY:
          case DSPF_YUY2:
               break;

          default:
               D_BUG( "unexpected format" );
               return;
     }

     /* buffer locations */
     regs->OBUF_0Y = y_offset;
     regs->OBUF_0U = u_offset;
     regs->OBUF_0V = v_offset;

     //D_INFO("Buffers: Y0: 0x%08x, U0: 0x%08x, V0: 0x%08x\n", regs->OBUF_0Y,
     //       regs->OBUF_0U, regs->OBUF_0V);

     if (buffers_only)
          return;

     switch (config->format) {
          case DSPF_YV12:
          case DSPF_I420:
               swidth = (width + 1) & ~1 & 0xfff;
               regs->SWIDTH = swidth;

               swidth /= 2;
               regs->SWIDTH |= (swidth & 0x7ff) << 16;

               swidth = ((y_offset + width + 0x1f) >> 5) - (y_offset >> 5) - 1;

               //D_INFO("Y width is %d, swidthsw is %d\n", width, swidth);

               regs->SWIDTHSW = swidth << 2;

               swidth = ((u_offset + (width / 2) + 0x1f) >> 5) - (u_offset >> 5) - 1;
               //D_INFO("UV width is %d, swidthsw is %d\n", width / 2, swidth);

               regs->SWIDTHSW |= swidth << 18;
               break;

          case DSPF_UYVY:
          case DSPF_YUY2:
               /* XXX Check for i845 */

               swidth = ((width + 31) & ~31) << 1;
               regs->SWIDTH = swidth;
               regs->SWIDTHSW = swidth >> 3;
               break;

          default:
               D_BUG( "unexpected format" );
               return;
     }

     regs->SHEIGHT = height | ((height / 2) << 16);

#if NOT_PORTED_YET
     if (pPriv->oneLineMode) {
          /* change the coordinates with panel fitting active */
          dstBox->y1 = (((dstBox->y1 - 1) * pPriv->scaleRatio) >> 16) + 1;
          dstBox->y2 = ((dstBox->y2 * pPriv->scaleRatio) >> 16) + 1;

          /* Now, alter the height, so we scale to the correct size */
          drw_h = dstBox->y2 - dstBox->y1;
          if (drw_h < height) drw_h = height;
     }
#endif

     regs->DWINPOS = (config->dest.y << 16) | config->dest.x;
     regs->DWINSZ  = (config->dest.h << 16) | config->dest.w;

     //D_INFO("pos: 0x%08x, size: 0x%08x\n", regs->DWINPOS, regs->DWINSZ);


     regs->OCMD    = OVERLAY_ENABLE;
     regs->OCONFIG = CC_OUT_8BIT;

     /*
      * Calculate horizontal and vertical scaling factors and polyphase
      * coefficients.
      */

     {
          bool scaleChanged = false;
          int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
          int xscaleIntUV, xscaleFractUV;
          int yscaleIntUV, yscaleFractUV;
          /* UV is half the size of Y -- YUV420 */
          int uvratio = 2;
          __u32 newval;
          coeffRec xcoeffY[N_HORIZ_Y_TAPS * N_PHASES];
          coeffRec xcoeffUV[N_HORIZ_UV_TAPS * N_PHASES];
          int i, j, pos;

          /*
           * Y down-scale factor as a multiple of 4096.
           */
          xscaleFract = (config->source.w << 12) / config->dest.w;
          yscaleFract = (config->source.h << 12) / config->dest.h;

          /* Calculate the UV scaling factor. */
          xscaleFractUV = xscaleFract / uvratio;
          yscaleFractUV = yscaleFract / uvratio;

          /*
           * To keep the relative Y and UV ratios exact, round the Y scales
           * to a multiple of the Y/UV ratio.
           */
          xscaleFract = xscaleFractUV * uvratio;
          yscaleFract = yscaleFractUV * uvratio;

          /* Integer (un-multiplied) values. */
          xscaleInt = xscaleFract >> 12;
          yscaleInt = yscaleFract >> 12;

          xscaleIntUV = xscaleFractUV >> 12;
          yscaleIntUV = yscaleFractUV >> 12;

          //D_INFO("xscale: 0x%x.%03x, yscale: 0x%x.%03x\n", xscaleInt,
          //       xscaleFract & 0xFFF, yscaleInt, yscaleFract & 0xFFF);
          //D_INFO("UV xscale: 0x%x.%03x, UV yscale: 0x%x.%03x\n", xscaleIntUV,
          //       xscaleFractUV & 0xFFF, yscaleIntUV, yscaleFractUV & 0xFFF);

          newval = (xscaleInt << 16) |
                   ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
          if (newval != regs->YRGBSCALE) {
               scaleChanged = true;
               regs->YRGBSCALE = newval;
          }

          newval = (xscaleIntUV << 16) | ((xscaleFractUV & 0xFFF) << 3) |
                   ((yscaleFractUV & 0xFFF) << 20);
          if (newval != regs->UVSCALE) {
               scaleChanged = true;
               regs->UVSCALE = newval;
          }

          newval = yscaleInt << 16 | yscaleIntUV;
          if (newval != regs->UVSCALEV) {
               scaleChanged = true;
               regs->UVSCALEV = newval;
          }

          /* Recalculate coefficients if the scaling changed. */

          /*
           * Only Horizontal coefficients so far.
           */
          if (scaleChanged) {
               double fCutoffY;
               double fCutoffUV;

               fCutoffY = xscaleFract / 4096.0;
               fCutoffUV = xscaleFractUV / 4096.0;

               /* Limit to between 1.0 and 3.0. */
               if (fCutoffY < MIN_CUTOFF_FREQ)
                    fCutoffY = MIN_CUTOFF_FREQ;
               if (fCutoffY > MAX_CUTOFF_FREQ)
                    fCutoffY = MAX_CUTOFF_FREQ;
               if (fCutoffUV < MIN_CUTOFF_FREQ)
                    fCutoffUV = MIN_CUTOFF_FREQ;
               if (fCutoffUV > MAX_CUTOFF_FREQ)
                    fCutoffUV = MAX_CUTOFF_FREQ;

               UpdateCoeff(N_HORIZ_Y_TAPS, fCutoffY, true, true, xcoeffY);
               UpdateCoeff(N_HORIZ_UV_TAPS, fCutoffUV, true, false, xcoeffUV);

               for (i = 0; i < N_PHASES; i++) {
                    for (j = 0; j < N_HORIZ_Y_TAPS; j++) {
                         pos = i * N_HORIZ_Y_TAPS + j;
                         regs->Y_HCOEFS[pos] = xcoeffY[pos].sign << 15 |
                                               xcoeffY[pos].exponent << 12 |
                                               xcoeffY[pos].mantissa;
                    }
               }
               for (i = 0; i < N_PHASES; i++) {
                    for (j = 0; j < N_HORIZ_UV_TAPS; j++) {
                         pos = i * N_HORIZ_UV_TAPS + j;
                         regs->UV_HCOEFS[pos] = xcoeffUV[pos].sign << 15 |
                                                xcoeffUV[pos].exponent << 12 |
                                                xcoeffUV[pos].mantissa;
                    }
               }
          }
     }

     switch (config->format) {
          case DSPF_YV12:
          case DSPF_I420:
               //D_INFO("YUV420\n");
#if 0
               /* set UV vertical phase to -0.25 */
               regs->UV_VPH = 0x30003000;
#endif

               regs->OSTRIDE = front->video.pitch | (front->video.pitch << 15);
               regs->OCMD |= YUV_420;
               break;

          case DSPF_UYVY:
          case DSPF_YUY2:
               //D_INFO("YUV422\n");

               regs->OSTRIDE = front->video.pitch;
               regs->OCMD |= YUV_422;

               if (config->format == DSPF_UYVY)
                    regs->OCMD |= Y_SWAP;
               break;

          default:
               D_BUG( "unexpected format" );
               return;
     }


     /*
      * Destination color keying.
      */

     primary_format = dfb_primary_layer_pixelformat();

     regs->DCLRKV = dfb_color_to_pixel( primary_format,
                                        config->dst_key.r, config->dst_key.g, config->dst_key.b );

     regs->DCLRKM = (1 << DFB_COLOR_BITS_PER_PIXEL( primary_format )) - 1;

     if (config->options & DLOP_DST_COLORKEY)
          regs->DCLRKM |= DEST_KEY_ENABLE;


     //D_INFO("OCMD is 0x%08x\n", regs->OCMD);
}

