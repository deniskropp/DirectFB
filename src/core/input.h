/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <core/fusion/reactor.h>

#include <core/coretypes.h>


/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_INPUT_DRIVER_ABI_VERSION         4

#define DFB_INPUT_DRIVER_INFO_NAME_LENGTH   60
#define DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH 80

#define DFB_INPUT_DEVICE_INFO_NAME_LENGTH   60
#define DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH 80


typedef struct {
     int          major;              /* major version */
     int          minor;              /* minor version */
} InputDriverVersion;                 /* major.minor, e.g. 0.1 */

typedef struct {
     InputDriverVersion version;

     char               name[DFB_INPUT_DRIVER_INFO_NAME_LENGTH+1];
                                      /* Name of driver,
                                         e.g. 'Serial Mouse Driver' */

     char               vendor[DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH+1];
                                      /* Vendor (or author) of the driver,
                                         e.g. 'convergence' or 'Sven Neumann' */
} InputDriverInfo;

typedef struct {
     char               name[DFB_INPUT_DEVICE_INFO_NAME_LENGTH+1];
                                      /* Device name,
                                         e.g. 'MouseMan Serial Mouse' */

     char               vendor[DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH+1];
                                      /* Vendor of the device,
                                         e.g. 'Logitech' or just 'Unknown' */

     unsigned int       prefered_id;  /* Prefered predefined input device id,
                                         e.g. DIDID_MOUSE */

     DFBInputDeviceDescription desc;  /* Capabilities, type, etc. */
} InputDeviceInfo;

typedef struct {
     int       (*GetAbiVersion)  ();
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

/*
 * core init function, probes all input drivers and creates devices of them
 */
DFBResult dfb_input_initialize();
DFBResult dfb_input_join();

/*
 * cancels input threads, deinitializes drivers, deallocates device structs
 */
DFBResult dfb_input_shutdown( bool emergency );
DFBResult dfb_input_leave( bool emergency );

#ifdef FUSION_FAKE
DFBResult dfb_input_suspend();
DFBResult dfb_input_resume();
#endif

void dfb_input_register_module( InputDriverFuncs *funcs );

typedef DFBEnumerationResult (*InputDeviceCallback) (InputDevice *device,
                                                     void        *ctx);

void dfb_input_enumerate_devices( InputDeviceCallback  callback,
                                  void                *ctx );

void dfb_input_attach( InputDevice *device, React react, void *ctx );
void dfb_input_detach( InputDevice *device, React react, void *ctx );
void dfb_input_dispatch( InputDevice *device, DFBInputEvent *event );

void dfb_input_device_description( const InputDevice         *device,
                                   DFBInputDeviceDescription *desc );

DFBInputDeviceID dfb_input_device_id( const InputDevice *device );

DFBResult dfb_input_device_get_keymap_entry( InputDevice               *device,
                                             int                        keycode,
                                             DFBInputDeviceKeymapEntry *entry );

#endif
