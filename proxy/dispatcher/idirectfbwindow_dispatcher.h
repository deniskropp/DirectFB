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

#ifndef __IDIRECTFBWINDOW_DISPATCHER_H__
#define __IDIRECTFBWINDOW_DISPATCHER_H__

#define IDIRECTFBWINDOW_METHOD_ID_AddRef                     1
#define IDIRECTFBWINDOW_METHOD_ID_Release                    2
#define IDIRECTFBWINDOW_METHOD_ID_CreateEventBuffer          3
#define IDIRECTFBWINDOW_METHOD_ID_AttachEventBuffer          4
#define IDIRECTFBWINDOW_METHOD_ID_EnableEvents               5
#define IDIRECTFBWINDOW_METHOD_ID_DisableEvents              6
#define IDIRECTFBWINDOW_METHOD_ID_GetID                      7
#define IDIRECTFBWINDOW_METHOD_ID_GetPosition                8
#define IDIRECTFBWINDOW_METHOD_ID_GetSize                    9
#define IDIRECTFBWINDOW_METHOD_ID_GetSurface                10
#define IDIRECTFBWINDOW_METHOD_ID_SetOptions                11
#define IDIRECTFBWINDOW_METHOD_ID_GetOptions                12
#define IDIRECTFBWINDOW_METHOD_ID_SetColorKey               13
#define IDIRECTFBWINDOW_METHOD_ID_SetColorKeyIndex          14
#define IDIRECTFBWINDOW_METHOD_ID_SetOpaqueRegion           15
#define IDIRECTFBWINDOW_METHOD_ID_SetOpacity                16
#define IDIRECTFBWINDOW_METHOD_ID_GetOpacity                17
#define IDIRECTFBWINDOW_METHOD_ID_SetCursorShape            18
#define IDIRECTFBWINDOW_METHOD_ID_RequestFocus              19
#define IDIRECTFBWINDOW_METHOD_ID_GrabKeyboard              20
#define IDIRECTFBWINDOW_METHOD_ID_UngrabKeyboard            21
#define IDIRECTFBWINDOW_METHOD_ID_GrabPointer               22
#define IDIRECTFBWINDOW_METHOD_ID_UngrabPointer             23
#define IDIRECTFBWINDOW_METHOD_ID_GrabKey                   24
#define IDIRECTFBWINDOW_METHOD_ID_UngrabKey                 25
#define IDIRECTFBWINDOW_METHOD_ID_Move                      26
#define IDIRECTFBWINDOW_METHOD_ID_MoveTo                    27
#define IDIRECTFBWINDOW_METHOD_ID_Resize                    28
#define IDIRECTFBWINDOW_METHOD_ID_SetStackingClass          29
#define IDIRECTFBWINDOW_METHOD_ID_Raise                     30
#define IDIRECTFBWINDOW_METHOD_ID_Lower                     31
#define IDIRECTFBWINDOW_METHOD_ID_RaiseToTop                32
#define IDIRECTFBWINDOW_METHOD_ID_LowerToBottom             33
#define IDIRECTFBWINDOW_METHOD_ID_PutAtop                   34
#define IDIRECTFBWINDOW_METHOD_ID_PutBelow                  35
#define IDIRECTFBWINDOW_METHOD_ID_Close                     36
#define IDIRECTFBWINDOW_METHOD_ID_Destroy                   37

/*
 * private data struct of IDirectFBWindow_Dispatcher
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBWindow     *real;

     VoodooInstanceID     self;
     VoodooInstanceID     super;
} IDirectFBWindow_Dispatcher_data;

#endif
