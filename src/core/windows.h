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

#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#include <directfb.h>
#include <directfb_windows.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <fusion/object.h>

typedef enum {
     CWMGT_KEYBOARD,
     CWMGT_POINTER,
     CWMGT_KEY,
     CWMGT_UNSELECTED_KEYS,
} CoreWMGrabTarget;

#define CoreWindowConfigFlags DFBWindowConfigFlags

#define CWCF_NONE                  DWCONF_NONE
#define CWCF_POSITION              DWCONF_POSITION
#define CWCF_SIZE                  DWCONF_SIZE
#define CWCF_OPACITY               DWCONF_OPACITY
#define CWCF_STACKING              DWCONF_STACKING
#define CWCF_OPTIONS               DWCONF_OPTIONS
#define CWCF_EVENTS                DWCONF_EVENTS
#define CWCF_ASSOCIATION           DWCONF_ASSOCIATION
#define CWCF_COLOR_KEY             DWCONF_COLOR_KEY
#define CWCF_OPAQUE                DWCONF_OPAQUE
#define CWCF_COLOR                 DWCONF_COLOR
#define CWCF_STEREO_DEPTH          DWCONF_STEREO_DEPTH
#define CWCF_KEY_SELECTION         DWCONF_KEY_SELECTION
#define CWCF_CURSOR_FLAGS          DWCONF_CURSOR_FLAGS
#define CWCF_CURSOR_RESOLUTION     DWCONF_CURSOR_RESOLUTION
#define CWCF_SRC_GEOMETRY          DWCONF_SRC_GEOMETRY
#define CWCF_DST_GEOMETRY          DWCONF_DST_GEOMETRY
#define CWCF_ROTATION              DWCONF_ROTATION
#define CWCF_APPLICATION_ID        DWCONF_APPLICATION_ID
#define CWCF_ALL                   DWCONF_ALL

struct __DFB_CoreWindowConfig {
     DFBRectangle             bounds;         /* position and size */
     int                      opacity;        /* global alpha factor */
     DFBWindowStackingClass   stacking;       /* level boundaries */
     DFBWindowOptions         options;        /* flags for appearance/behaviour */
     DFBWindowEventType       events;         /* mask of enabled events */
     DFBColor                 color;          /* color for DWCAPS_COLOR, never premultiplied! */
     u32                      color_key;      /* transparent pixel */
     DFBRegion                opaque;         /* region of the window forced to be opaque */
     int                      z;              /* stereoscopic offset used to establish perceived depth */

     DFBWindowKeySelection    key_selection;  /* how to filter keys in focus */
     DFBInputDeviceKeySymbol *keys;           /* list of keys for DWKS_LIST */
     unsigned int             num_keys;       /* number of entries in key array */

     DFBWindowGeometry        src_geometry;   /* advanced source geometry */
     DFBWindowGeometry        dst_geometry;   /* advanced destination geometry */

     int                      rotation;

     DFBWindowID              association;

     unsigned long            application_id; /* this can be changed at runtime by the application.
                                               * it's here so appman can get a callback on change. */

     DFBWindowCursorFlags     cursor_flags;
     DFBDimension             cursor_resolution;
};


#define TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                               (w)->config.options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING))

#define VISIBLE_WINDOW(w)     (!((w)->caps & DWCAPS_INPUTONLY) && \
                               (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED((w)))


/*
 * Creates a pool of window objects.
 */
FusionObjectPool *dfb_window_pool_create( const FusionWorld *world );

/*
 * Generates dfb_window_ref(), dfb_window_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreWindow, dfb_window )

/*
 * creates a window on a given stack
 */
DFBResult
dfb_window_create( CoreWindowStack             *stack,
                   const DFBWindowDescription  *desc,
                   CoreWindow                 **ret_window );

/*
 * deinitializes a window and removes it from the window stack
 */
void
dfb_window_destroy( CoreWindow *window );

/*
 * moves a window relative to its current position
 */
DFBResult
dfb_window_move( CoreWindow *window,
                 int         x,
                 int         y,
                 bool        relative );

/*
 * set position and size
 */
DFBResult
dfb_window_set_bounds( CoreWindow *window,
                       int         x,
                       int         y,
                       int         width,
                       int         height );

/*
 * resizes a window
 */
DFBResult
dfb_window_resize( CoreWindow   *window,
                   int           width,
                   int           height );

/*
 * binds a window to this window
 */
DFBResult
dfb_window_bind( CoreWindow *window,
                 CoreWindow *source,
                 int         x,
                 int         y );

/*
 * unbinds a window from this window
 */
DFBResult
dfb_window_unbind( CoreWindow *window,
                   CoreWindow *source );

/*
 * changes stacking class
 */
DFBResult
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking );

/*
 * move a window up one step in window stack
 */
DFBResult
dfb_window_raise( CoreWindow *window );

/*
 * move a window down one step in window stack
 */
DFBResult
dfb_window_lower( CoreWindow *window );

/*
 * makes a window the first (topmost) window in the window stack
 */
DFBResult
dfb_window_raisetotop( CoreWindow *window );

/*
 * makes a window the last (downmost) window in the window stack
 */
DFBResult
dfb_window_lowertobottom( CoreWindow *window );

/*
 * stacks the window on top of another one
 */
DFBResult
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower );

/*
 * stacks the window below another one
 */
DFBResult
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper );

/*
 * sets the source color key
 */
DFBResult
dfb_window_set_color( CoreWindow *window,
                      DFBColor    color );

/*
 * sets the source color key
 */
DFBResult
dfb_window_set_colorkey( CoreWindow *window,
                         u32         color_key );

/*
 * change window configuration
 */
DFBResult
dfb_window_set_config( CoreWindow             *window,
                       const CoreWindowConfig *config,
                       CoreWindowConfigFlags   flags );

/*
 * change window cursor
 */
DFBResult
dfb_window_set_cursor_shape( CoreWindow   *window,
                             CoreSurface  *surface,
                             unsigned int  hot_x,
                             unsigned int  hot_y );

/*
 * sets the global alpha factor
 */
DFBResult
dfb_window_set_opacity( CoreWindow *window,
                        u8          opacity );

/*
 * sets the window options
 */
DFBResult
dfb_window_change_options( CoreWindow       *window,
                           DFBWindowOptions  disable,
                           DFBWindowOptions  enable );

/*
 * sets the window options
 */
DFBResult
dfb_window_set_opaque( CoreWindow      *window,
                       const DFBRegion *region );

/*
 * manipulates the event mask
 */
DFBResult
dfb_window_change_events( CoreWindow         *window,
                          DFBWindowEventType  disable,
                          DFBWindowEventType  enable );

/*
 * repaints part of a window, if region is NULL the whole window is repainted 
 * right_region is ignored for all but stereo windows 
 */
DFBResult
dfb_window_repaint( CoreWindow          *window,
                    const DFBRegion     *left_region,
                    const DFBRegion     *right_region,
                    DFBSurfaceFlipFlags  flags );

/*
 * request a window to gain focus
 */
DFBResult
dfb_window_request_focus( CoreWindow *window );

DFBResult dfb_window_set_key_selection( CoreWindow                    *window,
                                        DFBWindowKeySelection          selection,
                                        const DFBInputDeviceKeySymbol *keys,
                                        unsigned int                   num_keys );

DFBResult dfb_window_change_grab    ( CoreWindow                 *window,
                                      CoreWMGrabTarget            target,
                                      bool                        grab );
DFBResult dfb_window_grab_key       ( CoreWindow                 *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers );
DFBResult dfb_window_ungrab_key     ( CoreWindow                 *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers );

void dfb_window_post_event( CoreWindow *window, DFBWindowEvent *event );

DFBResult dfb_window_send_configuration( CoreWindow *window );

DFBWindowID dfb_window_id( const CoreWindow *window );

CoreSurface *dfb_window_surface( const CoreWindow *window );

DFBResult
dfb_window_set_rotation( CoreWindow *window, int rotation );
#endif
