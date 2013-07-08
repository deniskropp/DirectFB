/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
     CoreLayerRegionConfig   config;
     XWindow               **xw;
} SetModeData;

typedef struct {
     bool                   stereo;
     DFBRegion              left_region;
     DFBRegion              right_region;
     CoreSurfaceBufferLock  left_lock;
     CoreSurfaceBufferLock  right_lock;
     XWindow               *xw;
} UpdateScreenData;

typedef struct {
     XWindow               **xw;
} DestroyData;

typedef struct {
     UpdateScreenData     update;
     SetModeData          setmode;
     DestroyData          destroy;
                         
     FusionCall           call;

     FusionSHMPoolShared *data_shmpool;
 
     CoreSurfacePool     *x11image_pool;

     CoreSurfacePool     *glx_pool; /* only used for GL */

     CoreSurfacePool     *vpsmem_pool;
     unsigned int         vpsmem_length;
 
     CoreSurfacePoolBridge *x11_pool_bridge;

     DFBDimension         screen_size;

     int                  window_count; /* merely for optimizing wait loop */

     Bool                 x_error;

     bool                 stereo;
     int                  stereo_width;
} DFBX11Shared;

struct __DFB_X11 {
     DFBX11Shared        *shared;

     CoreDFB             *core;
     CoreScreen          *screen;

     Bool                 use_shm;
     int                  xshm_major;
     int                  xshm_minor;
                         
     Display             *display;
     Screen              *screenptr;
     int                  screennum;

     Visual              *visuals[DFB_NUM_PIXELFORMATS];
     
     void               (*Sync)( DFBX11 *x11 );
};

typedef enum {
     X11_CREATE_WINDOW,
     X11_UPDATE_SCREEN,
     X11_SET_PALETTE,
     X11_IMAGE_INIT,
     X11_IMAGE_DESTROY,
     X11_DESTROY_WINDOW,
} DFBX11Call;



DFBResult dfb_x11_create_window_handler ( DFBX11 *x11, SetModeData *setmode );
DFBResult dfb_x11_destroy_window_handler( DFBX11 *x11, DestroyData *destroy );

DFBResult dfb_x11_update_screen_handler ( DFBX11 *x11, UpdateScreenData *data );
DFBResult dfb_x11_set_palette_handler   ( DFBX11 *x11, CorePalette *palette );

DFBResult dfb_x11_image_init_handler    ( DFBX11 *x11, x11Image *image );
DFBResult dfb_x11_image_destroy_handler ( DFBX11 *x11, x11Image *image );


#endif //__X11SYSTEM__X11_H__

