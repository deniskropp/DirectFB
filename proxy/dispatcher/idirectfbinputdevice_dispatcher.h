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

#ifndef __IDIRECTFBINPUTDEVICE_DISPATCHER_H__
#define __IDIRECTFBINPUTDEVICE_DISPATCHER_H__

#define IDIRECTFBINPUTDEVICE_METHOD_ID_AddRef                     1
#define IDIRECTFBINPUTDEVICE_METHOD_ID_Release                    2
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetID                      3
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetDescription             4
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetKeymapEntry             5
#define IDIRECTFBINPUTDEVICE_METHOD_ID_CreateEventBuffer          6
#define IDIRECTFBINPUTDEVICE_METHOD_ID_AttachEventBuffer          7
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetKeyState                8
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetModifiers               9
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetLockState              10
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetButtons                11
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetButtonState            12
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetAxis                   13
#define IDIRECTFBINPUTDEVICE_METHOD_ID_GetXY                     14

/*
 * private data struct of IDirectFBInputDevice_Dispatcher
 */
typedef struct {
     int                   ref;      /* reference counter */

     IDirectFBInputDevice *real;

     VoodooInstanceID      self;
     VoodooInstanceID      super;
} IDirectFBInputDevice_Dispatcher_data;

#endif
