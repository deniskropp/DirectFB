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

#ifndef __CORE__WINDOWS_INTERNAL_H__
#define __CORE__WINDOWS_INTERNAL_H__

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <direct/list.h>
#include <fusion/lock.h>
#include <fusion/object.h>


/*
 * Hidden capability for software cursor, window will be the "super topmost".
 */
#define DWHC_TOPMOST   0x80000000

typedef enum {
     CWF_NONE        = 0x00000000,

     CWF_INITIALIZED = 0x00000001,
     CWF_FOCUSED     = 0x00000002,
     CWF_ENTERED     = 0x00000004,
     CWF_DESTROYED   = 0x00000008,

     CWF_ALL         = 0x0000000F
} CoreWindowFlags;

#define DFB_WINDOW_INITIALIZED(w)  ((w)->flags & CWF_INITIALIZED)
#define DFB_WINDOW_FOCUSED(w)      ((w)->flags & CWF_FOCUSED)
#define DFB_WINDOW_ENTERED(w)      ((w)->flags & CWF_ENTERED)
#define DFB_WINDOW_DESTROYED(w)    ((w)->flags & CWF_DESTROYED)

/*
 * Core data of a window.
 */
struct __DFB_CoreWindow {
     FusionObject            object;

     DFBWindowID             id;

     int                     x;              /* x position in pixels */
     int                     y;              /* y position in pixels */
     int                     width;          /* width in pixels */
     int                     height;         /* width in pixels */

     CoreWindowFlags         flags;

     DFBRegion               opaque;

     DFBWindowCapabilities   caps;           /* window capabilities, to enable blending etc. */

     DFBWindowOptions        options;        /* flags for appearance/behaviour */
     DFBWindowEventType      events;         /* mask of enabled events */

     DFBWindowStackingClass  stacking;       /* level boundaries */

     __u8                    opacity;        /* global alpha factor */
     __u32                   color_key;      /* transparent pixel */

     CoreSurface            *surface;        /* backing store surface */
     GlobalReaction          surface_reaction;

     CoreWindowStack        *stack;          /* window stack the window belongs */

     CoreLayerRegion        *primary_region; /* default region of context */

     CoreLayerRegion        *region;         /* hardware allocated window */

     void                   *window_data;    /* private data of window manager */
};

/*
 * Core data of a window stack.
 */
struct __DFB_CoreWindowStack {
     CoreLayerContext   *context;

     bool                active;          /* true while context is active */

     int                 width;
     int                 height;

     DFBWindowID         id_pool;

     int                 num;

     struct {
          int            enabled;         /* is cursor enabled ? */
          int            x, y;            /* cursor position */
          CoreWindow    *window;          /* super-toplevel-window
                                             for software cursor */
          __u8           opacity;         /* cursor opacity */
          DFBRegion      region;          /* cursor is clipped by this region */

          int            numerator;       /* cursor acceleration */
          int            denominator;
          int            threshold;

          bool           set;             /* cursor enable/disable has
                                             been called at least one time */
     } cursor;

     /* stores information on handling the background on exposure */
     struct {
          DFBDisplayLayerBackgroundMode mode;
                                       /* background handling mode:
                                          don't care, solid color or image */

          DFBColor            color;   /* color for solid background mode */


          CoreSurface        *image;   /* surface for background image mode */

          GlobalReaction      image_reaction;
     } bg;

     DirectLink         *devices;      /* input devices attached to the stack */

     bool                hw_mode;      /* recompositing is done by hardware */

     void               *stack_data;    /* private data of window manager */
};



/* global reactions */
ReactionResult _dfb_window_surface_listener              ( const void *msg_data,
                                                           void       *ctx );

ReactionResult _dfb_windowstack_inputdevice_listener     ( const void *msg_data,
                                                           void       *ctx );

ReactionResult _dfb_windowstack_background_image_listener( const void *msg_data,
                                                           void       *ctx );

#endif
