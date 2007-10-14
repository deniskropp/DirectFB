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

#ifndef __IDIRECTFBSCREEN_DISPATCHER_H__
#define __IDIRECTFBSCREEN_DISPATCHER_H__

#define IDIRECTFBSCREEN_METHOD_ID_AddRef                     1
#define IDIRECTFBSCREEN_METHOD_ID_Release                    2
#define IDIRECTFBSCREEN_METHOD_ID_GetID                      3
#define IDIRECTFBSCREEN_METHOD_ID_GetDescription             4
#define IDIRECTFBSCREEN_METHOD_ID_GetSize                    5
#define IDIRECTFBSCREEN_METHOD_ID_EnumDisplayLayers          6
#define IDIRECTFBSCREEN_METHOD_ID_SetPowerMode               7
#define IDIRECTFBSCREEN_METHOD_ID_WaitForSync                8
#define IDIRECTFBSCREEN_METHOD_ID_GetMixerDescriptions       9
#define IDIRECTFBSCREEN_METHOD_ID_GetMixerConfiguration     10
#define IDIRECTFBSCREEN_METHOD_ID_TestMixerConfiguration    11
#define IDIRECTFBSCREEN_METHOD_ID_SetMixerConfiguration     12
#define IDIRECTFBSCREEN_METHOD_ID_GetEncoderDescriptions    13
#define IDIRECTFBSCREEN_METHOD_ID_GetEncoderConfiguration   14
#define IDIRECTFBSCREEN_METHOD_ID_TestEncoderConfiguration  15
#define IDIRECTFBSCREEN_METHOD_ID_SetEncoderConfiguration   16
#define IDIRECTFBSCREEN_METHOD_ID_GetOutputDescriptions     17
#define IDIRECTFBSCREEN_METHOD_ID_GetOutputConfiguration    18
#define IDIRECTFBSCREEN_METHOD_ID_TestOutputConfiguration   19
#define IDIRECTFBSCREEN_METHOD_ID_SetOutputConfiguration    20

typedef struct {
     DFBDisplayLayerID          layer_id;
     DFBDisplayLayerDescription desc;
} IDirectFBScreen_Dispatcher_EnumDisplayLayers_Item;

#endif
