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

#include "idirectfbimageprovider_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBImageProvider *thiz,
                            IDirectFBImageProvider *real,
                            VoodooManager          *manager,
                            VoodooInstanceID        super,
                            void                   *arg,
                            VoodooInstanceID       *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBImageProvider_Dispatcher
 */
typedef struct {
     int                     ref;      /* reference counter */

     IDirectFBImageProvider *real;
} IDirectFBImageProvider_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBImageProvider_Dispatcher_Destruct( IDirectFBImageProvider *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBImageProvider_Dispatcher_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Dispatcher_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     if (--data->ref == 0)
          IDirectFBImageProvider_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Dispatcher_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                         DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_Dispatcher_GetImageDescription( IDirectFBImageProvider *thiz,
                                                       DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_Dispatcher_RenderTo( IDirectFBImageProvider *thiz,
                                            IDirectFBSurface       *destination,
                                            const DFBRectangle     *destination_rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_Dispatcher_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                     DIRenderCallback        callback,
                                                     void                   *callback_data )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetSurfaceDescription( IDirectFBImageProvider *thiz, IDirectFBImageProvider *real,
                                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     DFBSurfaceDescription desc;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     ret = real->GetSurfaceDescription( real, &desc );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBSurfaceDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetImageDescription( IDirectFBImageProvider *thiz, IDirectFBImageProvider *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     DFBImageDescription desc;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     ret = real->GetImageDescription( real, &desc );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBImageDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_RenderTo( IDirectFBImageProvider *thiz, IDirectFBImageProvider *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     const DFBRectangle  *rect;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_ODATA( parser, rect );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     ret = real->RenderTo( real, surface, rect );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBImageProvider/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBIMAGEPROVIDER_METHOD_ID_GetSurfaceDescription:
               return Dispatch_GetSurfaceDescription( dispatcher, real, manager, msg );

          case IDIRECTFBIMAGEPROVIDER_METHOD_ID_GetImageDescription:
               return Dispatch_GetImageDescription( dispatcher, real, manager, msg );

          case IDIRECTFBIMAGEPROVIDER_METHOD_ID_RenderTo:
               return Dispatch_RenderTo( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
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
           IDirectFBImageProvider *real,
           VoodooManager          *manager,
           VoodooInstanceID        super,
           void                   *arg,      /* Optional arguments to constructor */
           VoodooInstanceID       *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref  = 1;
     data->real = real;

     thiz->AddRef                = IDirectFBImageProvider_Dispatcher_AddRef;
     thiz->Release               = IDirectFBImageProvider_Dispatcher_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_Dispatcher_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_Dispatcher_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_Dispatcher_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_Dispatcher_SetRenderCallback;

     return DFB_OK;
}

