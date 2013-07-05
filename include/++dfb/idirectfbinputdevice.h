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


#ifndef IDIRECTFBINPUTDEVICE_H
#define IDIRECTFBINPUTDEVICE_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBInputDevice : public IPPAny<IDirectFBInputDevice, IDirectFBInputDevice_C>{
friend
     class IDirectFB;

public:
     PPDFB_API IDirectFBInputDevice(IDirectFBInputDevice_C* myptr=NULL):IPPAny<IDirectFBInputDevice, IDirectFBInputDevice_C>(myptr){}

     DFBInputDeviceID           PPDFB_API GetID             ();
     void                       PPDFB_API GetDescription    (DFBInputDeviceDescription      *desc);

     void                       PPDFB_API GetKeymapEntry    (int                             code,
                                                             DFBInputDeviceKeymapEntry      *entry);

     IDirectFBEventBuffer       PPDFB_API CreateEventBuffer ();
     void                       PPDFB_API AttachEventBuffer (IDirectFBEventBuffer           *buffer);
     void                       PPDFB_API DetachEventBuffer (IDirectFBEventBuffer           *buffer);

     DFBInputDeviceKeyState     PPDFB_API GetKeyState       (DFBInputDeviceKeyIdentifier     key_id);
     DFBInputDeviceModifierMask PPDFB_API GetModifiers      ();
     DFBInputDeviceLockState    PPDFB_API GetLockState      ();
     DFBInputDeviceButtonMask   PPDFB_API GetButtons        ();
     DFBInputDeviceButtonState  PPDFB_API GetButtonState    (DFBInputDeviceButtonIdentifier  button);
     int                        PPDFB_API GetAxis           (DFBInputDeviceAxisIdentifier    axis);

     void                       PPDFB_API GetXY             (int                            *x,
                                                             int                            *y);


     inline IDirectFBInputDevice PPDFB_API & operator = (const IDirectFBInputDevice& other){
          return IPPAny<IDirectFBInputDevice, IDirectFBInputDevice_C>::operator =(other);
     }
     inline IDirectFBInputDevice PPDFB_API & operator = (IDirectFBInputDevice_C* other){
          return IPPAny<IDirectFBInputDevice, IDirectFBInputDevice_C>::operator =(other);
     }

};

#endif
