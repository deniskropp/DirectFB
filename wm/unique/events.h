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

#ifndef __UNIQUE__EVENTS_H__
#define __UNIQUE__EVENTS_H__

#include <directfb.h>

#include <unique/types.h>

typedef enum {
     UIET_NONE      = 0x00000000,

     UIET_MOTION    = 0x00000001,
     UIET_BUTTON    = 0x00000002,

     UIET_WHEEL     = 0x00000010,

     UIET_KEY       = 0x00000100,

     UIET_ALL       = 0x00000113
} UniqueInputEventType;


typedef struct {
     UniqueInputEventType               type;

     DFBInputDeviceID                   device_id;

     int                                x;
     int                                y;

     DFBInputDeviceButtonIdentifier     button;

     DFBInputDeviceButtonMask           buttons;
} UniqueInputPointerEvent;

typedef struct {
     UniqueInputEventType               type;

     DFBInputDeviceID                   device_id;

     int                                value;
} UniqueInputWheelEvent;

typedef struct {
     UniqueInputEventType               type;

     DFBInputDeviceID                   device_id;

     int                                key_code;
     DFBInputDeviceKeyIdentifier        key_id;
     DFBInputDeviceKeySymbol            key_symbol;

     DFBInputDeviceModifierMask         modifiers;
     DFBInputDeviceLockState            locks;
} UniqueInputKeyboardEvent;


typedef union {
     UniqueInputEventType               type;

     UniqueInputPointerEvent            pointer;
     UniqueInputWheelEvent              wheel;
     UniqueInputKeyboardEvent           keyboard;
} UniqueInputEvent;


#endif

