/*
   TI Davinci driver - Video Layer

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>

#include <stdio.h>
#include <sys/ioctl.h>

#include <directfb.h>
#include <directfb_util.h>

#include <core/layers.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include "davincifb.h"

#include "davinci_gfxdriver.h"
#include "davinci_video.h"


#define D_VIDERROR(x...) do {} while (0)


D_DEBUG_DOMAIN( Davinci_Video, "Davinci/Video", "TI Davinci Video" );

/**********************************************************************************************************************/

static DFBResult ShowBuffer( DavinciDriverData     *ddrv,
                             DavinciVideoLayerData *dvid,
                             CoreSurfaceBufferLock *lock,
                             const DFBRectangle    *area,
                             DFBSurfaceFlipFlags    flags );

static void SetupResizerParams( vpfe_resizer_params_t *params,
                                int srcWidth, int srcHeight,
                                int outWidth, int outHeight,
                                int *ret_outWidth,
                                int *ret_outHeight );

/**********************************************************************************************************************/

static int
videoLayerDataSize()
{
     return sizeof(DavinciVideoLayerData);
}

static DFBResult
videoInitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     int                    ret;
     DavinciDriverData     *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     /* Initialize with configuration from VID0 to start with a fullscreen (unscaled) layer */
     ret = ioctl( ddrv->fb[VID0].fd, FBIOGET_VSCREENINFO, &dvid->var );
     if (ret) {
          D_PERROR( "Davinci/Video: FBIOGET_VSCREENINFO (fb%d) failed!\n", VID0 );
          return DFB_INIT;
     }

     /* Disable VID0 (unused) */
     ret = ioctl( ddrv->fb[VID0].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_VIDERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID0, 0 );

     /* Disable VID1 (our layer) */
     ret = ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_VIDERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "TI Davinci Video" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = dvid->var.xres;
     config->height      = dvid->var.yres;
     config->pixelformat = DSPF_UYVY;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     return DFB_OK;
}

static DFBResult
videoTestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Davinci_Video, config );

     if (config->options & ~DAVINCI_VIDEO_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_UYVY:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width  < 8 || config->width  > 1920)
          fail |= CLRCF_WIDTH;

     if (config->height < 8 || config->height > 1080)
          fail |= CLRCF_HEIGHT;

     if (config->dest.x < 0 || config->dest.y < 0)
          fail |= CLRCF_DEST;

     if (config->dest.x + config->dest.w > 1920)
          fail |= CLRCF_DEST;

     if (config->dest.y + config->dest.h > 1080)
          fail |= CLRCF_DEST;

     if (failed)
          *failed = fail;

     if (fail) {
          D_DEBUG_AT( Davinci_Video, "  -> FAILED (0x%08x)\n", fail );
          return DFB_UNSUPPORTED;
     }

     D_DEBUG_AT( Davinci_Video, "  -> OK\n" );

     return DFB_OK;
}

static DFBResult
videoSetRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                void                       *region_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags  updated,
                CoreSurface                *surface,
                CorePalette                *palette,
                CoreSurfaceBufferLock      *lock )
{
     int                    ret;
     DavinciDriverData     *ddrv = driver_data;
     DavinciDeviceData     *ddev = ddrv->ddev;
     DavinciVideoLayerData *dvid = layer_data;
     CoreLayerRegionConfig *old  = &dvid->config;

     D_DEBUG_AT( Davinci_Video, "%s( updated 0x%08x, surface %p )\n", __FUNCTION__, updated, surface );

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Davinci_Video, config );

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( dvid != NULL );

     /* Update output size? */
     if ((updated & CLRCF_DEST) && (config->dest.w != old->dest.w || config->dest.h != old->dest.h)) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_Video, "  => dest    %4dx%4d\n", config->dest.w, config->dest.h );

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
          if (ret)
               D_VIDERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

          dvid->enabled = false;

/*********************************** Start workaround ***********************************/
          win_pos.xpos = 0;
          win_pos.ypos = 0;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_VIDERROR( "Davinci/Video: FBIO_SETPOS (fb%d - %d,%d) failed!\n", VID1, win_pos.xpos, win_pos.ypos );

          dvid->var.yoffset = 0;
/*********************************** End workaround ***********************************/

          /* Set output width and height. */
          dvid->var.xres = config->dest.w;
          dvid->var.yres = config->dest.h;

          dvid->var.yres_virtual = ddrv->fb[VID1].size / lock->pitch;

          ret = ioctl( ddrv->fb[VID1].fd, FBIOPUT_VSCREENINFO, &dvid->var );
          if (ret)
               D_PERROR( "Davinci/Video: FBIOPUT_VSCREENINFO (fb%d) failed!\n", VID1 );

          /* Read back new pitch etc. */
          ret = ioctl( ddrv->fb[VID1].fd, FBIOGET_FSCREENINFO, &ddev->fix[VID1] );
          if (ret)
               D_PERROR( "Davinci/Video: FBIOGET_FSCREENINFO (fb%d) failed!\n", VID1 );
     }

     /* Update output position? */
     if (updated & CLRCF_DEST) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_Video, "  => dest    %4d,%4d\n", config->dest.x, config->dest.y );

          if (dvid->enabled)
               ioctl( ddrv->fb[VID1].fd, FBIO_WAITFORVSYNC );

          /* Set horizontal and vertical offset. */
          win_pos.xpos = config->dest.x;
          win_pos.ypos = config->dest.y;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_VIDERROR( "Davinci/Video: FBIO_SETPOS (fb%d - %d,%d) failed!\n", VID1, config->dest.x, config->dest.y );
     }

     /* Update format? */
     if (updated & CLRCF_FORMAT) {
          vpbe_video_config_params_t params;

          params.cb_cr_order = (config->format == DSPF_YUY2) ? 1 : 0;

          params.exp_info.horizontal = VPBE_DISABLE;
          params.exp_info.vertical   = VPBE_DISABLE;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SET_VIDEO_CONFIG_PARAMS, &params );
          if (ret)
               D_VIDERROR( "Davinci/Video: FBIO_SET_VIDEO_CONFIG_PARAMS (fb%d - %s) failed!\n",
                         VID1, params.cb_cr_order ? "CrCb" : "CbCr" );
     }

     /* Update scaling parameters? */
     if ((updated & (CLRCF_SOURCE | CLRCF_DEST)) &&
         (config->source.w != old->source.w || config->source.h != old->source.h ||
          config->dest.w   != old->dest.w   || config->dest.h   != old->dest.h) &&
         (config->dest.w != config->source.w || config->dest.h != config->source.h))
     {
          D_DEBUG_AT( Davinci_Video, "  => scaling %4dx%4d -> %4dx%4d\n",
                      config->source.w, config->source.h, config->dest.w, config->dest.h );

          SetupResizerParams( &dvid->resizer, config->source.w, config->source.h,
                              config->dest.w, config->dest.h, &dvid->resized.w, &dvid->resized.h );

          dvid->offset.x = (config->dest.w - dvid->resized.w) / 2;
          dvid->offset.y = (config->dest.h - dvid->resized.h) / 2;

          D_DEBUG_AT( Davinci_Video, "  => resized %4dx%4d, centered %d,%d\n",
                      dvid->resized.w, dvid->resized.h, dvid->offset.x, dvid->offset.y );

          dvid->offset.x += dvid->offset.x & 1; /* Round up to multiple of two */

          D_DEBUG_AT( Davinci_Video, "  => offset  %4d,%4d\n", dvid->offset.x, dvid->offset.y );

          davincifb_pan_display( &ddrv->fb[VID1], &dvid->var, NULL, DSFLIP_NONE, 0, 0 );
     }

     dvid->enable = true;
     dvid->config = *config;

     return DFB_OK;
}

static DFBResult
videoRemoveRegion( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   void      *region_data )
{
     int                    ret;
     DavinciDriverData     *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     D_ASSERT( ddrv != NULL );

     ret = ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_VIDERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

     dvid->enabled = false;
     dvid->enable  = false;

     return DFB_OK;
}

static DFBResult
videoFlipRegion( CoreLayer             *layer,
                 void                  *driver_data,
                 void                  *layer_data,
                 void                  *region_data,
                 CoreSurface           *surface,
                 DFBSurfaceFlipFlags    flags,
                 CoreSurfaceBufferLock *lock )
{
     DFBResult              ret;
     DavinciDriverData     *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( dvid != NULL );

     D_DEBUG_AT( Davinci_Video, "%s( 0x%08lx [%d] 0x%04x [%4dx%4d] )\n", __FUNCTION__,
                 lock->phys, lock->pitch, flags, dvid->config.width, dvid->config.height );

     ret = ShowBuffer( ddrv, dvid, lock, NULL, flags );
     if (ret)
          return ret;

     dfb_surface_flip( surface, false );

     return DFB_OK;
}

static DFBResult
videoUpdateRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   const DFBRegion       *update,
                   CoreSurfaceBufferLock *lock )
{
     DavinciDriverData     *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( dvid != NULL );

     if (update) {
          DFBRectangle area = DFB_RECTANGLE_INIT_FROM_REGION( update );

          D_DEBUG_AT( Davinci_Video, "%s( 0x%08lx [%d], %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                      lock->phys, lock->pitch, DFB_RECTANGLE_VALS( &area ) );

          if (!dfb_rectangle_intersect( &area, &dvid->config.source )) {
               D_DEBUG_AT( Davinci_Video, "  -> NO INTERSECTION with %4d,%4d-%4dx%4d\n",
                           DFB_RECTANGLE_VALS( &dvid->config.source ) );

               return DFB_OK;
          }

          if (!DFB_RECTANGLE_EQUAL( area, dvid->config.source ))
               return ShowBuffer( ddrv, dvid, lock, &area, DSFLIP_NONE );
     }
     else
          D_DEBUG_AT( Davinci_Video, "%s( 0x%08lx [%d], %4dx%4d )\n", __FUNCTION__,
                      lock->phys, lock->pitch, dvid->config.width, dvid->config.height );

     return ShowBuffer( ddrv, dvid, lock, NULL, DSFLIP_NONE );
}

const DisplayLayerFuncs davinciVideoLayerFuncs = {
     LayerDataSize:      videoLayerDataSize,
     InitLayer:          videoInitLayer,

     TestRegion:         videoTestRegion,
     SetRegion:          videoSetRegion,
     RemoveRegion:       videoRemoveRegion,
     FlipRegion:         videoFlipRegion,
     UpdateRegion:       videoUpdateRegion
};

/***********************************************************************************************************************
** Frame Output
*/

static void
enable_video( DavinciDriverData     *ddrv,
              DavinciVideoLayerData *dvid )
{
     if (dvid->enable && !dvid->enabled) {
          ioctl( ddrv->fb[VID1].fd, FBIO_WAITFORVSYNC );

          if (ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 1 ))
               D_VIDERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 1 );

          dvid->enabled = true;
     }
}

static DFBResult
ShowBuffer( DavinciDriverData     *ddrv,
            DavinciVideoLayerData *dvid,
            CoreSurfaceBufferLock *lock,
            const DFBRectangle    *area,
            DFBSurfaceFlipFlags    flags )
{
     const CoreLayerRegionConfig *config = &dvid->config;

     if (area)
          D_DEBUG_AT( Davinci_Video, "%s( 0x%08lx [%d], %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                      lock->phys, lock->pitch, DFB_RECTANGLE_VALS( area ) );
     else
          D_DEBUG_AT( Davinci_Video, "%s( 0x%08lx [%d] )\n", __FUNCTION__, lock->phys, lock->pitch );

     if (config->dest.w == config->source.w && config->dest.h == config->source.h) {
          /*
           * Unscaled video, buffer displayed directly
           */
          D_DEBUG_AT( Davinci_Video, "  -> unscaled %4dx%4d <- %4d,%4d [%4dx%4d]\n",
                      config->source.w, config->source.h, config->source.x, config->source.y,
                      config->width, config->height );

          /* Partial update, assuming proper buffer is shown, saving system calls */
          if (area && dvid->enabled)
               return DFB_OK;
               
          davincifb_pan_display( &ddrv->fb[VID1], &dvid->var, lock, flags, config->source.x, config->source.y );
     }
     else {
          int                    ret;
          DavinciDeviceData     *ddev   = ddrv->ddev;
          CoreSurfaceBuffer     *buffer = lock->buffer;
          vpfe_resizer_params_t *params = &dvid->resizer;

          /*
           * Scaled video, buffer scaled to output buffer by resizer
           */
          D_DEBUG_AT( Davinci_Video, "  -> scaled %4dx%4d -> %4dx%4d <- %4d,%4d [%4dx%4d]\n",
                      config->source.w, config->source.h, config->dest.w, config->dest.h,
                      config->source.x, config->source.y, config->width, config->height );

          /* FIXME: Implement scaled partial updates! */
          if (area)
               D_UNIMPLEMENTED();

          params->sdr_inoff = lock->pitch;
          params->sdr_inadd = lock->phys + DFB_BYTES_PER_LINE( buffer->format, config->source.x )
                                         + config->source.y * params->sdr_inoff;

          params->sdr_outoff = ddev->fix[VID1].line_length;
          params->sdr_outadd = ddev->fix[VID1].smem_start + dvid->offset.x * 2
                                                          + dvid->offset.y * params->sdr_outoff;

          params->in_start    = (params->sdr_outadd & 0x1f) / 2;
          params->sdr_outadd &= ~0x1f;

          D_DEBUG_AT( Davinci_Video, "  -> FBIO_RESIZER running...\n" );

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_RESIZER, params );
          if (ret)
               D_VIDERROR( "Davinci/Video: FBIO_RESIZER (fb%d)!\n", VID1 );

          D_DEBUG_AT( Davinci_Video, "  => FBIO_RESIZER returned %d\n", ret );
     }

     enable_video( ddrv, dvid );

     return DFB_OK;
}

/***********************************************************************************************************************
** Scaling Setup
*/

static int
limitInput(int rsz,int inSize,int outSize,int* pInSize)
{
     int phases;
     int phaseShift;
     int taps;
     int phaseMask;
     int coarseShift;
     int halfCoarse;
     int tmp;

     do {
          if (rsz<=512) {
               //1/2x to 4x resize uses 8 phase, 4 taps
               phaseShift = 3;
               taps = 4;
          }
          else {
               //4-phase, 7 taps
               phaseShift = 2;
               taps = 7;
          }
          phases = 1<<phaseShift;
          phaseMask = phases-1;
          coarseShift = (8-phaseShift);
          halfCoarse = (1<<(coarseShift-1));
          tmp = (((outSize-1)* rsz + halfCoarse)>>8) + taps;
          if (tmp <= inSize) break;
          rsz--;
     } while (1);

     *pInSize = tmp;

     return rsz;
}

static void
SetupCoef(unsigned int* pCoef,int rsz)
{
     int startCoef;
     int highCoef;
     int c;
     int phases;
     int taps;
     if (rsz<=512) {
          //1/2x to 4x resize uses 8 phase, 4 taps
          highCoef = 0x100;
          c=1;
          phases=8;
          taps=4;
     }
     else {
          //4-phase, 7 taps
          if (rsz<=(256*3)) {
               highCoef = 0x100/2; c=2;
          }
          else {
               highCoef = 0x100/4; c=1;
          }
          phases=4;
          taps=7;
     }
     startCoef = highCoef>>1;
     while (phases) {
          int prev = startCoef;
          int tapNum=0;
          int rem=256 - startCoef;
          while ( tapNum < (c-1)) {
               *pCoef++ = 0;
               tapNum+=2;
          }
          if (c&1) {
               *pCoef++ = prev<<16;
               tapNum+=2;
          }
          else {
               tapNum++;
          }
          while ( tapNum < taps) {
               int min = (rem<highCoef)? rem : highCoef;
               if (tapNum&1) *pCoef++ = (min<<16)+prev;
               else prev = min;
               rem -= min;
               tapNum++;
          }
          if (tapNum&1) {
               *pCoef++ = prev;
               tapNum++;
          }
          while ( tapNum < taps) {
               *pCoef++ = 0;
               tapNum+=2;
          }
          if (startCoef > (highCoef>>3)) startCoef -= (highCoef>>3);
          else {
               startCoef = highCoef; c++;
          }
          phases--;
     }
}

#define SDRAM_SRC (1<<28)
#define BILINEAR (1<<29)

static void
SetupResizerParams( vpfe_resizer_params_t *params,
                    int srcWidth, int srcHeight,
                    int outWidth, int outHeight,
                    int *ret_outWidth,
                    int *ret_outHeight )
{
     int rsz;
     int hrsz;
     int vrsz;
     int tmp;

     D_DEBUG_AT( Davinci_Video, "%s( %4dx%4d->%4dx%4d )\n", __FUNCTION__, srcWidth, srcHeight, outWidth, outHeight );

     params->sdr_inadd = 0;
     params->sdr_inoff = 0;

     params->sdr_outadd = 0;
     params->sdr_outoff = 0;

     params->in_start = (0<<16)|(0);
     params->yenh = 0;

     params->rsz_cnt = SDRAM_SRC;




     //find scale factor
     rsz = (srcWidth<<8)/outWidth;
     if (rsz<64) {
          //too much upscaling, reduce destination size
          rsz = 64;
     }
     else if (rsz>1024) {
          //too much down scaling, reduce source size
          rsz=1024;
          srcWidth  = (outWidth * rsz)>>8;
     }

     tmp = ((srcWidth<<8)+255)/rsz;
     if (tmp > outWidth) tmp = outWidth;
     tmp &= ~1;  //force even
     if (rsz>256) {
          //upsize in vertical direction requires a multiple of 16 bytes (8 pixels)
          tmp &= ~0x7;
     }
     do {
          int t;
          hrsz = limitInput(rsz,srcWidth,tmp,&t);
          if (hrsz>=64) {
               srcWidth = t;
               break;
          }
          tmp-=2;
     } while (1);
     outWidth = tmp;

     if (srcWidth==outWidth) {
          int i=0;
          params->rsz_cnt |= ((256-1)<<0);    //1 to 1
          params->in_size = (srcWidth+3);  //4 taps
          while (i<16) {
               params->hfilt[i] = i? 0 : 0x100; //2 coefficient written at a time
               i++;
          }
     }
     else {
          SetupCoef(&params->hfilt[0],hrsz);
          params->rsz_cnt |= ((hrsz-1)<<0) | ((hrsz<256)? BILINEAR : 0);
          params->in_size = (srcWidth);
     }




     //find scale factor
     rsz = (srcHeight<<8)/outHeight;
     if (rsz<64) {
          //too much upscaling, reduce destination size
          rsz = 64;
     }
     else if (rsz>1024) {
          //too much down scaling, reduce source size
          rsz=1024;
          srcHeight = (outHeight * rsz)>>8;
     }

     tmp = ((srcHeight<<8)+255)/rsz;
     if (tmp > outHeight) tmp = outHeight;
     do {
          int t;
          vrsz = limitInput(rsz,srcHeight,tmp,&t);
          if (vrsz>=64) {
               srcHeight = t;
               break;
          }
          tmp--;
     } while (1);
     outHeight = tmp;

     if (srcHeight==outHeight) {
          int i=0;
          params->rsz_cnt |= ((256-1)<<10);   //1 to 1
          params->in_size |= ((srcHeight+3)<<16);   //4 taps
          while (i<16) {
               params->vfilt[i] = i? 0 : 0x100; //2 coefficient written at a time
               i++;
          }
     }
     else {
          SetupCoef(&params->vfilt[0],vrsz);
          params->rsz_cnt |= ((vrsz-1)<<10);
          params->in_size |= (srcHeight<<16);
     }


     params->out_size = (outHeight<<16)|(outWidth);

     D_DEBUG_AT( Davinci_Video, "  => %4dx%4d->%4dx%4d\n", srcWidth, srcHeight, outWidth, outHeight );

     if (ret_outWidth)
          *ret_outWidth = outWidth;
          
     if (ret_outHeight)
          *ret_outHeight = outHeight;
}

