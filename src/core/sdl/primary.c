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

#include <config.h>

#include <stdio.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/memcpy.h>

#include <SDL.h>

#include "sdl.h"

extern DFBSDL *dfb_sdl;

static DFBResult dfb_sdl_set_video_mode( DFBDisplayLayerConfig *config );
static DFBResult dfb_sdl_update_screen( DFBRegion *region );
static DFBResult dfb_sdl_set_palette( CorePalette *palette );


static int
primaryLayerDataSize     ();

static DFBResult
primaryInitLayer         ( GraphicsDevice             *device,
                           CoreLayer                  *layer,
                           DisplayLayerInfo           *layer_info,
                           DFBDisplayLayerConfig      *default_config,
                           DFBColorAdjustment         *default_adj,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryEnable            ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryDisable           ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryTestConfiguration ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           DFBDisplayLayerConfigFlags *failed );

static DFBResult
primarySetConfiguration  ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config );

static DFBResult
primarySetOpacity        ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        opacity );

static DFBResult
primarySetScreenLocation ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           float                       x,
                           float                       y,
                           float                       width,
                           float                       height );

static DFBResult
primarySetSrcColorKey    ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );

static DFBResult
primarySetDstColorKey    ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );

static DFBResult
primaryFlipBuffers       ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBSurfaceFlipFlags         flags );

static DFBResult
primaryUpdateRegion      ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBRegion                  *region,
                           DFBSurfaceFlipFlags         flags );

static DFBResult
primarySetColorAdjustment( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj );

static DFBResult
primarySetPalette        ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           CorePalette                *palette );

static DFBResult
primaryAllocateSurface   ( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface               **surface );

static DFBResult
primaryReallocateSurface ( CoreLayer                  *layer,
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
primaryLayerDataSize()
{
     return 0;
}

static DFBResult
primaryInitLayer( GraphicsDevice        *device,
                  CoreLayer             *layer,
                  DisplayLayerInfo      *layer_info,
                  DFBDisplayLayerConfig *default_config,
                  DFBColorAdjustment    *default_adj,
                  void                  *driver_data,
                  void                  *layer_data )
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

     fusion_skirmish_prevail( &dfb_sdl->lock );

     /* Set video mode */
     if ( (screen=SDL_SetVideoMode(default_config->width,
                                   default_config->height,
                                   DFB_BITS_PER_PIXEL(default_config->pixelformat),
                                   SDL_HWSURFACE)) == NULL ) {
             ERRORMSG("Couldn't set %dx%dx%d video mode: %s\n",
                      default_config->width, default_config->height,
                      DFB_BITS_PER_PIXEL(default_config->pixelformat), SDL_GetError());
             fusion_skirmish_dismiss( &dfb_sdl->lock );
             return DFB_FAILURE;
     }

     fusion_skirmish_dismiss( &dfb_sdl->lock );

     return DFB_OK;
}

static DFBResult
primaryEnable( CoreLayer *layer,
               void      *driver_data,
               void      *layer_data )
{
     /* always enabled */
     return DFB_OK;
}

static DFBResult
primaryDisable( CoreLayer *layer,
                void      *driver_data,
                void      *layer_data )
{
     /* cannot be disabled */
     return DFB_UNSUPPORTED;
}

static DFBResult
primaryTestConfiguration( CoreLayer                  *layer,
                          void                       *driver_data,
                          void                       *layer_data,
                          DFBDisplayLayerConfig      *config,
                          DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
               break;

          default:
               fail |= DLCONF_BUFFERMODE;
               break;
     }

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primarySetConfiguration( CoreLayer             *layer,
                         void                  *driver_data,
                         void                  *layer_data,
                         DFBDisplayLayerConfig *config )
{
     DFBResult ret;

     if (config->buffermode == DLBM_TRIPLE)
          return DFB_UNSUPPORTED;

     ret = dfb_sdl_set_video_mode( config );
     if (ret)
          return ret;

     return DFB_OK;
}

static DFBResult
primarySetOpacity( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   __u8       opacity )
{
     /* opacity is not supported for normal primary layer */
     if (opacity != 0xFF)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primarySetScreenLocation( CoreLayer *layer,
                          void      *driver_data,
                          void      *layer_data,
                          float      x,
                          float      y,
                          float      width,
                          float      height )
{
     /* can only be fullscreen (0, 0, 1, 1) */
     if (x != 0  ||  y != 0  ||  width != 1  ||  height != 1)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primarySetSrcColorKey( CoreLayer *layer,
                       void      *driver_data,
                       void      *layer_data,
                       __u8       r,
                       __u8       g,
                       __u8       b )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
primarySetDstColorKey( CoreLayer *layer,
                       void      *driver_data,
                       void      *layer_data,
                       __u8       r,
                       __u8       g,
                       __u8       b )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
primaryFlipBuffers( CoreLayer           *layer,
                    void                *driver_data,
                    void                *layer_data,
                    DFBSurfaceFlipFlags  flags )
{
     CoreSurface *surface = dfb_layer_surface( layer );

     dfb_surface_flip_buffers( surface );

     return dfb_sdl_update_screen( NULL );
}

static DFBResult
primaryUpdateRegion( CoreLayer           *layer,
                     void                *driver_data,
                     void                *layer_data,
                     DFBRegion           *region,
                     DFBSurfaceFlipFlags  flags )
{
     return dfb_sdl_update_screen( region );
}

static DFBResult
primarySetColorAdjustment( CoreLayer          *layer,
                           void               *driver_data,
                           void               *layer_data,
                           DFBColorAdjustment *adj )
{
     /* maybe we could use the gamma ramp here */
     return DFB_UNSUPPORTED;
}

static DFBResult
primarySetPalette( CoreLayer   *layer,
                   void        *driver_data,
                   void        *layer_data,
                   CorePalette *palette )
{
     return dfb_sdl_set_palette( palette );
}

static DFBResult
primaryAllocateSurface( CoreLayer              *layer,
                        void                   *driver_data,
                        void                   *layer_data,
                        DFBDisplayLayerConfig  *config,
                        CoreSurface           **ret_surface )
{
     DFBSurfaceCapabilities caps = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_FLIPPING;

     return dfb_surface_create( config->width, config->height,
                                config->pixelformat, CSP_SYSTEMONLY,
                                caps, NULL, ret_surface );
}

static DFBResult
primaryReallocateSurface( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          DFBDisplayLayerConfig *config,
                          CoreSurface           *surface )
{
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

     SDL_UpdateRect( screen, x, y, w, h );

     return DFB_OK;
}

/******************************************************************************/

typedef enum {
     SDL_SET_VIDEO_MODE,
     SDL_UPDATE_SCREEN,
     SDL_SET_PALETTE
} DFBSDLCall;

static DFBResult
dfb_sdl_set_video_mode_handler( DFBDisplayLayerConfig *config )
{
     fusion_skirmish_prevail( &dfb_sdl->lock );

     /* Set video mode */
     if ( (screen=SDL_SetVideoMode(config->width,
                                   config->height,
                                   DFB_BITS_PER_PIXEL(config->pixelformat),
                                   SDL_HWSURFACE)) == NULL ) {
             ERRORMSG("Couldn't set %dx%dx%d video mode: %s\n",
                      config->width, config->height,
                      DFB_BITS_PER_PIXEL(config->pixelformat), SDL_GetError());

             fusion_skirmish_dismiss( &dfb_sdl->lock );

             return DFB_FAILURE;
     }

     fusion_skirmish_dismiss( &dfb_sdl->lock );

     return DFB_OK;
}

static DFBResult
dfb_sdl_update_screen_handler( DFBRegion *region )
{
     DFBResult    ret;
     CoreSurface *surface = dfb_layer_surface( dfb_layer_at(DLID_PRIMARY) );

     fusion_skirmish_prevail( &dfb_sdl->lock );

     if (!region)
          ret = update_screen( surface, 0, 0, surface->width, surface->height );
     else
          ret = update_screen( surface,
                               region->x1, region->y1,
                               region->x2 - region->x1 + 1,
                               region->y2 - region->y1 + 1 );

     fusion_skirmish_dismiss( &dfb_sdl->lock );

     return DFB_OK;
}

static DFBResult
dfb_sdl_set_palette_handler( CorePalette *palette )
{
     unsigned int i;
     SDL_Color    colors[palette->num_entries];

     for (i=0; i<palette->num_entries; i++) {
          colors[i].r = palette->entries[i].r;
          colors[i].g = palette->entries[i].g;
          colors[i].b = palette->entries[i].b;
     }

     fusion_skirmish_prevail( &dfb_sdl->lock );

     SDL_SetColors( screen, colors, 0, palette->num_entries );

     fusion_skirmish_dismiss( &dfb_sdl->lock );

     return DFB_OK;
}

int
dfb_sdl_call_handler( int   caller,
                      int   call_arg,
                      void *call_ptr,
                      void *ctx )
{
     switch (call_arg) {
          case SDL_SET_VIDEO_MODE:
               return dfb_sdl_set_video_mode_handler( call_ptr );

          case SDL_UPDATE_SCREEN:
               return dfb_sdl_update_screen_handler( call_ptr );

          case SDL_SET_PALETTE:
               return dfb_sdl_set_palette_handler( call_ptr );

          default:
               BUG( "unknown call" );
               break;
     }

     return 0;
}

static DFBResult
dfb_sdl_set_video_mode( DFBDisplayLayerConfig *config )
{
     int                    ret;
     DFBDisplayLayerConfig *tmp = NULL;

     DFB_ASSERT( config != NULL );

     if (dfb_core_is_master())
          return dfb_sdl_set_video_mode_handler( config );

     if (!fusion_is_shared( config )) {
          tmp = SHMALLOC( sizeof(DFBDisplayLayerConfig) );
          if (!tmp)
               return DFB_NOSYSTEMMEMORY;

          dfb_memcpy( tmp, config, sizeof(DFBDisplayLayerConfig) );
     }

     fusion_call_execute( &dfb_sdl->call, SDL_SET_VIDEO_MODE,
                          tmp ? tmp : config, &ret );

     if (tmp)
          SHFREE( tmp );

     return ret;
}

static DFBResult
dfb_sdl_update_screen( DFBRegion *region )
{
     int        ret;
     DFBRegion *tmp = NULL;

     if (dfb_core_is_master())
          return dfb_sdl_update_screen_handler( region );

     if (region) {
          if (!fusion_is_shared( region )) {
               tmp = SHMALLOC( sizeof(DFBRegion) );
               if (!tmp)
                    return DFB_NOSYSTEMMEMORY;

               dfb_memcpy( tmp, region, sizeof(DFBRegion) );
          }
     }

     fusion_call_execute( &dfb_sdl->call, SDL_UPDATE_SCREEN,
                          tmp ? tmp : region, &ret );

     if (tmp)
          SHFREE( tmp );

     return ret;
}

static DFBResult
dfb_sdl_set_palette( CorePalette *palette )
{
     int ret;

     fusion_call_execute( &dfb_sdl->call, SDL_SET_PALETTE,
                          palette, &ret );

     return ret;
}

