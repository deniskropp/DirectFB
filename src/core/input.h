/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <directfb.h>

#include <pthread.h>

/*
 * callback function for input device listener
 */
typedef void (*InputDeviceNotify)( DFBInputEvent *evt, void *ctx );

typedef struct _InputDeviceListener {
     InputDeviceNotify             notify;   /* called when an input event
                                                arrives */
     void                          *ctx;     /* passed to notify callback,
                                                for listener specific data */

     struct _InputDeviceListener   *next;
} InputDeviceListener;

typedef struct _InputDevice {

     /* these must be filled by the driver */
     struct {
          char                     name[20];      /* device name,
                                                     e.g. PS/2 Mouse */
          char                     vendor[40];    /* driver vendor name,
                                                     e.g. convergence */

          struct {
               int                 major;         /* major version */
               int                 minor;         /* minor version */
          } version;                              /* major.minor, e.g. 0.1 */
     } driver;

     DFBInputDeviceDescription     desc;          /* number of axis etc. */
     unsigned int                  id;            /* device id (TYPE/INDEX) */

     /*
      * Input thread routine, e.g. to do a blocking read() on a file
      * descriptor, generate input events and pass them to input_dispath().
      * Thread runs at realtime priority!
      */
     void*                         (*EventThread)(void *device);

     int                           number;             /* for drivers providing
                                                          multiple devices */


     /* these are private input core fields */
     pthread_mutex_t               listeners_mutex;    /* for synchronized
                                                          access to listeners */
     InputDeviceListener           *listeners;         /* list of input event
                                                          listeners */

     pthread_t                     event_thread;       /* EventThread handle */

     /*
      * Device driver (de)initialization function.
      */
     int                           (*init)(struct _InputDevice *device);
     void                          (*deinit)(struct _InputDevice *device);

     struct _InputDevice           *next;
} InputDevice;

extern InputDevice *inputdevices;


/*
 * core init function, probes all input drivers and creates devices of them
 */
DFBResult input_init_devices();

/*
 * adds a listener to the device's listeners
 */
DFBResult input_add_listener( InputDevice *device,
                               InputDeviceNotify notify, void *ctx );

/*
 * removes a listener from the device's listeners
 */
DFBResult input_remove_listener( InputDevice *device, void *ctx );

/*
 * called by input threads to post an event, notifies all listeners
 */
void input_dispatch( InputDevice *device, DFBInputEvent event );


DFBResult input_suspend();
DFBResult input_resume();

#endif
