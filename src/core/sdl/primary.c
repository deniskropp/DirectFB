/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <directfb.h>
                                   
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <SDL.h>

#include "sdl.h"


static int
primaryLayerDataSize     ();
     
static DFBResult
primaryInitLayer         ( GraphicsDevice             *device,
                           DisplayLayer               *layer,
                           DisplayLayerInfo           *layer_info,
                           DFBDisplayLayerConfig      *default_config,
                           DFBColorAdjustment         *default_adj,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryEnable            ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryDisable           ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryTestConfiguration ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           DFBDisplayLayerConfigFlags *failed );

static DFBResult
primarySetConfiguration  ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config );

static DFBResult
primarySetOpacity        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        opacity );
     
static DFBResult
primarySetScreenLocation ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           float                       x,
                           float                       y,
                           float                       width,
                           float                       height );
     
static DFBResult
primarySetSrcColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );
     
static DFBResult
primarySetDstColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );
     
static DFBResult
primaryFlipBuffers       ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBSurfaceFlipFlags         flags );
     
static DFBResult
primaryUpdateRegion      ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBRegion                  *region,
                           DFBSurfaceFlipFlags         flags );

static DFBResult
primarySetColorAdjustment( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj );

static DFBResult
primarySetPalette        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           CorePalette                *palette );

static DFBResult
primaryAllocateSurface   ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface               **surface );

static DFBResult
primaryReallocateSurface ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface                *surface );

DisplayLayerFuncs sdlPrimaryLayerFuncs = {
     LayerDataSize:      primaryLayerDataSize,
     InitLayer:          primaryInitLayer,
     Enable:             primaryEnable,
     Disable:            primaryDisable,
     TestConfiguration:  primaryTestConfiguration,
     SetConfiguration:   primarySetConfiguration,
     SetOpacity:         primarySetOpacity,
     SetScreenLocation:  primarySetScreenLocation,
     SetSrcColorKey:     primarySetSrcColorKey,
     SetDstColorKey:     primarySetDstColorKey,
     FlipBuffers:        primaryFlipBuffers,
     UpdateRegion:       primaryUpdateRegion,
     SetColorAdjustment: primarySetColorAdjustment,
     SetPalette:         primarySetPalette,
          
     AllocateSurface:    primaryAllocateSurface,
     ReallocateSurface:  primaryReallocateSurface,
};


static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h );

static SDL_Surface *screen = NULL;



/** primary layer functions **/

static int
primaryLayerDataSize     ()
{
     return 0;
}
     
static DFBResult
primaryInitLayer         ( GraphicsDevice             *device,
                           DisplayLayer               *layer,
                           DisplayLayerInfo           *layer_info,
                           DFBDisplayLayerConfig      *default_config,
                           DFBColorAdjustment         *default_adj,
                           void                       *driver_data,
                           void                       *layer_data )
{
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE;
     layer_info->desc.type = DLTF_GRAPHICS;

     /* set name */
     snprintf( layer_info->desc.name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "SDL Primary Layer" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     default_config->buffermode  = DLBM_BACKSYSTEM;

     if (dfb_config->mode.width)
          default_config->width  = dfb_config->mode.width;
     else
          default_config->width  = 640;

     if (dfb_config->mode.height)
          default_config->height = dfb_config->mode.height;
     else
          default_config->height = 480;
     
     if (dfb_config->mode.format != DSPF_UNKNOWN)
          default_config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          default_config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else
          default_config->pixelformat = DSPF_RGB16;

     skirmish_prevail( &dfb_sdl_lock );
     
     /* Set video mode */
     if ( (screen=SDL_SetVideoMode(default_config->width,
                                   default_config->height,
                                   DFB_BITS_PER_PIXEL(default_config->pixelformat),
                                   SDL_HWSURFACE | SDL_DOUBLEBUF)) == NULL ) {
             ERRORMSG("Couldn't set %dx%dx%d video mode: %s\n",
                      default_config->width, default_config->height,
                      DFB_BITS_PER_PIXEL(default_config->pixelformat), SDL_GetError());
             skirmish_dismiss( &dfb_sdl_lock );
             return DFB_FAILURE;
     }
     
     skirmish_dismiss( &dfb_sdl_lock );
     
     return DFB_OK;
}

static DFBResult
primaryEnable            ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data )
{
     /* always enabled */
     return DFB_OK;
}

static DFBResult
primaryDisable           ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data )
{
     /* cannot be disabled */
     return DFB_UNSUPPORTED;
}

static DFBResult
primaryTestConfiguration ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

/*     if (config->buffermode == DLBM_FRONTONLY)
          fail |= DLCONF_BUFFERMODE;*/

     if (config->buffermode == DLBM_TRIPLE)
          fail |= DLCONF_BUFFERMODE;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primarySetConfiguration  ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config )
{
     Uint32       flags;
     CoreSurface *surface = dfb_layer_surface( layer );

     flags = SDL_HWSURFACE;

     if (config->buffermode == DLBM_TRIPLE)
          return DFB_UNSUPPORTED;

     if (config->buffermode != DLBM_FRONTONLY)
          flags |= SDL_DOUBLEBUF;

     skirmish_prevail( &dfb_sdl_lock );
     
     /* Set video mode */
     if ( (screen=SDL_SetVideoMode(config->width,
                                   config->height,
                                   DFB_BITS_PER_PIXEL(config->pixelformat),
                                   flags)) == NULL ) {
             ERRORMSG("Couldn't set %dx%dx%d video mode: %s\n",
                      config->width, config->height,
                      DFB_BITS_PER_PIXEL(config->pixelformat), SDL_GetError());
             skirmish_dismiss( &dfb_sdl_lock );
             return DFB_FAILURE;
     }

     skirmish_dismiss( &dfb_sdl_lock );
     
     surface->back_buffer->system.addr  = screen->pixels;
     surface->back_buffer->system.pitch = screen->pitch;
     
     surface->front_buffer->system.addr  = screen->pixels;
     surface->front_buffer->system.pitch = screen->pitch;
     
     return DFB_OK;
}

static DFBResult
primarySetOpacity        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        opacity )
{
     /* opacity is not supported for normal primary layer */
     if (opacity != 0xFF)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}
     
static DFBResult
primarySetScreenLocation ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           float                       x,
                           float                       y,
                           float                       width,
                           float                       height )
{
     /* can only be fullscreen (0, 0, 1, 1) */
     if (x != 0  ||  y != 0  ||  width != 1  ||  height != 1)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}
     
static DFBResult
primarySetSrcColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b )
{
     return DFB_UNSUPPORTED;
}
     
static DFBResult
primarySetDstColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b )
{
     return DFB_UNSUPPORTED;
}
     
static DFBResult
primaryFlipBuffers       ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBSurfaceFlipFlags         flags )
{
     CoreSurface *surface = dfb_layer_surface( layer );

     dfb_surface_flip_buffers( surface );

     skirmish_prevail( &dfb_sdl_lock );

     SDL_Flip( screen );

     skirmish_dismiss( &dfb_sdl_lock );

     surface->back_buffer->system.addr  = screen->pixels;
     surface->back_buffer->system.pitch = screen->pitch;

     surface->front_buffer->system.addr  = screen->pixels;
     surface->front_buffer->system.pitch = screen->pitch;
     
     return DFB_OK;
}
     
static DFBResult
primaryUpdateRegion      ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBRegion                  *region,
                           DFBSurfaceFlipFlags         flags )
{
     CoreSurface *surface = dfb_layer_surface( layer );

     if (!region)
          return update_screen( surface,
                                0, 0, surface->width, surface->height );

     return update_screen( surface,
                           region->x1, region->y1,
                           region->x2 - region->x1 + 1,
                           region->y2 - region->y1 + 1 );
}
     
static DFBResult
primarySetColorAdjustment( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj )
{
     /* maybe we could use the gamma ramp here */
     return DFB_UNSUPPORTED;
}

static DFBResult
primarySetPalette ( DisplayLayer               *layer,
                    void                       *driver_data,
                    void                       *layer_data,
                    CorePalette                *palette )
{
     int       i;
     SDL_Color colors[palette->num_entries];

     for (i=0; i<palette->num_entries; i++) {
          colors[i].r = palette->entries[i].r;
          colors[i].g = palette->entries[i].g;
          colors[i].b = palette->entries[i].b;
     }
     
     skirmish_prevail( &dfb_sdl_lock );
     
     SDL_SetColors( screen, colors, 0, palette->num_entries );
     
     skirmish_dismiss( &dfb_sdl_lock );

     return DFB_OK;
}

static DFBResult
primaryAllocateSurface   ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface               **ret_surface )
{
     DFBSurfaceCapabilities caps = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_FLIPPING;

     return dfb_surface_create_preallocated( config->width, config->height,
                                config->pixelformat, CSP_SYSTEMONLY,
                                caps, NULL, screen->pixels, screen->pixels,
                                screen->pitch, screen->pitch, ret_surface );
}

static DFBResult
primaryReallocateSurface ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface                *surface )
{
#if 0
     DFBResult ret;
     
     /* FIXME: write surface management functions
               for easier configuration changes */
     
     switch (config->buffermode) {
          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
               surface->caps |= DSCAPS_FLIPPING;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;

          case DLBM_FRONTONLY:
               surface->caps &= ~DSCAPS_FLIPPING;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;
          
          default:
               BUG("unknown buffermode");
               return DFB_BUG;
     }
     if (ret)
          return ret;

     ret = dfb_surface_reformat( surface, config->width,
                                 config->height, config->pixelformat );
     if (ret)
          return ret;

     if (config->options & DLOP_DEINTERLACING)
          surface->caps |= DSCAPS_INTERLACED;
     else
          surface->caps &= ~DSCAPS_INTERLACED;
#endif

     surface->width  = config->width;
     surface->height = config->height;
     surface->format = config->pixelformat;

     switch (config->buffermode) {
          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
               surface->caps |= DSCAPS_FLIPPING;
               break;

          case DLBM_FRONTONLY:
               surface->caps &= ~DSCAPS_FLIPPING;
               break;
          
          default:
               BUG("unknown buffermode");
               return DFB_BUG;
     }
     
     if (DFB_PIXELFORMAT_IS_INDEXED(config->pixelformat) && !surface->palette) {
          DFBResult    ret;
          CorePalette *palette;
           
          ret = dfb_palette_create( 1 << DFB_BITS_PER_PIXEL( config->pixelformat ),
                                    &palette );
          if (ret)
               return ret;

          if (config->pixelformat == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );
          
          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }
     
     return DFB_OK;
}


/******************************************************************************/

static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h )
{
#if 0
     int          i;
     void        *dst;
     void        *src;
     int          pitch;
     DFBResult    ret;

     DFB_ASSERT( surface != NULL );
     
     if (SDL_LockSurface( screen ) < 0) {
          ERRORMSG( "DirectFB/SDL: "
                    "Couldn't lock the display surface: %s\n", SDL_GetError() );
          return DFB_FAILURE;
     }

     ret = dfb_surface_soft_lock( surface, DSLF_READ, &src, &pitch, true );
     if (ret) {
          ERRORMSG( "DirectFB/SDL: Couldn't lock layer surface: %s\n",
                    DirectFBErrorString( ret ) );
          SDL_UnlockSurface(screen);
          return ret;
     }

     dst = screen->pixels;

     src += DFB_BYTES_PER_LINE( surface->format, x ) + y * pitch;
     dst += DFB_BYTES_PER_LINE( surface->format, x ) + y * screen->pitch;

     for (i=0; i<h; ++i) {
          dfb_memcpy( dst, src,
                      DFB_BYTES_PER_LINE( surface->format, w ) );

          src += pitch;
          dst += screen->pitch;
     }

     dfb_surface_unlock( surface, true );
     
     SDL_UnlockSurface( screen );
#endif     
     
     skirmish_prevail( &dfb_sdl_lock );
     
     SDL_UpdateRect( screen, x, y, w, h );
     
     skirmish_dismiss( &dfb_sdl_lock );
     
     return DFB_OK;
}

