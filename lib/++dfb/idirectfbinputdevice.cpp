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


#include "++dfb.h"
#include "++dfb_internal.h"

DFBInputDeviceID IDirectFBInputDevice::GetID()
{
     DFBInputDeviceID device_id;

     DFBCHECK( iface->GetID (iface, &device_id) );

     return device_id;
}

void IDirectFBInputDevice::GetDescription (DFBInputDeviceDescription *desc)
{
     DFBCHECK( iface->GetDescription (iface, desc) );
}

void IDirectFBInputDevice::GetKeymapEntry (int                        code,
                                           DFBInputDeviceKeymapEntry *entry)
{
     DFBCHECK( iface->GetKeymapEntry (iface, code, entry) );
}

IDirectFBEventBuffer IDirectFBInputDevice::CreateEventBuffer()
{
     IDirectFBEventBuffer_C *idirectfbeventbuffer;

     DFBCHECK( iface->CreateEventBuffer (iface, &idirectfbeventbuffer) );

     return IDirectFBEventBuffer (idirectfbeventbuffer);
}

void IDirectFBInputDevice::AttachEventBuffer (IDirectFBEventBuffer *buffer)
{
     DFBCHECK( iface->AttachEventBuffer (iface, buffer->get_iface()) );
}

void IDirectFBInputDevice::DetachEventBuffer (IDirectFBEventBuffer *buffer)
{
     DFBCHECK( iface->DetachEventBuffer (iface, buffer->get_iface()) );
}

DFBInputDeviceKeyState IDirectFBInputDevice::GetKeyState (DFBInputDeviceKeyIdentifier key_id)
{
     DFBInputDeviceKeyState state;

     DFBCHECK( iface->GetKeyState (iface, key_id, &state) );

     return state;
}

DFBInputDeviceModifierMask IDirectFBInputDevice::GetModifiers()
{
     DFBInputDeviceModifierMask modifiers;

     DFBCHECK( iface->GetModifiers (iface, &modifiers) );

     return modifiers;
}

DFBInputDeviceLockState IDirectFBInputDevice::GetLockState()
{
     DFBInputDeviceLockState state;

     DFBCHECK( iface->GetLockState (iface, &state) );

     return state;
}

DFBInputDeviceButtonMask IDirectFBInputDevice::GetButtons()
{
     DFBInputDeviceButtonMask mask;

     DFBCHECK( iface->GetButtons (iface, &mask) );

     return mask;
}

DFBInputDeviceButtonState IDirectFBInputDevice::GetButtonState (DFBInputDeviceButtonIdentifier button)
{
     DFBInputDeviceButtonState state;

     DFBCHECK( iface->GetButtonState (iface, button, &state) );

     return state;
}

int IDirectFBInputDevice::GetAxis (DFBInputDeviceAxisIdentifier axis)
{
     int value;

     DFBCHECK( iface->GetAxis (iface, axis, &value) );

     return value;
}

void IDirectFBInputDevice::GetXY (int *x, int *y)
{
     DFBCHECK( iface->GetXY (iface, x, y) );
}

