/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layer_control.h>
#include <core/layers_internal.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <misc/conf.h>

#include "radeon.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"



/*************************** CRTC1 Screen functions **************************/

static DFBResult
crtc1WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     volatile __u8    *mmio = rdrv->mmio_base; 
     int               i;
     
     if (dfb_config->pollvsync_none)
          return DFB_OK;
          
     radeon_out32( mmio, GEN_INT_STATUS, 
          (radeon_in32( mmio, GEN_INT_STATUS ) & ~VSYNC_INT) | VSYNC_INT_AK );
     
     for (i = 0; i < 2000000; i++) {
          struct timespec t = { 0, 0 };     
          
          if (radeon_in32( mmio, GEN_INT_STATUS ) & VSYNC_INT)
               break;
          nanosleep( &t, NULL );
     }

     return DFB_OK;
}

ScreenFuncs RadeonCrtc1ScreenFuncs = {
     .WaitVSync = crtc1WaitVSync
};

ScreenFuncs  OldPrimaryScreenFuncs;
void        *OldPrimaryScreenDriverData;


/*************************** CRTC1 Layer functions **************************/

#define CRTC1_SUPPORTED_OPTIONS ( DLOP_ALPHACHANNEL )

static DFBResult
crtc1InitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     DFBResult ret;
     
     ret = OldPrimaryLayerFuncs.InitLayer( layer,
                                           OldPrimaryLayerDriverData,
                                           layer_data, description,
                                           config, adjustment );
                                          
     description->caps |= DLCAPS_ALPHACHANNEL;
     
     return ret;
}

static DFBResult
crtc1TestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfig      layer_config;
     CoreLayerRegionConfigFlags fail = 0;
     DFBResult                  ret;
         
     layer_config = *config;
     layer_config.options &= ~CRTC1_SUPPORTED_OPTIONS;
     
     ret = OldPrimaryLayerFuncs.TestRegion( layer,
                                            OldPrimaryLayerDriverData,
                                            layer_data, &layer_config, &fail );
      
     if (config->options & ~CRTC1_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;
          
     if (config->options & DLOP_ALPHACHANNEL && config->format != DSPF_ARGB)
          fail |= CLRCF_OPTIONS;
     
     if (failed)
          *failed = fail;
          
     return fail ? DFB_UNSUPPORTED : DFB_OK;
}

static DFBResult
crtc1SetRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                void                       *region_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags  updated,
                CoreSurface                *surface,
                CorePalette                *palette )
{
     
     if (updated & ~CLRCF_OPTIONS) {
          return OldPrimaryLayerFuncs.SetRegion( layer,
                                                 OldPrimaryLayerDriverData,
                                                 layer_data, region_data,
                                                 config, updated, surface, palette );
     }

     return DFB_OK;
}

DisplayLayerFuncs RadeonCrtc1LayerFuncs = {
     .InitLayer  = crtc1InitLayer,
     .TestRegion = crtc1TestRegion,
     .SetRegion  = crtc1SetRegion
};

DisplayLayerFuncs  OldPrimaryLayerFuncs;
void              *OldPrimaryLayerDriverData;

