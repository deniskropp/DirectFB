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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/input.h>

#include "idirectfbinputdevice.h"
#include "idirectfbinputbuffer.h"

#include <directfb_internals.h>


/* 
 * processes an event, updates device state 
 * (funcion is added to the event listeners)
 */  
void IDirectFBInputDevice_Receive( DFBInputEvent *evt, void *ctx );

/*
 * private data struct of IDirectFBInputDevice
 */
typedef struct {
     int                           ref;            /* refetence counter */
     InputDevice                   *device;        /* pointer to input core
                                                      device struct*/                                                	     

     int                           axis[8];        /* position of all axes */
     DFBInputDeviceKeyState        keystates[256]; /* state of all keys */
     DFBInputDeviceModifierKeys    modifiers;      /* bitmask reflecting the
                                                      state of the modifier
                                                      keys */
     DFBInputDeviceButtonMask      buttonmask;     /* bitmask reflecting the
                                                      state of the buttons */
                                                      
} IDirectFBInputDevice_data;



void IDirectFBInputDevice_Destruct( IDirectFBInputDevice *thiz )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     input_remove_listener( data->device, data );

     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBInputDevice_AddRef( IDirectFBInputDevice *thiz )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_Release( IDirectFBInputDevice *thiz )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBInputDevice_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_CreateInputBuffer( IDirectFBInputDevice *thiz,
                                                  IDirectFBInputBuffer **buffer)
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     DFB_ALLOCATE_INTERFACE( *buffer, IDirectFBInputBuffer );

     return IDirectFBInputBuffer_Construct( *buffer, data->device );
}

DFBResult IDirectFBInputDevice_GetDescription( IDirectFBInputDevice *thiz,
                                               DFBInputDeviceDescription *desc )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!desc)
          return DFB_INVARG;

     *desc = data->device->desc;

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetKeyState( IDirectFBInputDevice *thiz, 
                                            DFBInputDeviceKeyIdentifier keycode,
                                            DFBInputDeviceKeyState *state )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!state)
          return DFB_INVARG;

     *state = data->keystates[keycode];

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetModifiers( IDirectFBInputDevice *thiz, 
                                             DFBInputDeviceModifierKeys *modifiers )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!modifiers)
          return DFB_INVARG;

     *modifiers = data->modifiers;

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetButtons( IDirectFBInputDevice *thiz,
                                           DFBInputDeviceButtonMask *buttons )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;


     if (!data)
          return DFB_DEAD;

     if (!buttons)
          return DFB_INVARG;

     *buttons = data->buttonmask;

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetButtonState( IDirectFBInputDevice *thiz,
                                     DFBInputDeviceButtonIdentifier  button,
                                     DFBInputDeviceButtonState      *state)
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;


     if (!data)
          return DFB_DEAD;

     if (!state || button < 0 || button > 7)
          return DFB_INVARG;

     *state = (data->buttonmask & (1 << button)) ? DIBS_DOWN : DIBS_UP;

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetAxis( IDirectFBInputDevice *thiz, 
                                        DFBInputDeviceAxisIdentifier axis,
                                        int *pos )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!pos)
          return DFB_INVARG;

     *pos = data->axis[axis];

     return DFB_OK;
}

DFBResult IDirectFBInputDevice_GetXY( IDirectFBInputDevice *thiz, 
                                      int *x, int *y )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;
          
     if (!x && !y)
          return DFB_INVARG;
     
     if (!x)     
          *x = data->axis[DIAI_X];
          
     if (!y)
          *y = data->axis[DIAI_Y]; 
     
     return DFB_OK;
}

DFBResult IDirectFBInputDevice_Construct( IDirectFBInputDevice *thiz,
                                          InputDevice *device )
{
     IDirectFBInputDevice_data *data;

     data = (IDirectFBInputDevice_data*)malloc( sizeof(
                                                IDirectFBInputDevice_data) );
                                                
     memset( data, 0, sizeof(IDirectFBInputDevice_data) );
     thiz->priv = data;

     data->ref = 1;
     data->device = device;

     input_add_listener( device, IDirectFBInputDevice_Receive, data );

     thiz->AddRef = IDirectFBInputDevice_AddRef;
     thiz->Release = IDirectFBInputDevice_Release;
     thiz->CreateInputBuffer = IDirectFBInputDevice_CreateInputBuffer;
     thiz->GetDescription = IDirectFBInputDevice_GetDescription;
     thiz->GetKeyState = IDirectFBInputDevice_GetKeyState;
     thiz->GetModifiers = IDirectFBInputDevice_GetModifiers;
     thiz->GetButtons = IDirectFBInputDevice_GetButtons;
     thiz->GetButtonState = IDirectFBInputDevice_GetButtonState;
     thiz->GetAxis = IDirectFBInputDevice_GetAxis;
     thiz->GetXY = IDirectFBInputDevice_GetXY;

     return DFB_OK;
}

void IDirectFBInputDevice_Receive( DFBInputEvent *evt, void *ctx )
{
     IDirectFBInputDevice_data *data = (IDirectFBInputDevice_data*)ctx;

     switch (evt->type) {
          case DIET_KEYPRESS:
               if (evt->flags & DIEF_KEYCODE)
                    data->keystates[evt->keycode] = DIKS_DOWN;
               if (evt->flags & DIEF_MODIFIERS)
                    data->modifiers = evt->modifiers;
               break;

          case DIET_KEYRELEASE:
               if (evt->flags & DIEF_KEYCODE)
                    data->keystates[evt->keycode] = DIKS_UP;
               if (evt->flags & DIEF_MODIFIERS)
                    data->modifiers = evt->modifiers;
               break;

          case DIET_BUTTONPRESS:
               data->buttonmask |= (1 << evt->button);
               break;

          case DIET_BUTTONRELEASE:
               data->buttonmask &= ~(1 << evt->button );
               break;

          case DIET_AXISMOTION:
               if (evt->flags & DIEF_AXISREL)
                    data->axis[evt->axis] += evt->axisrel;
               if (evt->flags & DIEF_AXISABS)
                    data->axis[evt->axis] = evt->axisabs;
               break;

          default:
               DEBUGMSG( "DirectFB/IDirectFBInputDevice: Unknown event type detected (0x%x), skipping!\n", evt->type );
     }
}

