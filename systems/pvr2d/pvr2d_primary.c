/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <fusion/types.h>

#include <stdio.h>

#include <directfb.h>
#include <directfb_util.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include <string.h>
#include <stdlib.h>

#include "pvr2d_system.h"
#include "pvr2d_primary.h"

D_DEBUG_DOMAIN( PVR2D_Layer,  "PVR2D/Layer",  "PVR2D Layer" );
D_DEBUG_DOMAIN( PVR2D_Update, "PVR2D/Update", "PVR2D Update" );

/**********************************************************************************************************************/

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     PVR2DData       *pvr2d  = driver_data;
     PVR2DDataShared *shared = pvr2d->shared;

     (void) shared;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     /* Set the screen capabilities. */
     description->caps    = DSCCAPS_OUTPUTS;
     description->outputs = 1;

     /* Set the screen name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "PVR2D Primary Screen" );




     return DFB_OK;
}

static DFBResult
primaryShutdownScreen( CoreScreen *screen,
                       void       *driver_data,
                       void       *screen_data )
{
     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     PVR2DData       *pvr2d  = driver_data;
     PVR2DDataShared *shared = pvr2d->shared;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     *ret_width  = shared->screen_size.w;
     *ret_height = shared->screen_size.h;

     return DFB_OK;
}

static DFBResult
primaryInitOutput( CoreScreen                   *screen,
                   void                         *driver_data,
                   void                         *screen_data,
                   int                           output,
                   DFBScreenOutputDescription   *description,
                   DFBScreenOutputConfig        *config )
{
     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     description->caps = DSOCAPS_RESOLUTION;

     config->flags |= DSOCONF_RESOLUTION;
     config->resolution = DSOR_UNKNOWN;

     return DFB_OK;
}

static DFBResult
primaryTestOutputConfig( CoreScreen                  *screen,
                         void                        *driver_data,
                         void                        *screen_data,
                         int                          output,
                         const DFBScreenOutputConfig *config,
                         DFBScreenOutputConfigFlags  *failed )
{
     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primarySetOutputConfig( CoreScreen                  *screen,
                        void                        *driver_data,
                        void                        *screen_data,
                        int                          output,
                        const DFBScreenOutputConfig *config )
{
     PVR2DData       *pvr2d  = driver_data;
     PVR2DDataShared *shared = pvr2d->shared;

     int hor[] = { 640,720,720,800,1024,1152,1280,1280,1280,1280,1400,1600,1920 };
     int ver[] = { 480,480,576,600, 768, 864, 720, 768, 960,1024,1050,1200,1080 };

     int res;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     (void)output; /* all outputs are active */

     /* we support screen resizing only */
     if (config->flags != DSOCONF_RESOLUTION)
          return DFB_INVARG;

     res = D_BITn32(config->resolution);
     if ( (res == -1) || (res >= D_ARRAY_SIZE(hor)) )
          return DFB_INVARG;

     shared->screen_size.w = hor[res];
     shared->screen_size.h = ver[res];

     // FIXME: recreate window/target etc.

     return DFB_OK;
}

static const ScreenFuncs _pvr2dPrimaryScreenFuncs = {
     .InitScreen       = primaryInitScreen,
     .ShutdownScreen   = primaryShutdownScreen,
     .GetScreenSize    = primaryGetScreenSize,
     .InitOutput       = primaryInitOutput,
     .TestOutputConfig = primaryTestOutputConfig,
     .SetOutputConfig  = primarySetOutputConfig
};

const ScreenFuncs *pvr2dPrimaryScreenFuncs = &_pvr2dPrimaryScreenFuncs;

/******************************************************************************/

static int
primaryLayerDataSize( void )
{
     return sizeof(PVR2DLayerData);
}

static int
primaryRegionDataSize( void )
{
     return 0;
}

static DFBResult
primaryInitLayer( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  DFBDisplayLayerDescription *description,
                  DFBDisplayLayerConfig      *config,
                  DFBColorAdjustment         *adjustment )
{
     PVR2DData       *pvr2d  = driver_data;
     PVR2DDataShared *shared = pvr2d->shared;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "PVR2D Primary Layer" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode  = DLBM_FRONTONLY;

     if (dfb_config->mode.width)
          config->width  = dfb_config->mode.width;
     else
          config->width  = shared->screen_size.w;

     if (dfb_config->mode.height)
          config->height = dfb_config->mode.height;
     else
          config->height = shared->screen_size.h;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else
          config->pixelformat = DSPF_RGB16;

     return DFB_OK;
}

static DFBResult
primaryTestRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
//          case DLBM_BACKSYSTEM:
//          case DLBM_BACKVIDEO:
//          case DLBM_TRIPLE:
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }

     switch (config->format) {
//          case DSPF_ARGB:
          case DSPF_RGB16:
               break;

          default:
               fail |= CLRCF_FORMAT;
               break;
     }

     if (config->options)
          fail |= CLRCF_OPTIONS;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primaryAddRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
primarySetRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette,
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
DisplaySurface( PVR2DData    *pvr2d,
                PVR2DMEMINFO *meminfo )
{
     PVR2DERROR ePVR2DStatus;
/*
     ePVR2DStatus = PVR2DPresentBlt( pvr2d->hPVR2DContext, meminfo, 0 );
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DPresentBlt() failed! (status %d)\n", ePVR2DStatus );
          return DFB_FAILURE;
     }
*/
     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   CoreSurfaceBufferLock *left_lock,
                   CoreSurfaceBufferLock *right_lock )
{
     PVR2DData       *pvr2d  = driver_data;
//     PVR2DDataShared *shared = pvr2d->shared;

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     dfb_surface_flip( surface, false );

     return DisplaySurface( pvr2d, left_lock->handle );
}

static DFBResult
primaryUpdateRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data,
                     CoreSurface           *surface,
                     const DFBRegion       *left_update,
                     CoreSurfaceBufferLock *left_lock,
                     const DFBRegion       *right_update,
                     CoreSurfaceBufferLock *right_lock )
{
     PVR2DData       *pvr2d  = driver_data;
//     PVR2DDataShared *shared = pvr2d->shared;

     DFBRegion        region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     D_DEBUG_AT( PVR2D_Layer, "%s()\n", __FUNCTION__ );

     if (left_update && !dfb_region_region_intersect( &region, left_update ))
          return DFB_OK;

     return DisplaySurface( pvr2d, left_lock->handle );
}

static const DisplayLayerFuncs _pvr2dPrimaryLayerFuncs = {
     .LayerDataSize  = primaryLayerDataSize,
     .RegionDataSize = primaryRegionDataSize,
     .InitLayer      = primaryInitLayer,

     .TestRegion     = primaryTestRegion,
     .AddRegion      = primaryAddRegion,
     .SetRegion      = primarySetRegion,
     .RemoveRegion   = primaryRemoveRegion,
     .FlipRegion     = primaryFlipRegion,
     .UpdateRegion   = primaryUpdateRegion,
};

const DisplayLayerFuncs *pvr2dPrimaryLayerFuncs = &_pvr2dPrimaryLayerFuncs;

