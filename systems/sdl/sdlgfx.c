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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <directfb.h>

#include <SDL.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/windows.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( sdlgfx )

#include "sdl.h"

/* FIXME: support for destination color keying */

#define SDL_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define SDL_DRAWING_FUNCTIONS \
               (DFXL_FILLRECTANGLE)

#define SDL_BLITTING_FLAGS \
               (DSBLIT_SRC_COLORKEY)

#define SDL_BLITTING_FUNCTIONS \
               (DFXL_BLIT)

D_DEBUG_DOMAIN( SDL_GFX, "SDL/Graphics", "SDL Graphics" );

typedef struct {
} SDLDriverData;

typedef struct {
     SDL_Surface *dest;
     SDL_Surface *source;

     u32          color;

     bool         color_valid;
     bool         key_valid;
} SDLDeviceData;


static DFBResult sdlEngineSync( void *drv, void *dev )
{
     return DFB_OK;
}

static void sdlCheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     /* check destination format first */
     switch (state->destination->config.format) {
          case DSPF_RGB16:
          case DSPF_RGB32:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          /* if there are no other drawing flags than the supported */
          if (state->drawingflags & ~SDL_DRAWING_FLAGS)
               return;

          state->accel |= SDL_DRAWING_FUNCTIONS;
     }
     else {
          /* if there are no other blitting flags than the supported
             and the source and destination formats are the same */
          if (state->blittingflags & ~SDL_BLITTING_FLAGS)
               return;

          /* check source format */
          switch (state->source->config.format) {
               case DSPF_RGB16:
               case DSPF_RGB32:
                    break;
               default:
                    return;
          }

          state->accel |= SDL_BLITTING_FUNCTIONS;
     }
}

static void sdlSetState( void *drv, void *dev, GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     SDLDeviceData *sdev = (SDLDeviceData*) dev;

     sdev->dest   = state->dst.handle;
     sdev->source = state->src.handle;

     if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS | SMF_SRC_COLORKEY))
          sdev->key_valid = false;

     if (state->modified & (SMF_DESTINATION | SMF_COLOR))
          sdev->color_valid = false;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
               if (!sdev->color_valid) {
                    switch (state->destination->config.format) {
                         case DSPF_RGB16:
                         case DSPF_RGB32:
                              sdev->color = dfb_color_to_pixel( state->destination->config.format,
                                                                state->color.r,
                                                                state->color.g,
                                                                state->color.b );
                              break;

                         default:
                              D_BUG( "unexpected format" );
                    }

                    sdev->color_valid = true;
               }
               break;

          case DFXL_BLIT:
               if (!sdev->key_valid) {
                    SDL_SetColorKey( sdev->source,
                                     (state->blittingflags &
                                      DSBLIT_SRC_COLORKEY) ? SDL_SRCCOLORKEY : 0,
                                     state->src_colorkey );

                    sdev->key_valid = true;
               }
               break;

          default:
               D_BUG("unexpected acceleration" );
               break;
     }

     state->set |= SDL_DRAWING_FUNCTIONS | SDL_BLITTING_FUNCTIONS;

     state->modified = 0;
}

static bool sdlFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     SDLDeviceData *sdev = (SDLDeviceData*) dev;
     SDL_Rect       dr;

     dr.x = rect->x;
     dr.y = rect->y;
     dr.w = rect->w;
     dr.h = rect->h;

     return SDL_FillRect( sdev->dest, &dr, sdev->color ) == 0;
}

static bool sdlBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     SDLDeviceData *sdev = (SDLDeviceData*) dev;
     SDL_Rect       sr, dr;

     D_DEBUG_AT( SDL_GFX, "%s()\n", __FUNCTION__ );

     sr.x = rect->x;
     sr.y = rect->y;
     sr.w = rect->w;
     sr.h = rect->h;

     dr.x = dx;
     dr.y = dy;
     dr.w = rect->w;
     dr.h = rect->h;

     return SDL_BlitSurface( sdev->source, &sr, sdev->dest, &dr ) == 0;
}


/* exported symbols */

static int
driver_probe( CoreGraphicsDevice *device )
{
     return dfb_system_type() == CORE_SDL;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "SDL Graphics Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "directfb.org" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof (SDLDriverData);
     info->device_data_size = sizeof (SDLDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     /* fill acceleration function table */
     funcs->EngineSync    = sdlEngineSync;
     funcs->CheckState    = sdlCheckState;
     funcs->SetState      = sdlSetState;

     funcs->FillRectangle = sdlFillRectangle;
     funcs->Blit          = sdlBlit;

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Graphics" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "SDL" );


     device_info->caps.flags    = CCF_READSYSMEM;
     device_info->caps.accel    = SDL_DRAWING_FUNCTIONS |
                                  SDL_BLITTING_FUNCTIONS;
     device_info->caps.drawing  = SDL_DRAWING_FLAGS;
     device_info->caps.blitting = SDL_BLITTING_FLAGS;

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
}

