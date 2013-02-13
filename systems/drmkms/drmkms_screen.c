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


//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>


#include "drmkms_system.h"


typedef struct {
     drmModeConnector *connector;
     drmModeEncoder   *encoder;
     drmModeModeInfo   mode;
} DRMKMSScreenData;

/**********************************************************************************************************************/

static int
drmkmsScreenDataSize( void )
{
     return sizeof(DRMKMSScreenData);
}

static DFBResult
drmkmsInitScreen( CoreScreen           *screen,
                  CoreGraphicsDevice   *device,
                  void                 *driver_data,
                  void                 *screen_data,
                  DFBScreenDescription *description )
{
     DRMKMSData       *drmkms = driver_data;

     description->caps = DSCCAPS_NONE;

     direct_snputs( description->name, "DRMKMS", DFB_SCREEN_DESC_NAME_LENGTH );

     drmModeRes       *resources;
     drmModeConnector *connector = NULL;
     drmModeEncoder   *encoder   = NULL;
     int               i;

     resources = drmkms->resources;

     D_INFO( "DirectFB/DRMKMS: Got %d connectors, %d encoders\n", resources->count_connectors, resources->count_encoders );

     for (i = resources->count_connectors-1; i >= 0; i--) {
          connector = drmModeGetConnector( drmkms->fd, resources->connectors[i] );
          if (connector == NULL)
               continue;

          if (connector->connection == DRM_MODE_CONNECTED &&
              connector->count_modes > 0)
               break;

          drmModeFreeConnector(connector);
     }

     if (i == -1) {
          D_ERROR( "DirectFB/DRMKMS: No currently active connector found. Forcing the last found connector\n");
          connector = drmModeGetConnector( drmkms->fd, resources->connectors[resources->count_connectors-1] );
     }

     D_INFO( "DirectFB/DRMKMS: using connector id %d.\n", connector->connector_id );

     for (i = resources->count_encoders-1; i >= 0; i--) {
          encoder = drmModeGetEncoder( drmkms->fd, resources->encoders[i] );

          if (encoder == NULL)
               continue;

          if (encoder->encoder_id == connector->encoder_id)
               break;

          drmModeFreeEncoder(encoder);
     }

     if (i == -1) {
          D_ERROR( "DirectFB/DRMKMS: Could not match encoder/connector. Forcing the last found encoder\n");
          encoder = drmModeGetEncoder( drmkms->fd, resources->encoders[resources->count_encoders-1] );
     }

     D_INFO( "DirectFB/DRMKMS: using encoder id %d.\n", encoder->encoder_id );

     if (!encoder->crtc_id) {
          D_ERROR( "DirectFB/DRMKMS: No crtc associated to the encoder. Forcing the last found crtc\n");
          encoder->crtc_id = resources->crtcs[resources->count_crtcs-1];
     }

     D_INFO( "DirectFB/DRMKMS: using crtc id %d.\n", encoder->crtc_id  );

     drmkms->connector = connector;
     drmkms->encoder   = encoder;
     drmkms->mode      = connector->modes[0];

     D_INFO( "DirectFB/DRMKMS: Default mode is %dx%d, we have %d modes in total\n", drmkms->mode.hdisplay, drmkms->mode.vdisplay, drmkms->connector->count_modes );

     drmkms->resources = resources;
     drmkms->saved_crtc = drmModeGetCrtc( drmkms->fd, drmkms->encoder->crtc_id );


     return DFB_OK;
}

static DFBResult
drmkmsGetScreenSize( CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data,
                     int        *ret_width,
                     int        *ret_height )
{
     DRMKMSScreenData *data = screen_data;

     *ret_width  = data->mode.hdisplay;
     *ret_height = data->mode.vdisplay;

     return DFB_OK;
}

static const ScreenFuncs _drmkmsScreenFuncs = {
     .ScreenDataSize = drmkmsScreenDataSize,
     .InitScreen     = drmkmsInitScreen,
     .GetScreenSize  = drmkmsGetScreenSize
};

const ScreenFuncs *drmkmsScreenFuncs = &_drmkmsScreenFuncs;

