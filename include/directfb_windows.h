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

#ifndef __DIRECTFB_WINDOWS_H__
#define __DIRECTFB_WINDOWS_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * The DirectFB Windows interface version.
 */
#define DIRECTFB_WINDOWS_INTERFACE_VERSION  1


/*
 * Window stack extension.
 */
DECLARE_INTERFACE( IDirectFBWindows )


typedef enum {
     DWCONF_NONE                   = 0x00000000,

     DWCONF_POSITION               = 0x00000001,
     DWCONF_SIZE                   = 0x00000002,
     DWCONF_OPACITY                = 0x00000004,
     DWCONF_STACKING               = 0x00000008,

     DWCONF_OPTIONS                = 0x00000010,
     DWCONF_EVENTS                 = 0x00000020,
     DWCONF_ASSOCIATION            = 0x00000040,

     DWCONF_COLOR_KEY              = 0x00000100,
     DWCONF_OPAQUE                 = 0x00000200,
     DWCONF_COLOR                  = 0x00000400,
     DWCONF_STEREO_DEPTH           = 0x00000800,

     DWCONF_KEY_SELECTION          = 0x00001000,
     DWCONF_CURSOR_FLAGS           = 0x00002000,
     DWCONF_CURSOR_RESOLUTION      = 0x00004000,

     DWCONF_SRC_GEOMETRY           = 0x00010000,
     DWCONF_DST_GEOMETRY           = 0x00020000,

     DWCONF_ROTATION               = 0x00040000,

     DWCONF_APPLICATION_ID         = 0x00080000,

     DWCONF_ALL                    = 0x000F7F7F
} DFBWindowConfigFlags;

typedef struct {
     DFBRectangle                  bounds;                  /* position and size */
     int                           opacity;                 /* global alpha factor */
     DFBWindowStackingClass        stacking;                /* level boundaries */

     DFBWindowOptions              options;                 /* flags for appearance/behaviour */
     DFBWindowEventType            events;                  /* mask of enabled events */
     DFBWindowID                   association;             /* ID of window which this is associated to */

     u32                           color_key;               /* transparent pixel */
     DFBRegion                     opaque;                  /* region of the window forced to be opaque */
     DFBColor                      color;                   /* constant color (no surface needed) */

//     DFBWindowKeySelection         key_selection;           /* how to filter keys in focus */
//     DFBInputDeviceKeySymbol      *keys;                    /* list of keys for DWKS_LIST */
//     unsigned int                  num_keys;                /* number of entries in key array */

     DFBWindowCursorFlags          cursor_flags;
     DFBDimension                  cursor_resolution;

     DFBWindowGeometry             src_geometry;            /* advanced source geometry */
     DFBWindowGeometry             dst_geometry;            /* advanced destination geometry */

     int                           rotation;

     u64                           application_id;
     int                           stereo_depth;
} DFBWindowConfig;


typedef enum {
     DWSTATE_NONE                  = 0x00000000,

     DWSTATE_INSERTED              = 0x00000001,

     DWSTATE_FOCUSED               = 0x00000002,            /* only used for GetWindowInfo */
     DWSTATE_ENTERED               = 0x00000004,            /* only used for GetWindowInfo */

     DWSTATE_UPDATING              = 0x00000010,

     DWSTATE_ALL                   = 0x00000017
} DFBWindowStateFlags;

typedef struct {
     DFBWindowStateFlags           flags;
} DFBWindowState;


typedef struct {
     DFBWindowID                   window_id;
     DFBWindowCapabilities         caps;

     u64                           resource_id;


     DFBWindowConfig               config;

     DFBWindowState                state;

     u32                           process_id;              /* Fusion ID or even pid, or something else
                                                               identifying a process. */
     u32                           instance_id;             /* ID of the instance of an application. Each process
                                                               can host a number of applications/instances. */
} DFBWindowInfo;


typedef enum {
     DWREL_TOP,
     DWREL_BOTTOM
} DFBWindowRelation;


/*
 * Windows watcher interface
 */
typedef struct {
     /*
      * Add window,
      *
      * called for each window existing at watcher registration and each added afterwards.
      */
     void (*WindowAdd)     ( void                   *context,
                             const DFBWindowInfo    *info );

     /*
      * Remove window,
      *
      * called for each window being removed.
      */
     void (*WindowRemove)  ( void                   *context,
                             DFBWindowID             window_id );

     /*
      * Change window configuration,
      *
      * called for each window changing its configuration.
      *
      * The flags specify which of the items have changed actually.
      */
     void (*WindowConfig)  ( void                   *context,
                             DFBWindowID             window_id,
                             const DFBWindowConfig  *config,
                             DFBWindowConfigFlags    flags );

     /*
      * Update window state,
      *
      * called for each window changing its state.
      *
      * In case of insertion of a window, prior to this, the watcher will receive the WindowRestack call,
      * which contains the z-position at which the window has been inserted.
      */
     void (*WindowState)   ( void                   *context,
                             DFBWindowID             window_id,
                             const DFBWindowState   *state );

     /*
      * Update window z-position,
      *
      * called for each window changing its z-position.
      *
      * In case of insertion of a window, after this call, the watcher will receive the WindowState call,
      * which indicates insertion of the window.
      *
      * Upon reinsertion, only this call will be received.
      */
     void (*WindowRestack) ( void                   *context,
                             DFBWindowID             window_id,
                             unsigned int            index );

     /*
      * Switch window focus,
      *
      * called for each window getting the focus.
      */
     void (*WindowFocus)   ( void                   *context,
                             DFBWindowID             window_id );
} DFBWindowsWatcher;


/********************
 * IDirectFBWindows *
 ********************/

/*
 * <i>No summary yet...</i>
 */
DEFINE_INTERFACE(   IDirectFBWindows,

   /** Watching **/

     /*
      * Registers a new windows watcher.
      *
      * For any window already existing, the WindowAdded callback will be called immediately.
      */
     DFBResult (*RegisterWatcher) (
          IDirectFBWindows         *thiz,
          const DFBWindowsWatcher  *watcher,
          void                     *context
     );

     /*
      * Unregisters a windows watcher.
      */
     DFBResult (*UnregisterWatcher) (
          IDirectFBWindows         *thiz,
          void                     *context
     );
)


#ifdef __cplusplus
}
#endif

#endif

