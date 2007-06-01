/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <unistd.h>

#include <fusiondale.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <core/dale_core.h>
#include <core/messenger.h>

#include <misc/dale_config.h>

#include <messenger/ifusiondalemessenger.h>

#include <coma/coma.h>
#include <coma/icoma.h>

#include "ifusiondale.h"



static void
IFusionDale_Destruct( IFusionDale *thiz )
{
     IFusionDale_data *data = (IFusionDale_data*)thiz->priv;

     fd_core_destroy( data->core, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
     
     if (ifusiondale_singleton == thiz)
          ifusiondale_singleton = NULL;
}

static DFBResult
IFusionDale_AddRef( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDale);

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionDale_Release( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDale)

     if (--data->ref == 0)
          IFusionDale_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionDale_CreateMessenger( IFusionDale           *thiz,
                             IFusionDaleMessenger **ret_interface )
{
     DirectResult          ret;
     CoreMessenger        *messenger;
     IFusionDaleMessenger *interface;

     DIRECT_INTERFACE_GET_DATA(IFusionDale)

     /* Check arguments */
     if (!ret_interface)
          return DFB_INVARG;

     /* Create a new messenger. */
     ret = fd_messenger_create( data->core, &messenger );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IFusionDaleMessenger );

     ret = IFusionDaleMessenger_Construct( interface, data->core, messenger );

     fd_messenger_unref( messenger );

     if (ret)
          return ret;

     *ret_interface = interface;

     return DFB_OK;
}

static DFBResult
IFusionDale_GetMessenger( IFusionDale           *thiz,
                          IFusionDaleMessenger **ret_interface )
{
     DirectResult          ret;
     CoreMessenger        *messenger, *tmp;
     IFusionDaleMessenger *interface;

     DIRECT_INTERFACE_GET_DATA(IFusionDale)

     /* Check arguments */
     if (!ret_interface)
          return DFB_INVARG;

     /* Try to get the messenger. */
     ret = fd_core_get_messenger( data->core, 1, &messenger );
     switch (ret) {
          case DFB_OK:
               break;

          case DFB_IDNOTFOUND:
               /* Create a temporary messenger... */
               ret = fd_messenger_create( data->core, &tmp );
               if (ret)
                    return ret;

               /* ...but get the first messenger, to work around race conditions... */
               ret = fd_core_get_messenger( data->core, 1, &messenger );

               /* ...and unref our temporary (most probably the same one). */
               fd_messenger_unref( tmp );

               if (ret)
                    return ret;
               break;

          default:
               return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( interface, IFusionDaleMessenger );

     ret = IFusionDaleMessenger_Construct( interface, data->core, messenger );

     fd_messenger_unref( messenger );

     if (ret)
          return ret;

     *ret_interface = interface;

     return DFB_OK;
}

static DFBResult
IFusionDale_EnterComa( IFusionDale  *thiz,
                       const char   *name,
                       IComa       **ret_interface )
{
     DirectResult  ret;
     Coma         *coma;
     IComa        *interface;

     DIRECT_INTERFACE_GET_DATA(IFusionDale)

     /* Check arguments */
     if (!name || !ret_interface)
          return DFB_INVARG;

     /* Enter the specified Coma. */
     ret = coma_enter( fd_core_world( data->core ), name, &coma );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IComa );

     ret = IComa_Construct( interface, coma );
     if (ret)
          return ret;

     *ret_interface = interface;

     return DFB_OK;
}

DFBResult
IFusionDale_Construct( IFusionDale *thiz )
{
     DFBResult ret;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionDale );

     /* Initialize interface data. */
     data->ref = 1;

     /* Create the core instance. */
     ret = fd_core_create( &data->core );
     if (ret) {
          FusionDaleError( "FusionDale: fd_core_create() failed", ret );

          DIRECT_DEALLOCATE_INTERFACE( thiz );

          return ret;
     }

     /* Assign interface pointers. */
     thiz->AddRef          = IFusionDale_AddRef;
     thiz->Release         = IFusionDale_Release;
     thiz->CreateMessenger = IFusionDale_CreateMessenger;
     thiz->GetMessenger    = IFusionDale_GetMessenger;
     thiz->EnterComa       = IFusionDale_EnterComa;

     return DFB_OK;
}
