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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/layers.h>
#include <core/screen.h>

#include "idirectfbscreen.h"

/*
 * private data struct of IDirectFBScreen
 */
typedef struct {
     int                              ref;     /* reference counter */

     CoreScreen                      *screen;

     DFBScreenID                      id;
     DFBScreenDescription             description;
} IDirectFBScreen_data;

/******************************************************************************/

typedef struct {
     CoreScreen              *screen;

     DFBDisplayLayerCallback  callback;
     void                    *callback_ctx;
} EnumDisplayLayers_Context;

static DFBEnumerationResult EnumDisplayLayers_Callback( CoreLayer   *layer,
                                                        void        *ctx );

/******************************************************************************/

static void
IDirectFBScreen_Destruct( IDirectFBScreen *thiz )
{
//     IDirectFBScreen_data *data = (IDirectFBScreen_data*)thiz->priv;

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBScreen_AddRef( IDirectFBScreen *thiz )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_Release( IDirectFBScreen *thiz )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     if (--data->ref == 0)
          IDirectFBScreen_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetID( IDirectFBScreen *thiz,
                       DFBScreenID     *id )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     if (!id)
          return DFB_INVARG;

     *id = data->id;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetDescription( IDirectFBScreen      *thiz,
                                DFBScreenDescription *desc )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     if (!desc)
          return DFB_INVARG;

     *desc = data->description;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_EnumDisplayLayers( IDirectFBScreen         *thiz,
                                   DFBDisplayLayerCallback  callbackfunc,
                                   void                    *callbackdata )
{
     EnumDisplayLayers_Context context;

     INTERFACE_GET_DATA(IDirectFBScreen)

     if (!callbackfunc)
          return DFB_INVARG;

     context.screen       = data->screen;
     context.callback     = callbackfunc;
     context.callback_ctx = callbackdata;

     dfb_layers_enumerate( EnumDisplayLayers_Callback, &context );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_SetPowerMode( IDirectFBScreen    *thiz,
                              DFBScreenPowerMode  mode )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     switch (mode) {
          case DSPM_ON:
          case DSPM_STANDBY:
          case DSPM_SUSPEND:
          case DSPM_OFF:
               break;

          default:
               return DFB_INVARG;
     }

     return dfb_screen_set_powermode( data->screen, mode );
}

static DFBResult
IDirectFBScreen_WaitForSync( IDirectFBScreen *thiz )
{
     INTERFACE_GET_DATA(IDirectFBScreen)

     return dfb_screen_wait_vsync( data->screen );
}

/******************************************************************************/

DFBResult
IDirectFBScreen_Construct( IDirectFBScreen *thiz,
                           CoreScreen      *screen )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBScreen)

     data->ref    = 1;
     data->screen = screen;

     dfb_screen_get_info( screen, &data->id, &data->description );

     thiz->AddRef             = IDirectFBScreen_AddRef;
     thiz->Release            = IDirectFBScreen_Release;
     thiz->GetID              = IDirectFBScreen_GetID;
     thiz->GetDescription     = IDirectFBScreen_GetDescription;
     thiz->EnumDisplayLayers  = IDirectFBScreen_EnumDisplayLayers;
     thiz->SetPowerMode       = IDirectFBScreen_SetPowerMode;
     thiz->WaitForSync        = IDirectFBScreen_WaitForSync;

     return DFB_OK;
}

/******************************************************************************/

static DFBEnumerationResult
EnumDisplayLayers_Callback( CoreLayer *layer, void *ctx )
{
     DFBDisplayLayerDescription  desc;
     EnumDisplayLayers_Context  *context = (EnumDisplayLayers_Context*) ctx;

     if (dfb_layer_screen( layer ) != context->screen)
          return DFENUM_OK;

     dfb_layer_get_description( layer, &desc );

     return context->callback( dfb_layer_id_translated( layer ), desc,
                               context->callback_ctx );
}

