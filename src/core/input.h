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
#include <pthread.h>

typedef struct _InputDevice InputDevice;


typedef struct {
     int  (*Probe)();
     int  (*Init)(InputDevice *device);
     void (*DeInit)(InputDevice *device);
} InputDriver;

struct _InputDevice {

     /* these must be filled by the driver */
     struct {
          char             *driver_name;          /* device name,
                                                     e.g. PS/2 Mouse */
          char             *driver_vendor;        /* driver vendor name,
                                                     e.g. convergence */

          struct {
               int          major;                /* major version */
               int          minor;                /* minor version */
          } driver_version;                       /* major.minor, e.g. 0.1 */

          InputDriver      *driver;
     } info;

     unsigned int                  id;            /* unique device id */
     DFBInputDeviceDescription     desc;          /* number of axis etc. */

     /*
      * Input thread routine, e.g. to do a blocking read() on a file
      * descriptor, generate input events and pass them to input_dispath().
      * Thread runs at realtime priority!
      */
     void*                         (*EventThread)(void *device);

     int                           number;             /* for drivers providing
                                                          multiple devices */

     Reactor                      *reactor;            /* event dispatcher */

     pthread_t                     event_thread;       /* EventThread handle */


     struct _InputDevice           *next;
};

extern InputDevice *inputdevices;


/*
 * core init function, probes all input drivers and creates devices of them
 */
DFBResult input_init_devices();

/*
 * cancels input threads, deinitializes drivers, deallocates device structs
 */
void input_deinit();

DFBResult input_suspend();
DFBResult input_resume();

#endif
