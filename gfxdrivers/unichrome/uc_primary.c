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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <directfb.h>

#include <core/layers.h>

#include <misc/conf.h>

#include "unichrome.h"
#include "uc_overlay.h"
#include "vidregs.h"
#include "mmio.h"

/* primary layer hooks */

#define OSD_OPTIONS      (DLOP_ALPHACHANNEL | DLOP_SRC_COLORKEY | DLOP_OPACITY)

DisplayLayerFuncs  ucOldPrimaryFuncs;
void              *ucOldPrimaryDriverData;

static DFBResult
osdInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     DFBResult ret;

     /* call the original initialization function first */
     ret = ucOldPrimaryFuncs.InitLayer( layer,
                                        ucOldPrimaryDriverData,
                                        layer_data, description,
                                        config, adjustment );
     if (ret)
          return ret;

     /* set name */
     snprintf(description->name,
              DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VIA CLE266 Graphics");

     /* add some capabilities */
     description->caps |= DLCAPS_ALPHACHANNEL |
                          DLCAPS_OPACITY | DLCAPS_SRC_COLORKEY;

     return DFB_OK;
}

static DFBResult
osdTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     DFBResult                  ret;
     CoreLayerRegionConfigFlags fail = 0;
     DFBDisplayLayerOptions     options = config->options;

     /* remove options before calling the original function */
     config->options = DLOP_NONE;

     /* call the original function */
     ret = ucOldPrimaryFuncs.TestRegion( layer, ucOldPrimaryDriverData,
                                         layer_data, config, &fail );

     /* check options if specified */
     if (options) {
          /* any unsupported option wanted? */
          if (options & ~OSD_OPTIONS)
               fail |= CLRCF_OPTIONS;

          /* opacity and alpha channel cannot be used at once */
          if ((options & (DLOP_OPACITY | DLOP_ALPHACHANNEL)) ==
              (DLOP_OPACITY | DLOP_ALPHACHANNEL))
          {
               fail |= CLRCF_OPTIONS;
          }

          if ((options & DLOP_ALPHACHANNEL) && config->format != DSPF_AiRGB)
               fail |= CLRCF_OPTIONS;
     }

     /* restore options */
     config->options = options;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return ret;
}

static DFBResult
osdSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     DFBResult     ret;
     UcDriverData *ucdrv = (UcDriverData*) driver_data;

     /* call the original function */
     ret = ucOldPrimaryFuncs.SetRegion( layer, ucOldPrimaryDriverData,
                                        layer_data, region_data,
                                        config, updated, surface,
                                        palette );
     if (ret)
          return ret;

     uc_ovl_vcmd_wait(ucdrv->hwregs);

     /* select pixel based or global alpha */

     if (!ucdrv->ovl)   // overlay not present
          return DFB_OK;
     
     if (config->options & DLOP_ALPHACHANNEL)
          ucdrv->ovl->opacity_primary = -1; // use primary alpha for overlay
     else if (config->options & DLOP_OPACITY)
          ucdrv->ovl->opacity_primary = config->opacity ^ 0xff; // use inverse for overlay
     else
          ucdrv->ovl->opacity_primary = 0x00; // primary opaque == overlay transparent

     if (ucdrv->ovl->v1.level < 0)  // primary on top?
     {
          VIDEO_OUT(ucdrv->hwregs, V_ALPHA_CONTROL,
               uc_ovl_map_alpha(ucdrv->ovl->opacity_primary));
          VIDEO_OUT(ucdrv->hwregs, V_COMPOSE_MODE,
               VIDEO_IN(ucdrv->hwregs, V_COMPOSE_MODE) | V1_COMMAND_FIRE);
     }
     
     return DFB_OK;
}

DisplayLayerFuncs ucPrimaryFuncs = {
     InitLayer:          osdInitLayer,

     TestRegion:         osdTestRegion,
     SetRegion:          osdSetRegion
};

