/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>


#include "mesa_system.h"


typedef struct {
     drmModeConnector *connector;
     drmModeEncoder   *encoder;
     drmModeModeInfo   mode;
} MesaScreenData;

/**********************************************************************************************************************/

static int
mesaScreenDataSize( void )
{
     return sizeof(MesaScreenData);
}

static DFBResult
mesaInitScreen( CoreScreen           *screen,
                CoreGraphicsDevice   *device,
                void                 *driver_data,
                void                 *screen_data,
                DFBScreenDescription *description )
{
     MesaData       *mesa = driver_data;
     MesaScreenData *data = screen_data;

     description->caps = DSCCAPS_NONE;

     direct_snputs( description->name, "Mesa", DFB_SCREEN_DESC_NAME_LENGTH );







     drmModeRes       *resources;
     drmModeConnector *connector;
     drmModeEncoder   *encoder;
     int               i;

     resources = drmModeGetResources( mesa->fd );
     if (!resources) {
          D_ERROR( "DirectFB/Mesa: drmModeGetResources() failed!\n" );
          return DFB_INIT;
     }

     D_INFO( "DirectFB/Mesa: Got %d connectors, %d encoders\n", resources->count_connectors, resources->count_encoders );

     for (i = 1; i < resources->count_connectors; i++) {
          connector = drmModeGetConnector( mesa->fd, resources->connectors[i] );
          if (connector == NULL)
               continue;

          if (connector->connection == DRM_MODE_CONNECTED &&
              connector->count_modes > 0)
               break;

          drmModeFreeConnector(connector);
     }

     if (i == resources->count_connectors) {
          D_ERROR( "DirectFB/Mesa: No currently active connector found.\n" );
          return DFB_INIT;
     }

     for (i = 1; i < resources->count_encoders; i++) {
          encoder = drmModeGetEncoder( mesa->fd, resources->encoders[i] );

          if (encoder == NULL)
               continue;

          if (encoder->encoder_id == connector->encoder_id)
               break;

          drmModeFreeEncoder(encoder);
     }

     data->connector = connector;
     data->encoder   = encoder;
     data->mode      = connector->modes[0];


     mesa->connector = connector;
     mesa->encoder   = encoder;
     mesa->mode      = connector->modes[0];


     return DFB_OK;
}

static DFBResult
mesaGetScreenSize( CoreScreen *screen,
                   void       *driver_data,
                   void       *screen_data,
                   int        *ret_width,
                   int        *ret_height )
{
     MesaScreenData *data = screen_data;

     *ret_width  = data->mode.hdisplay;
     *ret_height = data->mode.vdisplay;

     return DFB_OK;
}

static const ScreenFuncs _mesaScreenFuncs = {
     .ScreenDataSize = mesaScreenDataSize,
     .InitScreen     = mesaInitScreen,
     .GetScreenSize  = mesaGetScreenSize
};

const ScreenFuncs *mesaScreenFuncs = &_mesaScreenFuncs;

