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
#include <core/layers.h>

#include <unique/types.h>
#include <unique/stret.h>


#define UNIQUE_WM_ABI_VERSION 2


extern const StretRegionClass unique_root_region_class;
extern const StretRegionClass unique_frame_region_class;
extern const StretRegionClass unique_window_region_class;
extern const StretRegionClass unique_foo_region_class;


typedef enum {
     UCI_ROOT,
     UCI_FRAME,
     UCI_WINDOW,
     UCI_FOO,

     _UCI_NUM
} UniqueClassIndex;


struct __UniQuE_WMData {
     int                           module_abi;

     CoreDFB                      *core;

     WMShared                     *shared;
};

struct __UniQuE_WMShared {
     FusionObjectPool             *context_pool;

     StretRegionClassID            classes[_UCI_NUM];
};

struct __UniQuE_StackData {
     int                           magic;

     CoreWindowStack              *stack;

     UniqueContext                *context;

     StretRegion                  *root;

     DFBInputDeviceButtonMask      buttons;
     DFBInputDeviceModifierMask    modifiers;
     DFBInputDeviceLockState       locks;

     int                           wm_level;
     int                           wm_cycle;

     FusionVector                  windows;

     CoreWindow                   *pointer_window;     /* window grabbing the pointer */
     CoreWindow                   *keyboard_window;    /* window grabbing the keyboard */
     CoreWindow                   *focused_window;     /* window having the focus */
     CoreWindow                   *entered_window;     /* window under the pointer */

     DirectLink                   *grabbed_keys;       /* List of currently grabbed keys. */

     struct {
          DFBInputDeviceKeySymbol      symbol;
          DFBInputDeviceKeyIdentifier  id;
          int                          code;
          CoreWindow                  *owner;
     } keys[8];
};

struct __UniQuE_WindowData {
     int                           magic;

     CoreWindow                   *window;

     StretRegion                  *frame;
     StretRegion                  *region;

     DFBInsets                     insets;
     bool                          has_frame;

     StackData                    *stack_data;

     int                           priority;           /* derived from stacking class */

     CoreLayerRegionConfig         config;
};

struct __UniQuE_UniqueContext {
     FusionObject    object;

     int             magic;

     StackData      *stack_data;

     DFBColor        color;
};


DFBResult unique_wm_module_init  ( CoreDFB  *core,
                                   WMData   *data,
                                   WMShared *shared,
                                   bool      master );

void      unique_wm_module_deinit( bool      master,
                                   bool      emergency );

UniqueContext *unique_wm_create_context();


#endif

