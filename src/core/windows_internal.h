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
#define DWHC_TOPMOST     0x80000000

/*
 * a window
 */
struct __DFB_CoreWindow {
     FusionObject            object;

     DFBWindowID             id;

     int                     fid;

     int                     x;            /* x position in pixels */
     int                     y;            /* y position in pixels */
     int                     width;        /* width in pixels */
     int                     height;       /* width in pixels */

     DFBRegion               opaque;

     DFBWindowCapabilities   caps;         /* window capabilities, to enable
                                              blending etc. */

     DFBWindowOptions        options;      /* flags for appearance/behaviour */
     DFBWindowEventType      events;       /* mask of enabled events */

     DFBWindowStackingClass  stacking;

     __u8                    opacity;      /* global alpha factor */
     __u32                   color_key;    /* transparent pixel */

     CoreSurface            *surface;      /* backing store surface */
     GlobalReaction          surface_reaction;

     CoreWindowStack        *stack;        /* window stack the window belongs */

     bool                    initialized;  /* window has been inserted into
                                              the stack */
     bool                    destroyed;    /* window is (being) destroyed */

     CoreLayerRegion        *primary_region; /* default region of context */

     CoreLayerRegion        *region;       /* hardware allocated window */
};

/*
 * a window stack
 */
struct __DFB_CoreWindowStack {
     CoreLayerContext   *context;

     bool                active;          /* true while context is active */

     int                 width;
     int                 height;

     DFBWindowID         id_pool;

     int                 num_windows;     /* number of windows on the stack */
     CoreWindow        **windows;         /* array of windows */

     CoreWindow         *pointer_window;  /* window grabbing the pointer */
     CoreWindow         *keyboard_window; /* window grabbing the keyboard */
     CoreWindow         *focused_window;  /* window having the focus */
     CoreWindow         *entered_window;  /* window under the pointer */

     DirectLink         *grabbed_keys;    /* List of currently grabbed keys. */

     struct {
          DFBInputDeviceKeySymbol      symbol;
          DFBInputDeviceKeyIdentifier  id;
          int                          code;
          CoreWindow                  *owner;
     } keys[8];

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
