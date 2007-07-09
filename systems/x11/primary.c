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

#include <fusion/types.h>

#include <stdio.h>

#include <directfb.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>


#include <string.h>
#include <stdlib.h>

#include "xwindow.h"
#include "x11.h"
#include "primary.h"




extern DFBX11  *dfb_x11;
extern CoreDFB *dfb_x11_core;

/******************************************************************************/

static DFBResult dfb_x11_set_video_mode( CoreDFB *core, CoreLayerRegionConfig *config );
static DFBResult dfb_x11_update_screen( CoreDFB *core, DFBRegion *region );
static DFBResult dfb_x11_set_palette( CorePalette *palette );

static DFBResult update_screen( CoreSurface *surface,
                                int x, int y, int w, int h );



static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   GraphicsDevice       *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_NONE;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "X11 Primary Screen" );

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_ASSERT( dfb_x11 != NULL );

     if (dfb_x11->primary) {
          *ret_width  = dfb_x11->primary->width;
          *ret_height = dfb_x11->primary->height;
     }
     else {
          if (dfb_config->mode.width)
               *ret_width  = dfb_config->mode.width;
          else
               *ret_width  = 640;

          if (dfb_config->mode.height)
               *ret_height = dfb_config->mode.height;
          else
               *ret_height = 480;
     }

     return DFB_OK;
}

ScreenFuncs x11PrimaryScreenFuncs = {
     InitScreen:    primaryInitScreen,
     GetScreenSize: primaryGetScreenSize
};

/******************************************************************************/

static int
primaryLayerDataSize()
{
     return 0;
}

static int
primaryRegionDataSize()
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
     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "X11 Primary Layer" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode  = DLBM_FRONTONLY;

     if (dfb_config->mode.width)
          config->width  = dfb_config->mode.width;
     else
          config->width  = 640;

     if (dfb_config->mode.height)
          config->height = dfb_config->mode.height;
     else
          config->height = 480;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else {
          Display *display =XOpenDisplay(NULL);
          int depth=DefaultDepth(display,DefaultScreen(display));
          XCloseDisplay(display);
          switch (depth) {
               case 16:
                    config->pixelformat = DSPF_RGB16;
                    break;
               case 24:
                    config->pixelformat = DSPF_RGB32;
                    break;
               default:
                    printf(" Unsupported X11 screen depth %d \n",depth);
                    exit(-1);
                    break;
          }
     }

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

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
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
                  CorePalette                *palette )
{
     DFBResult ret;

     ret = dfb_x11_set_video_mode( dfb_x11_core, config );
     if (ret)
          return ret;

     if (surface)
          dfb_x11->primary = surface;

     if (palette)
          dfb_x11_set_palette( palette );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     dfb_x11->primary = NULL;

     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer           *layer,
                   void                *driver_data,
                   void                *layer_data,
                   void                *region_data,
                   CoreSurface         *surface,
                   DFBSurfaceFlipFlags  flags )
{
     dfb_surface_flip_buffers( surface, false );

     return dfb_x11_update_screen( dfb_x11_core, NULL );
}

static DFBResult
primaryUpdateRegion( CoreLayer           *layer,
                     void                *driver_data,
                     void                *layer_data,
                     void                *region_data,
                     CoreSurface         *surface,
                     const DFBRegion     *update )
{
     if (update) {
          DFBRegion region = *update;

          return dfb_x11_update_screen( dfb_x11_core, &region );
     }

     return dfb_x11_update_screen( dfb_x11_core, NULL );
}

static DFBResult
primaryAllocateSurface( CoreLayer              *layer,
                        void                   *driver_data,
                        void                   *layer_data,
                        void                   *region_data,
                        CoreLayerRegionConfig  *config,
                        CoreSurface           **ret_surface )
{
     DFBSurfaceCapabilities caps = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_DOUBLE;

     return dfb_surface_create( NULL, config->width, config->height,
                                config->format, CSP_SYSTEMONLY,
                                caps, NULL, ret_surface );
}

static DFBResult
primaryReallocateSurface( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          void                  *region_data,
                          CoreLayerRegionConfig *config,
                          CoreSurface           *surface )
{
     DFBResult ret;

     /* FIXME: write surface management functions
               for easier configuration changes */

     switch (config->buffermode) {
          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
               surface->caps |= DSCAPS_DOUBLE;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;

          case DLBM_FRONTONLY:
               surface->caps &= ~DSCAPS_DOUBLE;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }
     if (ret)
          return ret;

     ret = dfb_surface_reformat( NULL, surface, config->width,
                                 config->height, config->format );
     if (ret)
          return ret;


     if (DFB_PIXELFORMAT_IS_INDEXED(config->format) && !surface->palette) {
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( NULL,    /* FIXME */
                                    1 << DFB_COLOR_BITS_PER_PIXEL( config->format ),
                                    &palette );
          if (ret)
               return ret;

          if (config->format == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     return DFB_OK;
}

DisplayLayerFuncs x11PrimaryLayerFuncs = {
     LayerDataSize:     primaryLayerDataSize,
     RegionDataSize:    primaryRegionDataSize,
     InitLayer:         primaryInitLayer,

     TestRegion:        primaryTestRegion,
     AddRegion:         primaryAddRegion,
     SetRegion:         primarySetRegion,
     RemoveRegion:      primaryRemoveRegion,
     FlipRegion:        primaryFlipRegion,
     UpdateRegion:      primaryUpdateRegion,

     AllocateSurface:   primaryAllocateSurface,
     ReallocateSurface: primaryReallocateSurface
};

/******************************************************************************/

static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h )
{
     int          i;
     void        *dst;
     void        *src;
     int          pitch;
     DFBResult    ret;
     XWindow     *xw = dfb_x11->xw;

     D_ASSERT( surface != NULL );
     ret = dfb_surface_soft_lock( dfb_x11_core, surface, DSLF_READ, &src, &pitch, true );
     if (ret) {
          D_ERROR( "DirectFB/X11: Couldn't lock layer surface: %s\n",
                   DirectFBErrorString( ret ) );
          return ret;
     }

     xw->ximage_offset = xw->ximage_offset ? 0 : (xw->ximage->height / 2);

     dst = xw->virtualscreen + xw->ximage_offset * xw->ximage->bytes_per_line;

     src += DFB_BYTES_PER_LINE( surface->format, x ) + y * pitch;
     dst += x * xw->bpp + y * xw->ximage->bytes_per_line;

     switch (xw->depth) {
          case 16:
               dfb_convert_to_rgb16( surface->format, src, pitch,
                                     surface->height, dst, xw->ximage->bytes_per_line, w, h );
               break;

          case 24:
               if (xw->bpp == 4)
                    dfb_convert_to_rgb32( surface->format, src, pitch,
                                          surface->height, dst, xw->ximage->bytes_per_line, w, h );
               break;

          default:
               D_ONCE( "unsupported depth %d", xw->depth );
     }

     dfb_surface_unlock( surface, true );

     XSync(xw->display, False);

     XShmPutImage(xw->display, xw->window, xw->gc, xw->ximage,
                  x, xw->ximage_offset + y, x, y, w, h, False);

     return DFB_OK;
}

/******************************************************************************/

typedef enum {
     X11_SET_VIDEO_MODE,
     X11_UPDATE_SCREEN,
     X11_SET_PALETTE
} DFBX11Call;

static DFBResult
dfb_x11_set_video_mode_handler( CoreLayerRegionConfig *config )
{
     XWindow *xw = dfb_x11->xw;

     if (xw != NULL) {
          if (xw->width == config->width && xw->height == config->height)
               return DFB_OK;

          dfb_x11_close_window( xw );
          dfb_x11->xw = NULL;
     }

     bool bSucces = dfb_x11_open_window(&xw, 0, 0, config->width, config->height);

     /* Set video mode */
     if ( !bSucces ) {
          D_ERROR( "ML: DirectFB/X11: Couldn't open %dx%d window: %s\n",
                   config->width, config->height, "X11 error!");

          fusion_skirmish_dismiss( &dfb_x11->lock );
          return DFB_FAILURE;
     }
     else
          dfb_x11->xw = xw;

     return DFB_OK;
}

static DFBResult
dfb_x11_update_screen_handler( const DFBRegion *region )
{
     DFBResult    ret;
     CoreSurface *surface = dfb_x11->primary;

     if (!region)
          ret = update_screen( surface, 0, 0, surface->width, surface->height );
     else
          ret = update_screen( surface,
                               region->x1,  region->y1,
                               region->x2 - region->x1 + 1,
                               region->y2 - region->y1 + 1 );

     return DFB_OK;
}

static DFBResult
dfb_x11_set_palette_handler( CorePalette *palette )
{
     return DFB_OK;
}

FusionCallHandlerResult
dfb_x11_call_handler( int           caller,
                      int           call_arg,
                      void         *call_ptr,
                      void         *ctx,
                      unsigned int  serial,
                      int          *ret_val )
{
     switch (call_arg) {
          case X11_SET_VIDEO_MODE:
               *ret_val = dfb_x11_set_video_mode_handler( call_ptr );
               break;

          case X11_UPDATE_SCREEN:
               *ret_val = dfb_x11_update_screen_handler( call_ptr );
               break;

          case X11_SET_PALETTE:
               *ret_val = dfb_x11_set_palette_handler( call_ptr );
               break;

          default:
               D_BUG( "unknown call" );
               *ret_val = DFB_BUG;
               break;
     }

     return FCHR_RETURN;
}

static DFBResult
dfb_x11_set_video_mode( CoreDFB *core, CoreLayerRegionConfig *config )
{
     int                    ret;
     CoreLayerRegionConfig *tmp = NULL;


     D_ASSERT( config != NULL );

     if (dfb_core_is_master( core ))
          return dfb_x11_set_video_mode_handler( config );

     if (!fusion_is_shared( dfb_core_world(core), config )) {
          tmp = SHMALLOC( dfb_core_shmpool(core), sizeof(CoreLayerRegionConfig) );
          if (!tmp)
               return D_OOSHM();

          direct_memcpy( tmp, config, sizeof(CoreLayerRegionConfig) );
     }

     fusion_call_execute( &dfb_x11->call, FCEF_NODIRECT, X11_SET_VIDEO_MODE,
                          tmp ? tmp : config, &ret );

     if (tmp)
          SHFREE( dfb_core_shmpool(core), tmp );

     return ret;
}

static DFBResult
dfb_x11_update_screen( CoreDFB *core, DFBRegion *region )
{
     int        ret;
     DFBRegion *tmp = NULL;

     if (dfb_core_is_master( core ))
          return dfb_x11_update_screen_handler( region );

     if (region) {
          if (!fusion_is_shared( dfb_core_world(core), region )) {
               tmp = SHMALLOC( dfb_core_shmpool(core), sizeof(DFBRegion) );
               if (!tmp)
                    return D_OOSHM();

               direct_memcpy( tmp, region, sizeof(DFBRegion) );
          }
     }

     fusion_call_execute( &dfb_x11->call, FCEF_NONE, X11_UPDATE_SCREEN,
                          tmp ? tmp : region, &ret );

     if (tmp)
          SHFREE( dfb_core_shmpool(core), tmp );

     return DFB_OK;
}

static DFBResult
dfb_x11_set_palette( CorePalette *palette )
{
     int ret;

     fusion_call_execute( &dfb_x11->call, FCEF_NODIRECT, X11_SET_PALETTE,
                          palette, &ret );

     return ret;
}

