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

#ifndef __IDIRECTFBDISPLAYLAYER_DISPATCHER_H__
#define __IDIRECTFBDISPLAYLAYER_DISPATCHER_H__

#define IDIRECTFBDISPLAYLAYER_METHOD_ID_AddRef                     1
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_Release                    2
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetID                      3
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetDescription             4
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetSurface                 5
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetScreen                  6
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCooperativeLevel        7
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetOpacity                 8
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetCurrentOutputField      9
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetScreenLocation         10
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetSrcColorKey            11
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetDstColorKey            12
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetLevel                  13
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetLevel                  14
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetConfiguration          15
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_TestConfiguration         16
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetConfiguration          17
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundMode         18
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundColor        19
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundImage        20
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetColorAdjustment        21
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetColorAdjustment        22
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_CreateWindow              23
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetWindow                 24
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_WarpCursor                25
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCursorAcceleration     26
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_EnableCursor              27
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_GetCursorPosition         28
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCursorShape            29
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCursorOpacity          30
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_SetFieldParity            31
#define IDIRECTFBDISPLAYLAYER_METHOD_ID_WaitForSync               32

/*
 * private data struct of IDirectFBDisplayLayer_Dispatcher
 */
typedef struct {
     int                    ref;      /* reference counter */

     IDirectFBDisplayLayer *real;

     VoodooInstanceID       self;
     VoodooInstanceID       super;
} IDirectFBDisplayLayer_Dispatcher_data;

#endif
