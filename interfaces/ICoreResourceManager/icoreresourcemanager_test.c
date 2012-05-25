/*
   (c) Copyright 2001-2012  The DirectFB Organization (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdarg.h>

#include <directfb.h>
#include <directfb_windows.h>

#include <direct/debug.h>
#include <direct/interface.h>

#include <core/core.h>

#include "icoreresourcemanager_test.h"
#include "icoreresourcemanager_test_client.h"


D_DEBUG_DOMAIN( ICoreResourceManager_test, "ICoreResourceManager/test", "ICoreResourceManager Interface test Implementation" );

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( ICoreResourceManager, test )

/**********************************************************************************************************************/

static void
ICoreResourceManager_test_Destruct( ICoreResourceManager *thiz )
{
     D_DEBUG_AT( ICoreResourceManager_test, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
ICoreResourceManager_test_AddRef( ICoreResourceManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceManager_test)

     D_DEBUG_AT( ICoreResourceManager_test, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
ICoreResourceManager_test_Release( ICoreResourceManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceManager_test)

     D_DEBUG_AT( ICoreResourceManager_test, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          ICoreResourceManager_test_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
ICoreResourceManager_test_CreateClient( ICoreResourceManager  *thiz,
                                        FusionID               identity,
                                        ICoreResourceClient  **ret_client )
{
     DFBResult            ret;
     ICoreResourceClient *client;

     DIRECT_INTERFACE_GET_DATA(ICoreResourceManager_test)

     D_DEBUG_AT( ICoreResourceManager_test, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_ALLOCATE_INTERFACE( client, ICoreResourceClient );

     ret = ICoreResourceClient_test_Construct( client, thiz, identity );
     if (ret)
          return ret;

     *ret_client = client;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... )
{
     D_DEBUG_AT( ICoreResourceManager_test, "%s()\n", __FUNCTION__ );

     (void) ctx;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     DFBResult             ret  = DFB_INVARG;
     ICoreResourceManager *thiz = interface;
     CoreDFB              *core;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, ICoreResourceManager_test)

     D_DEBUG_AT( ICoreResourceManager_test, "%s( %p )\n", __FUNCTION__, thiz );

     va_list tag;
     va_start(tag, interface);
     core = va_arg(tag, CoreDFB *);
     va_end( tag );

     /* Check arguments. */
     if (!thiz)
          goto error;

     /* Initialize interface data. */
     data->ref  = 1;
     data->core = core;


     /* Initialize function pointer table. */
     thiz->AddRef       = ICoreResourceManager_test_AddRef;
     thiz->Release      = ICoreResourceManager_test_Release;

     thiz->CreateClient = ICoreResourceManager_test_CreateClient;


     return DFB_OK;


error:
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

