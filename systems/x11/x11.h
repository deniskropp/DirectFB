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

#ifndef __X11SYSTEM__X11_H__
#define __X11SYSTEM__X11_H__

#include <fusion/call.h>
#include <fusion/lock.h>
#include <core/layers.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include "x11image.h"
#include "xwindow.h"

typedef struct {
     CoreLayerRegionConfig  config;
} SetModeData;

typedef struct {
     DFBRegion              region;

     CoreSurfaceBufferLock *lock;
} UpdateScreenData;


typedef struct {
    UpdateScreenData update;
    SetModeData      setmode;

    FusionSkirmish   lock;
    FusionCall       call;

    CoreSurfacePool *surface_pool;

    CoreSurface     *primary;
    XWindow         *xw;
    Display         *display;
    Screen*          screenptr;
    int              screennum;

    Visual          *visuals[DFB_NUM_PIXELFORMATS];
} DFBX11;

typedef enum {
     X11_SET_VIDEO_MODE,
     X11_UPDATE_SCREEN,
     X11_SET_PALETTE,
     X11_IMAGE_INIT,
     X11_IMAGE_DESTROY
} DFBX11Call;



DFBResult dfb_x11_set_video_mode_handler( CoreLayerRegionConfig *config );
DFBResult dfb_x11_update_screen_handler ( UpdateScreenData *data );
DFBResult dfb_x11_set_palette_handler   ( CorePalette *palette );

DFBResult dfb_x11_image_init_handler    ( x11Image *image );
DFBResult dfb_x11_image_destroy_handler ( x11Image *image );


extern DFBX11  *dfb_x11;
extern CoreDFB *dfb_x11_core;


#endif //__X11SYSTEM__X11_H__

