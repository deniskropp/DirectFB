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

#ifndef __UNIQUE__INTERNAL_H__
#define __UNIQUE__INTERNAL_H__

#include <directfb.h>

#include <direct/list.h>

#include <fusion/object.h>
#include <fusion/vector.h>

#include <core/coretypes.h>
#include <core/windows.h>

#include <unique/context.h>
#include <unique/device.h>
#include <unique/stret.h>
#include <unique/types.h>
#include <unique/window.h>


#define UNIQUE_WM_ABI_VERSION 7


extern const StretRegionClass unique_root_region_class;
extern const StretRegionClass unique_frame_region_class;
extern const StretRegionClass unique_window_region_class;
extern const StretRegionClass unique_foo_region_class;

extern const UniqueDeviceClass unique_pointer_device_class;
extern const UniqueDeviceClass unique_wheel_device_class;
extern const UniqueDeviceClass unique_keyboard_device_class;

typedef enum {
     URCI_ROOT,
     URCI_FRAME,
     URCI_WINDOW,
     URCI_FOO,

     _URCI_NUM
} UniqueRegionClassIndex;

typedef enum {
     UDCI_POINTER,
     UDCI_WHEEL,
     UDCI_KEYBOARD,

     _UDCI_NUM
} UniqueDeviceClassIndex;

typedef enum {
     UFI_N,
     UFI_NE,
     UFI_E,
     UFI_SE,
     UFI_S,
     UFI_SW,
     UFI_W,
     UFI_NW
} UniqueFooIndex;

typedef ReactionResult (*UniqueWMContextNotify)( WMData                          *data,
                                                 const UniqueContextNotification *notification,
                                                 void                            *ctx );

typedef ReactionResult (*UniqueWMWindowNotify) ( WMData                          *data,
                                                 const UniqueWindowNotification  *notification,
                                                 void                            *ctx );


struct __UniQuE_WMData {
     int                           module_abi;

     CoreDFB                      *core;

     WMShared                     *shared;

     UniqueWMContextNotify         context_notify;
     UniqueWMWindowNotify          window_notify;
};

struct __UniQuE_WMShared {
     int                           magic;

     FusionObjectPool             *context_pool;
     FusionObjectPool             *window_pool;

     StretRegionClassID            region_classes[_URCI_NUM];
     UniqueDeviceClassID           device_classes[_UDCI_NUM];

     int                           device_listener;    /* react index of the registered global */

     DFBInsets                     insets;

     DFBRectangle                  foo_rects[8];

     CoreSurface                  *foo_surface;
};


typedef enum {
     UNRL_DESKTOP,       /* Icons, redirected fullscreen apps (force-desktop) */
     UNRL_USER,          /* User windows (all currently available stacking classes) */
     UNRL_SYSTEM,        /* Dock/Panel, Glass, Expos?, Clipboard, Virtual Keyboard, IMs */
     UNRL_CURSOR,        /* Cursor shape and attached objects, e.g. Drag'N'Drop */
     UNRL_SCREEN,        /* Display Locking, Screensaver */

     _UNRL_NUM
} UniqueRootLevel;

typedef enum {
     UNFL_BACKGROUND,    /* Background for blended content, effects, decorations */
     UNFL_CONTENT,       /* The actual DirectFB Window, i.e. its content */
     UNFL_FOREGROUND,    /* Decorations, effects, any other content overlay */

     _UNFL_NUM
} UniqueFrameLevel;

typedef struct {
     DirectLink                    link;

     DFBInputDeviceKeySymbol       symbol;
     DFBInputDeviceModifierMask    modifiers;

     UniqueWindow                 *owner;
} GrabbedKey;


struct __UniQuE_UniqueContext {
     FusionObject        object;

     int                 magic;

     CoreWindowStack    *stack;
     WMShared           *shared;

     CoreLayerRegion    *region;
     CoreSurface        *surface;

     DFBDisplayLayerID   layer_id;

     bool                active;

     DFBColor            color;

     int                           width;
     int                           height;

     StretRegion                  *root;

     FusionVector                  windows;


     UniqueDevice                 *devices[_UDCI_NUM];

     UniqueInputSwitch            *input_switch;


     DFBInputDeviceButtonMask      buttons;
     DFBInputDeviceModifierMask    modifiers;
     DFBInputDeviceLockState       locks;



     DFBPoint                      cursor;


     int                           wm_level;
     int                           wm_cycle;

     UniqueWindow                 *pointer_window;     /* window grabbing the pointer */
     UniqueWindow                 *keyboard_window;    /* window grabbing the keyboard */
     UniqueWindow                 *focused_window;     /* window having the focus */
     UniqueWindow                 *entered_window;     /* window under the pointer */

     DirectLink                   *grabbed_keys;       /* List of currently grabbed keys. */

     struct {
          DFBInputDeviceKeySymbol      symbol;
          DFBInputDeviceKeyIdentifier  id;
          int                          code;
          UniqueWindow                *owner;
     } keys[8];
};

struct __UniQuE_UniqueWindow {
     FusionObject             object;

     int                      magic;

     CoreWindow              *window;
     UniqueContext           *context;
     WMShared                *shared;

     CoreSurface             *surface;


     DFBWindowCapabilities    caps;

     UniqueWindowFlags        flags;

     StretRegion             *frame;
     StretRegion             *region;
     StretRegion             *foos[8];

     DFBInsets                insets;

     DFBRectangle             bounds;         /* absolute bounds of the content */
     DFBRectangle             full;           /* absolute bounds of the full frame */

     int                      opacity;        /* global alpha factor */

     DFBWindowStackingClass   stacking;       /* level boundaries */
     int                      priority;       /* derived from stacking class */

     DFBWindowOptions         options;        /* flags for appearance/behaviour */
     DFBWindowEventType       events;         /* mask of enabled events */

     __u32                    color_key;      /* transparent pixel */
     DFBRegion                opaque;         /* region of the window forced to be opaque */
};

struct __UniQuE_StretRegion {
     int                  magic;

     StretRegion         *parent;       /* Is NULL for the root region. */

     int                  level;        /* Level within the parent. */
     int                  index;        /* Index within the level. */

     int                  levels;       /* Number of levels provided. */
     FusionVector        *children;     /* Children of each level. */

     StretRegionFlags     flags;        /* Control appearance and activity. */

     DFBRegion            bounds;       /* Relative to its parent. */

     StretRegionClassID   clazz;        /* Region class (implementation) used for rendering etc. */

     void                *data;         /* Optional private data of region class. */
     unsigned long        arg;          /* Optional argument for region class instance. */
};

struct __UniQuE_UniqueDevice {
     DirectLink           link;

     int                  magic;

     UniqueContext       *context;

     UniqueDeviceClassID  clazz;        /* Device class (implementation) used for processing etc. */

     void                *data;         /* Optional private data of device class. */
     unsigned long        arg;          /* Optional argument for device class instance. */

     FusionReactor       *reactor;      /* UniqueInputEvent deployment */

     DirectLink          *connections;  /* CoreInputDevice connections */
};


typedef struct {
     DirectLink           link;

     int                  magic;

     UniqueInputChannel  *channel;

     UniqueInputEvent     filter;
} UniqueInputTargetFilter;

typedef struct {
     UniqueInputChannel  *normal;
     UniqueInputChannel  *fixed;

     DirectLink          *filters;
} UniqueInputTarget;

struct __UniQuE_UniqueInputSwitch {
     DirectLink           link;

     int                  magic;

     UniqueContext       *context;

     DirectLink          *connections;  /* UniqueDevice connections */

     UniqueInputTarget    targets[_UDCI_NUM];
};


DFBResult unique_wm_module_init  ( CoreDFB  *core,
                                   WMData   *data,
                                   WMShared *shared,
                                   bool      master );

void      unique_wm_module_deinit( bool      master,
                                   bool      emergency );

UniqueContext *unique_wm_create_context();
UniqueWindow  *unique_wm_create_window();


/* global reactions */
ReactionResult _unique_wm_module_context_listener( const void *msg_data,
                                                   void       *ctx );

ReactionResult _unique_wm_module_window_listener ( const void *msg_data,
                                                   void       *ctx );

ReactionResult _unique_device_listener ( const void *msg_data,
                                         void       *ctx );

ReactionResult _unique_input_switch_device_listener ( const void *msg_data,
                                                      void       *ctx );

#endif

