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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <idirectfbimageprovider_dispatcher.h>

#include "idirectfbsurface_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBImageProvider *thiz,
                            VoodooManager          *manager,
                            VoodooInstanceID        instance,
                            void                   *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBImageProvider_Requestor
 */
typedef struct {
     int                    ref;      /* reference counter */

     VoodooManager         *manager;
     VoodooInstanceID       instance;
} IDirectFBImageProvider_Requestor_data;

/**************************************************************************************************/

static void
IDirectFBImageProvider_Requestor_Destruct( IDirectFBImageProvider *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBImageProvider_Requestor_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Requestor_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     if (--data->ref == 0)
          IDirectFBImageProvider_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Requestor_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                        DFBSurfaceDescription  *desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     if (!desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBIMAGEPROVIDER_METHOD_ID_GetSurfaceDescription,
                                   VREQ_RESPOND | VREQ_ASYNC, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, desc, sizeof(DFBSurfaceDescription) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Requestor_GetImageDescription( IDirectFBImageProvider *thiz,
                                                      DFBImageDescription    *desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     if (!desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBIMAGEPROVIDER_METHOD_ID_GetImageDescription,
                                   VREQ_RESPOND | VREQ_ASYNC, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, desc, sizeof(DFBImageDescription) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Requestor_RenderTo( IDirectFBImageProvider *thiz,
                                           IDirectFBSurface       *destination,
                                           const DFBRectangle     *destination_rect )
{
     DirectResult                     ret;
     VoodooResponseMessage           *response;
     IDirectFBSurface_Requestor_data *destination_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     if (!destination)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( destination, destination_data, IDirectFBSurface_Requestor);

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBIMAGEPROVIDER_METHOD_ID_RenderTo,
                                   VREQ_RESPOND | VREQ_ASYNC, &response,
                                   VMBT_ID, destination_data->instance,
                                   VMBT_ODATA, sizeof(DFBRectangle), destination_rect,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBImageProvider_Requestor_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                    DIRenderCallback        callback,
                                                    void                   *callback_data )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           VoodooManager          *manager,
           VoodooInstanceID        instance,
           void                   *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef                = IDirectFBImageProvider_Requestor_AddRef;
     thiz->Release               = IDirectFBImageProvider_Requestor_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_Requestor_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_Requestor_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_Requestor_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_Requestor_SetRenderCallback;

     return DFB_OK;
}

