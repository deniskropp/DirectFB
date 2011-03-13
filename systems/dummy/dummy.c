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


#define DUMMY_WIDTH  8
#define DUMMY_HEIGHT 8
#define DUMMY_FORMAT DSPF_ARGB


#include <core/core_system.h>

DFB_CORE_SYSTEM( dummy )

/**********************************************************************************************************************/

static DFBResult
dummyInitScreen( CoreScreen           *screen,
                 CoreGraphicsDevice   *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     description->caps = DSCCAPS_NONE;

     direct_snputs( description->name, "Dummy", DFB_SCREEN_DESC_NAME_LENGTH );

     return DFB_OK;
}

static DFBResult
dummyGetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     *ret_width  = dfb_config->mode.width  ?: DUMMY_WIDTH;
     *ret_height = dfb_config->mode.height ?: DUMMY_HEIGHT;

     return DFB_OK;
}

static ScreenFuncs dummyScreenFuncs = {
     .InitScreen    = dummyInitScreen,
     .GetScreenSize = dummyGetScreenSize
};

/**********************************************************************************************************************/

static DFBResult
dummyInitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_SYSTEMONLY;
     description->surface_accessor = CSAID_CPU;

     direct_snputs( description->name, "Dummy", DFB_DISPLAY_LAYER_DESC_NAME_LENGTH );


     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;
     config->width       = dfb_config->mode.width  ?: DUMMY_WIDTH;
     config->height      = dfb_config->mode.height ?: DUMMY_HEIGHT;
     config->pixelformat = dfb_config->mode.format ?: DUMMY_FORMAT;

     return DFB_OK;
}

static DFBResult
dummyTestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *ret_failed )
{
     if (ret_failed)
          *ret_failed = DLCONF_NONE;

     return DFB_OK;
}

static DFBResult
dummySetRegion( CoreLayer                  *layer,
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
     return DFB_OK;
}

static DisplayLayerFuncs dummyLayerFuncs = {
     .InitLayer     = dummyInitLayer,
     .TestRegion    = dummyTestRegion,
     .SetRegion     = dummySetRegion
};

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_ANY;
     info->caps = CSCAPS_NONE;

     direct_snputs( info->name, "Dummy", DFB_CORE_SYSTEM_INFO_NAME_LENGTH );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     CoreScreen *screen = dfb_screens_register( NULL, NULL, &dummyScreenFuncs );
     CoreLayer  *layer  = dfb_layers_register( screen, NULL, &dummyLayerFuncs );

     (void) layer;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     CoreScreen *screen = dfb_screens_register( NULL, NULL, &dummyScreenFuncs );
     CoreLayer  *layer  = dfb_layers_register( screen, NULL, &dummyLayerFuncs );

     (void) layer;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     return DFB_OK;
}

static DFBResult
system_resume( void )
{
     return DFB_OK;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
}

static int
system_get_accelerator( void )
{
     return 0;
}

static VideoMode *
system_get_modes( void )
{
     return NULL;
}

static VideoMode *
system_get_current_mode( void )
{
     return NULL;
}

static DFBResult
system_thread_init( void )
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length( void )
{
     return 0;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_auxram_length( void )
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
}

static int
system_surface_data_size( void )
{
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

