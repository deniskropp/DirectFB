/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#ifndef __CORE__INPUT_HUB_H__
#define __CORE__INPUT_HUB_H__

#include <directfb.h>

typedef struct __CoreDFB__CoreInputHub       CoreInputHub;
typedef struct __CoreDFB__CoreInputHubClient CoreInputHubClient;


DFBResult CoreInputHub_Create       ( u32                                 queue_id,
                                      CoreInputHub                      **ret_hub );

DFBResult CoreInputHub_Destroy      ( CoreInputHub                       *hub );

DFBResult CoreInputHub_AddDevice    ( CoreInputHub                       *hub,
                                      DFBInputDeviceID                    device_id,
                                      const DFBInputDeviceDescription    *desc );

DFBResult CoreInputHub_RemoveDevice ( CoreInputHub                       *hub,
                                      DFBInputDeviceID                    device_id );

DFBResult CoreInputHub_DispatchEvent( CoreInputHub                       *hub,
                                      DFBInputDeviceID                    device_id,
                                      const DFBInputEvent                *event );



typedef struct {
     void (*DeviceAdd)    ( void                               *ctx,
                            DFBInputDeviceID                    device_id,
                            const DFBInputDeviceDescription    *desc );

     void (*DeviceRemove) ( void                               *ctx,
                            DFBInputDeviceID                    device_id );


     void (*EventDispatch)( void                               *ctx,
                            DFBInputDeviceID                    device_id,
                            const DFBInputEvent                *event );
} CoreInputHubClientCallbacks;

DFBResult CoreInputHubClient_Create  ( u32                                 remote_qid,
                                       const CoreInputHubClientCallbacks  *callbacks,
                                       void                               *ctx,
                                       CoreInputHubClient                **ret_client );

DFBResult CoreInputHubClient_Destroy ( CoreInputHubClient                 *client );

DFBResult CoreInputHubClient_Activate( CoreInputHubClient                 *client );


#endif
