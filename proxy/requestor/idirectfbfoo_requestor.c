/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <idirectfbfoo_dispatcher.h>

#include "idirectfbfoo_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBFoo     *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFoo, Requestor )


/**************************************************************************************************/

static void
IDirectFBFoo_Requestor_Destruct( IDirectFBFoo *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBFoo_Requestor_AddRef( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBFoo_Requestor_Release( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Requestor)

     if (--data->ref == 0)
          IDirectFBFoo_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBFoo_Requestor_Bar( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Requestor)

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
Construct( IDirectFBFoo    *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFoo_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef  = IDirectFBFoo_Requestor_AddRef;
     thiz->Release = IDirectFBFoo_Requestor_Release;
     thiz->Bar     = IDirectFBFoo_Requestor_Bar;

     return DFB_OK;
}

