/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrj��<syrjala@sci.fi>.

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


int 	g_pixelFormatRequested; /* Pixelformat requested by DFB, see dfb_x11_set_video_mode_handler in primary.c  */

XWindow* xw; 


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
     .InitScreen    = primaryInitScreen,
     .GetScreenSize = primaryGetScreenSize
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
		  switch(depth) {
			case 16:
          	config->pixelformat = DSPF_RGB16;
			break;
			case 24:
          	/*config->pixelformat = DSPF_RGB24;
			break;
			*/
			case 32:
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
     .LayerDataSize     = primaryLayerDataSize,
     .RegionDataSize    = primaryRegionDataSize,
     .InitLayer         = primaryInitLayer,

     .TestRegion        = primaryTestRegion,
     .AddRegion         = primaryAddRegion,
     .SetRegion         = primarySetRegion,
     .RemoveRegion      = primaryRemoveRegion,
     .FlipRegion        = primaryFlipRegion,
     .UpdateRegion      = primaryUpdateRegion,

     .AllocateSurface   = primaryAllocateSurface,
     .ReallocateSurface = primaryReallocateSurface
};

/******************************************************************************/

static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h )
{



//     printf("UpdateScreen (%d, %d)\n", surface->width, surface->height);
//	 printf("x, y, w, h; %d, %d, %d, %d \n", x , y, w, h);
//	 printf("DFB_BYTES_PER_LINE: %d\n", DFB_BYTES_PER_LINE( surface->format, w ));

    int          i;
    void        *dst;
	void        *src;
	int          pitch;
	DFBResult    ret;

	D_ASSERT( surface != NULL );
	ret = dfb_surface_soft_lock( surface, DSLF_READ, &src, &pitch, true );
	if (ret) {
		D_ERROR( "DirectFB/X11: Couldn't lock layer surface: %s\n",
				  DirectFBErrorString( ret ) );
		return ret;
	}

		
	dst = xw->virtualscreen;

	src += DFB_BYTES_PER_LINE( surface->format, x ) + y * pitch;
	dst += DFB_BYTES_PER_LINE( surface->format, x ) + y * xw->ximage->bytes_per_line;

	for (i=0; i<h; ++i) {
		direct_memcpy( dst, src, DFB_BYTES_PER_LINE( surface->format, w ) );

		src += pitch;
		dst += xw->ximage->bytes_per_line;
	}
	
	dfb_surface_unlock( surface, true );

	XShmPutImage(xw->display, xw->window, xw->gc, xw->ximage,
				 0, 0, 0, 0, xw->width, xw->height, False);
	XFlush(xw->display);        /* flush the ouput buffer*/
	

	



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
	printf("dfb_x11_set_video_mode_handler\n");
    fusion_skirmish_prevail( &dfb_x11->lock );

	
	// Match DFB requested pixelformat (if possible )	
	
// 	printf("Match DFB requested pixelformat (if possible )	\n");
// 	switch (config->format)
// 	{
// 		case DSPF_ARGB1555 :	
// 			/* 16 bit  ARGB (2 byte, alpha 1@15, red 5@10, green 5@5, blue 5@0) */
// 			g_pixelFormatRequested	= MWPF_TRUECOLOR555;
// 			break;
// 		case DSPF_RGB16 :	
// 			/* 16 bit   RGB (2 byte, red 5@11, green 6@5, blue 5@0) */
// 			g_pixelFormatRequested	= MWPF_TRUECOLOR565;
// 			break;
// 		case DSPF_RGB24 :	
// 			/* 24 bit   RGB (3 byte, red 8@16, green 8@8, blue 8@0) */
// 			g_pixelFormatRequested	= MWPF_TRUECOLOR888;
// 			break;
// 		case DSPF_RGB32 :	
// 			/* 24 bit   RGB (4 byte, nothing@24, red 8@16, green 8@8, blue 8@0) */
// 			g_pixelFormatRequested	= MWPF_TRUECOLOR0888;
// 			break;
// 		case DSPF_ARGB :	
// 			/* 32 bit  ARGB (4 byte, alpha 8@24, red 8@16, green 8@8, blue 8@0) */
// 			g_pixelFormatRequested	= MWPF_TRUECOLOR0888;
// 			break;
// 		default:
// 			/* 8 bit palette */
// 			g_pixelFormatRequested	= MWPF_PALETTE;
// 	}
	
	bool bSucces =	xw_openWindow(&xw, 0, 0, config->width, config->height, 
	                              DFB_COLOR_BITS_PER_PIXEL(config->format));
	
	/* Set video mode */
	if ( !bSucces )
	 {
		 D_ERROR( "ML: DirectFB/X11: Couldn't set %dx%dx%d video mode: %s\n",
				  config->width, config->height,
				  DFB_COLOR_BITS_PER_PIXEL(config->format), "X11 error!");

		 fusion_skirmish_dismiss( &dfb_x11->lock );

		 return DFB_FAILURE;
	 }
	 fusion_skirmish_dismiss( &dfb_x11->lock );
	     
     
     return DFB_OK;
}
 
static DFBResult
dfb_x11_update_screen_handler( const DFBRegion *region )
{
     DFBResult    ret;
     CoreSurface *surface = dfb_x11->primary;
 
     fusion_skirmish_prevail( &dfb_x11->lock );
 
     if (!region)
          ret = update_screen( surface, 0, 0, surface->width, surface->height );
     else
          ret = update_screen( surface,
                               region->x1,  region->y1,
                               region->x2 - region->x1 + 1,
                               region->y2 - region->y1 + 1 );

     fusion_skirmish_dismiss( &dfb_x11->lock );

     return DFB_OK;
}

static DFBResult
dfb_x11_set_palette_handler( CorePalette *palette )
{
     printf("dfb_x11_set_palette_handler\n");
     return DFB_OK;
}

int
dfb_x11_call_handler( int   caller,
                      int   call_arg,
                      void *call_ptr,
                      void *ctx )
{
     printf("dfb_x11_call_handler\n");
     switch (call_arg) {
          case X11_SET_VIDEO_MODE:
               return dfb_x11_set_video_mode_handler( call_ptr );

          case X11_UPDATE_SCREEN:
               return dfb_x11_update_screen_handler( call_ptr );

          case X11_SET_PALETTE:
               return dfb_x11_set_palette_handler( call_ptr );

          default:
               D_BUG( "unknown call" );
               break;
     }

     return 0;
}

static DFBResult
dfb_x11_set_video_mode( CoreDFB *core, CoreLayerRegionConfig *config )
{
     int                    ret;
     CoreLayerRegionConfig *tmp = NULL;
     
     printf("dfb_x11_set_video_mode\n");

     D_ASSERT( config != NULL );

     if (dfb_core_is_master( core ))
          return dfb_x11_set_video_mode_handler( config );

     if (!fusion_is_shared( dfb_core_world(core), config )) {
          tmp = SHMALLOC( dfb_core_shmpool(core), sizeof(CoreLayerRegionConfig) );
          if (!tmp)
               return D_OOSHM();

          direct_memcpy( tmp, config, sizeof(CoreLayerRegionConfig) );
     }

     fusion_call_execute( &dfb_x11->call, X11_SET_VIDEO_MODE,
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
//     printf("dfb_x11_update_screen\n");

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

     fusion_call_execute( &dfb_x11->call, X11_UPDATE_SCREEN,
                          tmp ? tmp : region, &ret );

     if (tmp)
          SHFREE( dfb_core_shmpool(core), tmp );

     return ret;
}

static DFBResult
dfb_x11_set_palette( CorePalette *palette )
{
     int ret;
     printf("dfb_x11_set_palette\n");

     fusion_call_execute( &dfb_x11->call, X11_SET_PALETTE,
                          palette, &ret );

     return ret;
}

