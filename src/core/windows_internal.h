/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__WINDOWS_INTERNAL_H__
#define __CORE__WINDOWS_INTERNAL_H__

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/windows.h>

#include <direct/list.h>

#include <fusion/fusion.h>
#include <fusion/lock.h>
#include <fusion/object.h>


typedef enum {
     CWF_NONE        = 0x00000000,

     CWF_INITIALIZED = 0x00000001,
     CWF_FOCUSED     = 0x00000002,
     CWF_ENTERED     = 0x00000004,
     CWF_DESTROYED   = 0x00000008,

     CWF_INSERTED    = 0x00000010,

     CWF_ALL         = 0x0000001F
} CoreWindowFlags;

#define DFB_WINDOW_INITIALIZED(w)  ((w)->flags & CWF_INITIALIZED)
#define DFB_WINDOW_FOCUSED(w)      ((w)->flags & CWF_FOCUSED)
#define DFB_WINDOW_ENTERED(w)      ((w)->flags & CWF_ENTERED)
#define DFB_WINDOW_DESTROYED(w)    ((w)->flags & CWF_DESTROYED)

typedef struct {
     DirectLink              link;

     CoreWindow             *window;
     int                     x;
     int                     y;
} BoundWindow;

/*
 * Core data of a window.
 */
struct __DFB_CoreWindow {
     FusionObject            object;

     int                     magic;

     DFBWindowID             id;

     CoreWindowFlags         flags;

     DFBWindowCapabilities   caps;           /* window capabilities, to enable blending etc. */

     CoreWindowConfig        config;

     CoreSurface            *surface;        /* backing store surface */
     GlobalReaction          surface_reaction;

     CoreWindowStack        *stack;          /* window stack the window belongs */

     CoreLayerRegion        *primary_region; /* default region of context */

     CoreLayerRegion        *region;         /* hardware allocated window */

     void                   *window_data;    /* private data of window manager */

     CoreGraphicsSerial      serial1;
     CoreGraphicsSerial      serial2;

     DirectLink             *bound_windows;  /* list of bound windows */
     CoreWindow             *boundto;        /* window to which this window is bound (window binding) */

     DFBWindowID             toplevel_id;    /* in case of a sub window toplevel_id != 0 */
     CoreWindow             *toplevel;       /* for top level windows this will be NULL */
     FusionVector            subwindows;     /* list of sub windows (only valid for top level windows) */

     CoreWindow             *subfocus;       /* which of the sub windows has the focus? */

     unsigned long           resource_id;

     FusionCall              call;

     struct {
          int                hot_x;
          int                hot_y;
          CoreSurface       *surface;
     } cursor;

     DFBWindowCapabilities   requested_caps; /* original caps from application upon window creation */
};

typedef enum {
     CWSF_NONE           = 0x00000000,

     CWSF_INITIALIZED    = 0x00000001,
     CWSF_ACTIVATED      = 0x00000002,

     CWSF_ALL            = 0x00000003
} CoreWindowStackFlags;

/*
 * Core data of a window stack.
 */
struct __DFB_CoreWindowStack {
     DirectLink          link;

     int                 magic;

     CoreLayerContext   *context;

     int                 width;
     int                 height;

     int                 rotation;

     int                      rotated_width;
     int                      rotated_height;
     DFBSurfaceBlittingFlags  rotated_blit;

     DFBWindowID         id_pool;

     int                 num;

     struct {
          int                enabled;     /* is cursor enabled ? */
          int                x, y;        /* cursor position */
          DFBDimension       size;        /* cursor shape size */
          DFBPoint           hot;         /* hot spot */
          CoreSurface       *surface;     /* shape */
          u8                 opacity;     /* cursor opacity */
          DFBRegion          region;      /* cursor is clipped by this region */

          int                numerator;   /* cursor acceleration */
          int                denominator;
          int                threshold;

          bool               set;         /* cursor enable/disable has
                                             been called at least one time */

          CoreSurfacePolicy  policy;
     } cursor;

     /* stores information on handling the background on exposure */
     struct {
          DFBDisplayLayerBackgroundMode mode;
                                        /* background handling mode:
                                           don't care, solid color or image */

          DFBColor            color;    /* color for solid background mode */
          int                 color_index;


          CoreSurface        *image;    /* surface for background image mode */

          GlobalReaction      image_reaction;
     } bg;

     DirectLink          *devices;      /* input devices attached to the stack */

     bool                 hw_mode;      /* recompositing is done by hardware */

     void                *stack_data;   /* private data of window manager */

     FusionSHMPoolShared *shmpool;

     CoreWindowStackFlags flags;

     FusionCall           call;


     FusionDispatchCleanup  *motion_cleanup;

     DFBInputEvent           motion_x;
     DFBInputEvent           motion_y;
};


DFBResult dfb_wm_close_all_stacks( void *data );


/* global reactions */
ReactionResult _dfb_windowstack_inputdevice_listener     ( const void *msg_data,
                                                           void       *ctx );

ReactionResult _dfb_windowstack_background_image_listener( const void *msg_data,
                                                           void       *ctx );

#endif
