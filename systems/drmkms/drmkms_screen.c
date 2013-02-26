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


D_DEBUG_DOMAIN( DRMKMS_Screen, "DRMKMS/Screen", "DRM/KMS Screen" );

/**********************************************************************************************************************/


static DFBResult
drmkmsInitScreen( CoreScreen           *screen,
                  CoreGraphicsDevice   *device,
                  void                 *driver_data,
                  void                 *screen_data,
                  DFBScreenDescription *description )
{
     DRMKMSData       *drmkms = driver_data;

     description->caps = DSCCAPS_ENCODERS;
     description->encoders = 1;

     direct_snputs( description->name, "DRMKMS Screen", DFB_SCREEN_DESC_NAME_LENGTH );

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

     if (dfb_config->mode.width && dfb_config->mode.height) {
          drmModeModeInfo *mode  = drmkms_find_mode( dfb_config->mode.width, dfb_config->mode.height );
          if (mode)
               drmkms->mode = *mode;
          else
               drmkms->mode = connector->modes[0];
     } else
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
     DRMKMSData *drmkms = driver_data;

     *ret_width  = drmkms->mode.hdisplay;
     *ret_height = drmkms->mode.vdisplay;

     return DFB_OK;
}

static DFBResult
drmkmsInitEncoder( CoreScreen                  *screen,
                   void                        *driver_data,
                   void                        *screen_data,
                   int                          encoder,
                   DFBScreenEncoderDescription *description,
                   DFBScreenEncoderConfig      *config )
{
     DRMKMSData       *drmkms    = driver_data;
     DRMKMSDataShared *shared    = drmkms->shared;

     (void) shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     direct_snputs( description->name, "DRMKMS Encoder", DFB_SCREEN_ENCODER_DESC_NAME_LENGTH );


     description->caps            = DSECAPS_RESOLUTION;
     description->type            = DSET_UNKNOWN;

     description->all_resolutions = drmkms_modes_to_dsor_bitmask();

     config->flags          = DSECONF_RESOLUTION;
     config->resolution     = drmkms_mode_to_dsor( &drmkms->mode );

     return DFB_OK;
}

static DFBResult
drmkmsSetEncoderConfig( CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     int ret;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     if (config->flags & DSECONF_RESOLUTION) {
          drmModeModeInfo *videomode = drmkms_dsor_to_mode( config->resolution);

          ret = drmModeSetCrtc( drmkms->fd, drmkms->encoder->crtc_id, shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector->connector_id, 1, videomode );
          if (ret) {
               D_PERROR( "DirectFB/DRMKMS: drmModeSetCrtc() failed! (%d)\n", ret );
               D_DEBUG_AT( DRMKMS_Screen, " crtc_id: %d connector_id %d, mode %dx%d\n", drmkms->encoder->crtc_id, drmkms->connector->connector_id, drmkms->mode.hdisplay, drmkms->mode.vdisplay );
               return DFB_FAILURE;
          }

          drmkms->mode = *videomode;
     }

     return DFB_OK;
}


static DFBResult
drmkmsTestEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config,
                         DFBScreenEncoderConfigFlags   *failed )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     if (config->flags & DSECONF_RESOLUTION) {
          drmModeModeInfo *videomode = drmkms_dsor_to_mode( config->resolution );

          if (!videomode) {
               *failed = DSECONF_RESOLUTION;

               return DFB_UNSUPPORTED;
          }
          if ((shared->primary_dimension.w < videomode->vdisplay ) ||
              (shared->primary_dimension.h < videomode->hdisplay )) {

               D_DEBUG_AT( DRMKMS_Screen, "%s() cannot switch to mode to something that is bigger than the current primary layer\n", __FUNCTION__ );

               *failed = DSECONF_RESOLUTION;

               return DFB_UNSUPPORTED;
          }

     }

     return DFB_OK;
}


static const ScreenFuncs _drmkmsScreenFuncs = {
     .InitScreen        = drmkmsInitScreen,
     .GetScreenSize     = drmkmsGetScreenSize,
     .InitEncoder       = drmkmsInitEncoder,
     .SetEncoderConfig  = drmkmsSetEncoderConfig,
     .TestEncoderConfig = drmkmsTestEncoderConfig,
};

const ScreenFuncs *drmkmsScreenFuncs = &_drmkmsScreenFuncs;

