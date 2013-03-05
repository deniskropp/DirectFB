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
     DRMKMSDataShared *shared = drmkms->shared;

     drmModeRes       *resources;
     drmModeConnector *connector = NULL;
     drmModeEncoder   *encoder   = NULL;
     uint32_t          crtc      = 0;

     int               i, j, k, l, found;

     description->caps = DSCCAPS_ENCODERS;
     description->encoders = 0;
     drmkms->enabled_connectors = 0;

     direct_snputs( description->name, "DRMKMS Screen", DFB_SCREEN_DESC_NAME_LENGTH );

     resources = drmkms->resources;

     D_INFO( "DirectFB/DRMKMS: Got %d connectors, %d encoders\n", resources->count_connectors, resources->count_encoders );

     for (i = 0; i < resources->count_connectors; i++) {
          crtc = 0;
          connector = drmModeGetConnector( drmkms->fd, resources->connectors[i] );
          if (!connector)
               continue;

          if ((connector->connection == DRM_MODE_CONNECTED || connector->connection == DRM_MODE_UNKNOWNCONNECTION) && connector->count_modes > 0) {
               D_INFO( "DirectFB/DRMKMS: found connected connector id %d.\n", connector->connector_id );

               if (connector->encoder_id) {
                    D_INFO( "DirectFB/DRMKMS: connector %d is already bound to encoder %d.\n", connector->connector_id, connector->encoder_id );
                    encoder = drmModeGetEncoder(drmkms->fd, connector->encoder_id);
               }

               if (encoder)
                    crtc = encoder->crtc_id;

               if (crtc)
                    D_INFO( "DirectFB/DRMKMS: encoder %d is already bound to ctrc %d.\n", connector->encoder_id, encoder->crtc_id );
               else {
                    D_INFO( "DirectFB/DRMKMS: Seaching for appropriate encoder/crtc for connector %d.\n", connector->connector_id );
                    for (j = 0; j < resources->count_encoders; j++) {
                         int busy = 0;
                         encoder = drmModeGetEncoder( drmkms->fd, resources->encoders[j] );

                         if (encoder == NULL)
                              continue;

                         for (k=0; k<drmkms->enabled_connectors; k++) {
                              if (drmkms->encoder[k]->encoder_id == encoder->encoder_id) {
                                   D_INFO( "DirectFB/DRMKMS: encoder %d is already in use by connector %d\n", encoder->encoder_id, drmkms->connector[k]->connector_id );
                                   busy = 1;
                              }
                         }

                         if (busy)
                              continue;

                         found = 0;
                         for (k = 0; k < resources->count_crtcs; k++) {
                              busy = 0;
                              if (!(encoder->possible_crtcs & (1 << k)))
                                   continue;

                              for (l=0; l<drmkms->enabled_connectors; l++) {
                                   if (drmkms->encoder[l]->crtc_id == resources->crtcs[k])
                                        busy = 1;
                              }
                              if (busy)
                                   continue;


                              crtc = resources->crtcs[k];
                              D_INFO( "DirectFB/DRMKMS: using encoder %d and crtc %d for connector %d.\n", encoder->encoder_id, crtc, connector->connector_id );
                              found = 1;
                              break;
                         }

                         if (found)
                              break;
                    }
               }

               if (encoder && crtc) {
                    drmkms->connector[drmkms->enabled_connectors] = connector;
                    drmkms->encoder[drmkms->enabled_connectors] = encoder;
                    drmkms->encoder[drmkms->enabled_connectors]->crtc_id = crtc;
                    drmkms->enabled_connectors++;

                    if (!shared->mirror_outputs)
                         break;
               }
               else if (encoder)
                    drmModeFreeEncoder( encoder );

               encoder = NULL;
          }
          else
               drmModeFreeConnector(connector);
     }

     if (!drmkms->enabled_connectors) {
          D_ERROR( "DirectFB/DRMKMS: No currently active connector found.\n");
          return DFB_INIT;
     }

     if (dfb_config->mode.width && dfb_config->mode.height) {
          drmModeModeInfo *mode  = drmkms_find_mode( 0, dfb_config->mode.width, dfb_config->mode.height, 0 );
          if (mode)
               shared->mode[0] = *mode;
          else
               shared->mode[0] = connector->modes[0];
     }
     else
          shared->mode[0] = connector->modes[0];

     for (int i=1; i<drmkms->enabled_connectors; i++)
          shared->mode[i] = shared->mode[0];


     D_INFO( "DirectFB/DRMKMS: Default mode is %dx%d, we have %d modes in total\n", shared->mode[0].hdisplay, shared->mode[0].vdisplay, drmkms->connector[0]->count_modes );

     drmkms->resources = resources;
     drmkms->saved_crtc = drmModeGetCrtc( drmkms->fd, drmkms->encoder[0]->crtc_id );

     description->encoders = drmkms->enabled_connectors;

     return DFB_OK;
}

static DFBResult
drmkmsGetScreenSize( CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data,
                     int        *ret_width,
                     int        *ret_height )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;

     *ret_width  = shared->mode[0].hdisplay;
     *ret_height = shared->mode[0].vdisplay;

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

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     direct_snputs( description->name, "DRMKMS Encoder", DFB_SCREEN_ENCODER_DESC_NAME_LENGTH );


     description->caps            = DSECAPS_RESOLUTION | DSECAPS_FREQUENCY;

     switch (drmkms->encoder[encoder]->encoder_type) {
          case DRM_MODE_ENCODER_DAC:
               description->type = DSET_CRTC;
               break;
          case DRM_MODE_ENCODER_LVDS:
          case DRM_MODE_ENCODER_TMDS:
               description->type = DSET_DIGITAL;
               break;
          case DRM_MODE_ENCODER_TVDAC:
               description->type = DSET_TV;
               break;
          default:
               description->type = DSET_UNKNOWN;
     }

     description->all_resolutions = drmkms_modes_to_dsor_bitmask( encoder );

     config->flags                = DSECONF_RESOLUTION | DSECONF_FREQUENCY;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &config->resolution, &config->frequency );

     return DFB_OK;
}

static DFBResult
drmkmsSetEncoderConfig( CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config )
{
     int ret = 0;

     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;

     DFBScreenEncoderFrequency dse_freq;
     DFBScreenOutputResolution dso_res;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_INVARG;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dso_res, &dse_freq );

     if (config->flags & DSECONF_FREQUENCY) {
          D_DEBUG_AT( DRMKMS_Screen, "   -> requested frequency change \n" );
          dse_freq = config->frequency;
     }

     if (config->flags & DSECONF_RESOLUTION) {
          D_DEBUG_AT( DRMKMS_Screen, "   -> requested resolution change \n" );
          dso_res = config->resolution;
     }

     drmModeModeInfo *videomode = drmkms_dsor_freq_to_mode( encoder, dso_res, dse_freq );

     if (!videomode)
          return DFB_INVARG;

     if ((shared->primary_dimension.w && (shared->primary_dimension.w < videomode->hdisplay) ) ||
         (shared->primary_dimension.h && (shared->primary_dimension.h < videomode->vdisplay ))) {

          D_DEBUG_AT( DRMKMS_Screen, "    -> cannot switch to mode to something that is bigger than the current primary layer\n" );

          return DFB_INVARG;
     }

     if (shared->primary_fb)
          ret = drmModeSetCrtc( drmkms->fd, drmkms->encoder[encoder]->crtc_id, shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector[encoder]->connector_id, 1, videomode );

     if (ret) {
          D_DEBUG_AT( DRMKMS_Screen, " crtc_id: %d connector_id %d, mode %dx%d\n", drmkms->encoder[encoder]->crtc_id, drmkms->connector[encoder]->connector_id, shared->mode[encoder].hdisplay, shared->mode[encoder].vdisplay );
          D_PERROR( "DirectFB/DRMKMS: drmModeSetCrtc() failed! (%d)\n", ret );
          return DFB_FAILURE;
     }

     shared->mode[encoder] = *videomode;

     return DFB_OK;
}


static DFBResult
drmkmsTestEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config,
                         DFBScreenEncoderConfigFlags  *failed )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;

     DFBScreenEncoderFrequency dse_freq;
     DFBScreenOutputResolution dso_res;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_UNSUPPORTED;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dso_res, &dse_freq );

     if (config->flags & DSECONF_FREQUENCY)
          dse_freq = config->frequency;

     if (config->flags & DSECONF_RESOLUTION)
          dso_res = config->resolution;

     drmModeModeInfo *videomode = drmkms_dsor_freq_to_mode( encoder, dso_res, dse_freq );
     if (!videomode) {
          *failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);

          return DFB_UNSUPPORTED;
     }

     if ((shared->primary_dimension.w && (shared->primary_dimension.w < videomode->hdisplay) ) ||
         (shared->primary_dimension.h && (shared->primary_dimension.h < videomode->vdisplay ))) {

          D_DEBUG_AT( DRMKMS_Screen, "    -> cannot switch to mode to something that is bigger than the current primary layer\n" );

          *failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);

          return DFB_UNSUPPORTED;
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

