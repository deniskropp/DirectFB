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

#ifndef __INPUT_H__
#define __INPUT_H__

#include <pthread.h>
#include <directfb.h>

#include <direct/modules.h>

#include <fusion/reactor.h>

#include <core/coretypes.h>


DECLARE_MODULE_DIRECTORY( dfb_input_modules );


/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_INPUT_DRIVER_ABI_VERSION         6

#define DFB_INPUT_DRIVER_INFO_NAME_LENGTH   60
#define DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH 80


typedef struct {
     int          major;              /* major version */
     int          minor;              /* minor version */
} InputDriverVersion;                 /* major.minor, e.g. 0.1 */

typedef struct {
     InputDriverVersion version;

     char               name[DFB_INPUT_DRIVER_INFO_NAME_LENGTH];
                                      /* Name of driver,
                                         e.g. 'Serial Mouse Driver' */

     char               vendor[DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH];
                                      /* Vendor (or author) of the driver,
                                         e.g. 'convergence' or 'Sven Neumann' */
} InputDriverInfo;

typedef struct {
     unsigned int       prefered_id;  /* Prefered predefined input device id,
                                         e.g. DIDID_MOUSE */

     DFBInputDeviceDescription desc;  /* Capabilities, type, etc. */
} InputDeviceInfo;

typedef struct {
     int       (*GetAvailable)   ();
     void      (*GetDriverInfo)  (InputDriverInfo            *driver_info);
     DFBResult (*OpenDevice)     (InputDevice                *device,
                                  unsigned int                number,
                                  InputDeviceInfo            *device_info,
                                  void                      **driver_data);
     DFBResult (*GetKeymapEntry) (InputDevice                *device,
                                  void                       *driver_data,
                                  DFBInputDeviceKeymapEntry  *entry);
     void      (*CloseDevice)    (void                       *driver_data);
} InputDriverFuncs;


typedef DFBEnumerationResult (*InputDeviceCallback) (InputDevice *device,
                                                     void        *ctx);

void dfb_input_enumerate_devices( InputDeviceCallback  callback,
                                  void                *ctx );

FusionResult dfb_input_attach( InputDevice *device,
                               React        react,
                               void        *ctx,
                               Reaction    *reaction );

FusionResult dfb_input_detach( InputDevice *device,
                               Reaction    *reaction );

FusionResult dfb_input_attach_global( InputDevice    *device,
                                      int             react_index,
                                      void           *ctx,
                                      GlobalReaction *reaction );

FusionResult dfb_input_detach_global( InputDevice    *device,
                                      GlobalReaction *reaction );

void dfb_input_dispatch( InputDevice *device, DFBInputEvent *event );

void dfb_input_device_description( const InputDevice         *device,
                                   DFBInputDeviceDescription *desc );

DFBInputDeviceID  dfb_input_device_id( const InputDevice *device );
InputDevice      *dfb_input_device_at( DFBInputDeviceID   id );

DFBResult dfb_input_device_get_keymap_entry( InputDevice               *device,
                                             int                        keycode,
                                             DFBInputDeviceKeymapEntry *entry );

/* global reactions */

typedef enum {
     DFB_WINDOWSTACK_INPUTDEVICE_LISTENER
} DFB_INPUT_GLOBALS;

#endif
