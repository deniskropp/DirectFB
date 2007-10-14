/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __IDIRECTFB_DISPATCHER_H__
#define __IDIRECTFB_DISPATCHER_H__

#include <directfb.h>

#define IDIRECTFB_METHOD_ID_AddRef                     1
#define IDIRECTFB_METHOD_ID_Release                    2
#define IDIRECTFB_METHOD_ID_SetCooperativeLevel        3
#define IDIRECTFB_METHOD_ID_GetDeviceDescription       4
#define IDIRECTFB_METHOD_ID_EnumVideoModes             5
#define IDIRECTFB_METHOD_ID_SetVideoMode               6
#define IDIRECTFB_METHOD_ID_CreateSurface              7
#define IDIRECTFB_METHOD_ID_CreatePalette              8
#define IDIRECTFB_METHOD_ID_EnumScreens                9
#define IDIRECTFB_METHOD_ID_GetScreen                 10
#define IDIRECTFB_METHOD_ID_EnumDisplayLayers         11
#define IDIRECTFB_METHOD_ID_GetDisplayLayer           12
#define IDIRECTFB_METHOD_ID_EnumInputDevices          13
#define IDIRECTFB_METHOD_ID_GetInputDevice            14
#define IDIRECTFB_METHOD_ID_CreateEventBuffer         15
#define IDIRECTFB_METHOD_ID_CreateInputEventBuffer    16
#define IDIRECTFB_METHOD_ID_CreateImageProvider       17
#define IDIRECTFB_METHOD_ID_CreateVideoProvider       18
#define IDIRECTFB_METHOD_ID_CreateFont                19
#define IDIRECTFB_METHOD_ID_CreateDataBuffer          20
#define IDIRECTFB_METHOD_ID_SetClipboardData          21
#define IDIRECTFB_METHOD_ID_GetClipboardData          22
#define IDIRECTFB_METHOD_ID_GetClipboardTimeStamp     23
#define IDIRECTFB_METHOD_ID_Suspend                   24
#define IDIRECTFB_METHOD_ID_Resume                    25
#define IDIRECTFB_METHOD_ID_WaitIdle                  26
#define IDIRECTFB_METHOD_ID_WaitForSync               27
#define IDIRECTFB_METHOD_ID_GetInterface              28

typedef struct {
     int  width;
     int  height;
     int  bpp;
} IDirectFB_Dispatcher_EnumVideoModes_Item;

typedef struct {
     DFBScreenID          screen_id;
     DFBScreenDescription desc;
} IDirectFB_Dispatcher_EnumScreens_Item;

typedef struct {
     DFBInputDeviceID          device_id;
     DFBInputDeviceDescription desc;
} IDirectFB_Dispatcher_EnumInputDevices_Item;

#endif
