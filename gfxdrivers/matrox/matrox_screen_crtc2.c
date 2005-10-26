/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <fbdev/fbdev.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_maven.h"


typedef struct {
     DFBScreenPowerMode power_mode;
} MatroxCrtc2ScreenData;

static void crtc2_wait_vsync( MatroxDriverData *mdrv );

/**************************************************************************************************/

static int
crtc2ScreenDataSize()
{
     return sizeof(MatroxCrtc2ScreenData);
}

static DFBResult
crtc2InitScreen( CoreScreen           *screen,
                 GraphicsDevice       *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_ENCODERS | DSCCAPS_OUTPUTS;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "Matrox CRTC2 Screen" );

     /* Set number of encoders and outputs. */
     description->encoders = 1;
     description->outputs  = 1;

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
crtc2InitEncoder( CoreScreen                  *screen,
                  void                        *driver_data,
                  void                        *screen_data,
                  int                          encoder,
                  DFBScreenEncoderDescription *description,
                  DFBScreenEncoderConfig      *config )
{
     /* Set the encoder capabilities & type. */
     description->caps = DSECAPS_TV_STANDARDS;
     description->type = DSET_TV;

     /* Set supported TV standards. */
     description->tv_standards = DSETV_PAL | DSETV_NTSC;

     /* Set default configuration. */
     config->flags       = DSECONF_TV_STANDARD;
     config->tv_standard = dfb_config->matrox_ntsc ? DSETV_NTSC : DSETV_PAL;

     return DFB_OK;
}

static DFBResult
crtc2InitOutput( CoreScreen                 *screen,
                 void                       *driver_data,
                 void                       *screen_data,
                 int                         output,
                 DFBScreenOutputDescription *description,
                 DFBScreenOutputConfig      *config )
{
     /* Set the output capabilities. */
     description->caps = DSOCAPS_CONNECTORS |
                         DSOCAPS_SIGNAL_SEL | DSOCAPS_CONNECTOR_SEL;

     /* Set supported output connectors and signals. */
     description->all_connectors = DSOC_CVBS | DSOC_YC | DSOC_SCART;
     description->all_signals    = DSOS_CVBS | DSOS_YC | DSOS_RGB;

     /* Set default configuration. */
     config->flags = DSOCONF_SIGNALS | DSOCONF_CONNECTORS;

     switch (dfb_config->matrox_cable) {
          case 1:
               /* SCART RGB */
               config->out_signals    = DSOS_RGB;
               config->out_connectors = DSOC_SCART;
               break;
          case 2:
               /* SCART Composite */
               config->out_signals    = DSOS_CVBS;
               config->out_connectors = DSOC_SCART;
               break;
          default:
               /* Composite / S-Video */
               config->out_signals    = DSOS_CVBS | DSOS_YC;
               config->out_connectors = DSOC_CVBS | DSOC_YC;
               break;
     }

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
crtc2SetPowerMode( CoreScreen         *screen,
                   void               *driver_data,
                   void               *screen_data,
                   DFBScreenPowerMode  mode )
{
     MatroxCrtc2ScreenData *msc2 = (MatroxCrtc2ScreenData*) screen_data;

     msc2->power_mode = mode;

     return DFB_OK;
}

static DFBResult
crtc2WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     MatroxDriverData      *mdrv = (MatroxDriverData*) driver_data;
     MatroxCrtc2ScreenData *msc2 = (MatroxCrtc2ScreenData*) screen_data;

     if (msc2->power_mode == DSPM_ON)
          crtc2_wait_vsync( mdrv );

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
crtc2TestEncoderConfig( CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config,
                        DFBScreenEncoderConfigFlags  *failed )
{
     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
crtc2SetEncoderConfig( CoreScreen                   *screen,
                       void                         *driver_data,
                       void                         *screen_data,
                       int                           encoder,
                       const DFBScreenEncoderConfig *config )
{
//     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DFBResult
crtc2TestOutputConfig( CoreScreen                  *screen,
                       void                        *driver_data,
                       void                        *screen_data,
                       int                          output,
                       const DFBScreenOutputConfig *config,
                       DFBScreenOutputConfigFlags  *failed )
{
     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
crtc2SetOutputConfig( CoreScreen                  *screen,
                      void                        *driver_data,
                      void                        *screen_data,
                      int                          output,
                      const DFBScreenOutputConfig *config )
{
//     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DFBResult
crtc2GetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     *ret_width  = 720;
     *ret_height = dfb_config->matrox_ntsc ? 480 : 576;

     return DFB_OK;
}

ScreenFuncs matroxCrtc2ScreenFuncs = {
     ScreenDataSize:     crtc2ScreenDataSize,
     InitScreen:         crtc2InitScreen,
     InitEncoder:        crtc2InitEncoder,
     InitOutput:         crtc2InitOutput,
     SetPowerMode:       crtc2SetPowerMode,
     WaitVSync:          crtc2WaitVSync,
     TestEncoderConfig:  crtc2TestEncoderConfig,
     SetEncoderConfig:   crtc2SetEncoderConfig,
     TestOutputConfig:   crtc2TestOutputConfig,
     SetOutputConfig:    crtc2SetOutputConfig,
     GetScreenSize:      crtc2GetScreenSize
};

/**************************************************************************************************/

static void crtc2_wait_vsync( MatroxDriverData *mdrv )
{
     int vdisplay = (dfb_config->matrox_ntsc ? 480/2 : 576/2) + 1;

#ifdef FBIO_WAITFORVSYNC
     static const int one = 1;
     FBDev *dfb_fbdev = dfb_system_data();
     if (ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, &one ))
#endif
          while ((int)(mga_in32( mdrv->mmio_base, C2VCOUNT ) & 0x00000FFF) != vdisplay)
               ;
}

