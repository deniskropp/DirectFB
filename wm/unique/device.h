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

#ifndef __UNIQUE__DEVICE_H__
#define __UNIQUE__DEVICE_H__

#include <directfb.h>

#include <fusion/reactor.h>

#include <core/coretypes.h>

#include <unique/types.h>


typedef struct {
     int data_size;


     DFBResult (*Initialize)    ( UniqueDevice        *device,
                                  void                *data,
                                  void                *ctx );

     void      (*Shutdown)      ( UniqueDevice        *device,
                                  void                *data,
                                  void                *ctx );


     void (*Connected)   ( UniqueDevice           *device,
                           void                   *data,
                           void                   *ctx,
                           CoreInputDevice        *source );

     void (*Disconnected)( UniqueDevice           *device,
                           void                   *data,
                           void                   *ctx,
                           CoreInputDevice        *source );

     void (*ProcessEvent)( UniqueDevice           *device,
                           void                   *data,
                           void                   *ctx,
                           const DFBInputEvent    *event );


     bool (*FilterEvent) ( const UniqueInputEvent *event,
                           const UniqueInputEvent *filter );
} UniqueDeviceClass;

typedef unsigned int UniqueDeviceID;
typedef unsigned int UniqueDeviceClassID;

typedef enum {
     UDCI_POINTER,
     UDCI_WHEEL,
     UDCI_KEYBOARD,

     _UDCI_NUM
} UniqueDeviceClassIndex;


DFBResult unique_device_class_register  ( const UniqueDeviceClass *clazz,
                                          UniqueDeviceClassID     *ret_id );

DFBResult unique_device_class_unregister( UniqueDeviceClassID      id );


DFBResult unique_device_create       ( CoreDFB                *core,
                                       UniqueContext          *context,
                                       UniqueDeviceClassID     class_id,
                                       void                   *ctx,
                                       UniqueDevice          **ret_device );

DFBResult unique_device_destroy      ( UniqueDevice           *device );


DFBResult unique_device_connect      ( UniqueDevice           *device,
                                       CoreInputDevice        *source );

DFBResult unique_device_disconnect   ( UniqueDevice           *device,
                                       CoreInputDevice        *source );

DFBResult unique_device_attach       ( UniqueDevice           *device,
                                       ReactionFunc            func,
                                       void                   *ctx,
                                       Reaction               *reaction );

DFBResult unique_device_detach       ( UniqueDevice           *device,
                                       Reaction               *reaction );

DFBResult unique_device_attach_global( UniqueDevice           *device,
                                       int                     index,
                                       void                   *ctx,
                                       GlobalReaction         *reaction );

DFBResult unique_device_detach_global( UniqueDevice           *device,
                                       GlobalReaction         *reaction );

DFBResult unique_device_dispatch     ( UniqueDevice           *device,
                                       const UniqueInputEvent *event );

bool      unique_device_filter       ( UniqueDeviceClassID     class_id,
                                       const UniqueInputEvent *event,
                                       const UniqueInputEvent *filter );


/* global reactions */

typedef enum {
     UNIQUE_INPUT_SWITCH_DEVICE_LISTENER,
     UNIQUE_CURSOR_DEVICE_LISTENER
} UNIQUE_DEVICE_GLOBALS;

#endif

