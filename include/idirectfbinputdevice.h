/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>

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

#ifndef IDIRECTFBINPUTDEVICE_H
#define IDIRECTFBINPUTDEVICE_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBInputDevice {
friend
     class IDirectFB;

public:
     DFBInputDeviceID           GetID             ();
     void                       GetDescription    (DFBInputDeviceDescription      *desc);

     void                       GetKeymapEntry    (int                             code,
                                                   DFBInputDeviceKeymapEntry      *entry);

     IDirectFBEventBuffer       CreateEventBuffer ();
     void                       AttachEventBuffer (IDirectFBEventBuffer           *buffer);

     DFBInputDeviceKeyState     GetKeyState       (DFBInputDeviceKeyIdentifier     key_id);
     DFBInputDeviceModifierMask GetModifiers      ();
     DFBInputDeviceLockState    GetLockState      ();
     DFBInputDeviceButtonMask   GetButtons        ();
     DFBInputDeviceButtonState  GetButtonState    (DFBInputDeviceButtonIdentifier  button);
     int                        GetAxis           (DFBInputDeviceAxisIdentifier    axis);

     void                       GetXY             (int                            *x,
                                                   int                            *y);


     __DFB_PLUS_PLUS__INTERFACE_CLASS( IDirectFBInputDevice );
};

#endif
