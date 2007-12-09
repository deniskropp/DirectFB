/*
   TI Davinci driver - Video Layer

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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


D_DEBUG_DOMAIN( Davinci_Video, "Davinci/Video", "TI Davinci Video" );

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

     ret = ioctl( ddrv->fb[VID1].fd, FBIOGET_VSCREENINFO, &dvid->var );
     if (ret) {
          D_PERROR( "Davinci/Video: FBIOGET_VSCREENINFO (fb%d) failed!\n", VID1 );
          return DFB_INIT;
     }

     ret = ioctl( ddrv->fb[VID0].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID0, 0 );

     ret = ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_SCREEN_POSITION;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "TI Davinci Video" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = 720;
     config->height      = 576;
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

     if (config->options & ~DAVINCI_VIDEO_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
//          case DSPF_YUY2:
          case DSPF_UYVY:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width  < 8 || config->width  > 720)
          fail |= CLRCF_WIDTH;

     if (config->height < 8 || config->height > 576)
          fail |= CLRCF_HEIGHT;


     if (config->dest.x < 0 || config->dest.y < 0)
          fail |= CLRCF_DEST;

     if (config->dest.x + config->dest.w > 720)
          fail |= CLRCF_DEST;

     if (config->dest.y + config->dest.h > 576)
          fail |= CLRCF_DEST;


     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

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
     int                  ret;
     DavinciDriverData   *ddrv = driver_data;
     DavinciDeviceData   *ddev = ddrv->ddev;
     DavinciVideoLayerData *dvid = layer_data;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( dvid != NULL );

     ret = ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

     ioctl( ddrv->fb[VID1].fd, FBIO_WAITFORVSYNC );

     /* Update size? */
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT)) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_Video, "  -> %dx%d\n", config->width, config->height );

/*********************************** Start workaround ***********************************/
          win_pos.xpos = 0;
          win_pos.ypos = 0;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/Video: FBIO_SETPOS (fb%d - %d,%d) failed!\n", VID1, win_pos.xpos, win_pos.ypos );

          updated |= CLRCF_DEST;

          dvid->var.yoffset = 0;
/*********************************** End workaround ***********************************/

          /* Set width and height. */
          dvid->var.xres = config->width;
          dvid->var.yres = config->height;

          dvid->var.yres_virtual = ddrv->fb[VID1].size / lock->pitch;

          ret = ioctl( ddrv->fb[VID1].fd, FBIOPUT_VSCREENINFO, &dvid->var );
          if (ret)
               D_PERROR( "Davinci/Video: FBIOPUT_VSCREENINFO (fb%d) failed!\n", VID1 );
     }

     /* Update position? */
     if (updated & CLRCF_DEST) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_Video, "  -> %d, %d\n", config->dest.x, config->dest.y );

          /* Set horizontal and vertical offset. */
          win_pos.xpos = config->dest.x;
          win_pos.ypos = config->dest.y;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/Video: FBIO_SETPOS (fb%d - %d,%d) failed!\n", VID1, config->dest.x, config->dest.y );
     }

     /* Update format? */
     if (updated & CLRCF_FORMAT) {
          vpbe_video_config_params_t params;

          params.cb_cr_order = (config->format == DSPF_YUY2) ? 1 : 0;

          params.exp_info.horizontal = VPBE_DISABLE;
          params.exp_info.vertical   = VPBE_DISABLE;

          ret = ioctl( ddrv->fb[VID1].fd, FBIO_SET_VIDEO_CONFIG_PARAMS, &params );
          if (ret)
               D_PERROR( "Davinci/Video: FBIO_SET_VIDEO_CONFIG_PARAMS (fb%d - %s) failed!\n",
                         VID1, params.cb_cr_order ? "CrCb" : "CbCr" );
     }

     davincifb_pan_display( &ddrv->fb[VID1], &dvid->var, lock, DSFLIP_NONE );

     ret = ioctl( ddrv->fb[VID1].fd, FBIOGET_FSCREENINFO, &ddev->fix[VID1] );
     if (ret)
          D_PERROR( "Davinci/Video: FBIOGET_FSCREENINFO (fb%d) failed!\n", VID1 );

     dvid->enable = true;

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
          D_PERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 0 );

     dvid->enable = false;

     return DFB_OK;
}

static void
enable_video( DavinciDriverData     *ddrv,
              DavinciVideoLayerData *dvid )
{
     if (!dvid->enable)
          return;

     ioctl( ddrv->fb[VID1].fd, FBIO_WAITFORVSYNC );

     if (ioctl( ddrv->fb[VID1].fd, FBIO_ENABLE_DISABLE_WIN, 1 ))
          D_PERROR( "Davinci/Video: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", VID1, 1 );

     dvid->enable = false;
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
     DavinciDriverData     *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( dvid != NULL );

     davincifb_pan_display( &ddrv->fb[VID1], &dvid->var, lock, flags );

     dfb_surface_flip( surface, false );

     enable_video( ddrv, dvid );

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
     DavinciDriverData *ddrv = driver_data;
     DavinciVideoLayerData *dvid = layer_data;

     D_DEBUG_AT( Davinci_Video, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( dvid != NULL );

     enable_video( ddrv, dvid );

     return DFB_OK;
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

