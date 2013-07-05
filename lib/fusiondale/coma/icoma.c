/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <unistd.h>

#include <fusiondale.h>

#include <direct/interface.h>

#include <fusion/shmalloc.h>

#include <coma/coma.h>
#include <coma/component.h>

#include <coma/icomacomponent.h>

#include "icoma.h"



static void
IComa_Destruct( IComa *thiz )
{
     IComa_data *data = (IComa_data*)thiz->priv;

     coma_exit( data->coma, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IComa_AddRef( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IComa);

     data->ref++;

     return DR_OK;
}

static DirectResult
IComa_Release( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IComa)

     if (--data->ref == 0)
          IComa_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IComa_CreateComponent( IComa           *thiz,
                       const char      *name,
                       ComaMethodFunc   func,
                       int              num_notifications,
                       void            *ctx,
                       IComaComponent **ret_interface )
{
     DirectResult    ret;
     ComaComponent  *component;
     IComaComponent *interface;

     DIRECT_INTERFACE_GET_DATA(IComa)

     /* Check arguments */
     if (!ret_interface)
          return DR_INVARG;

     /* Create a new component. */
     ret = coma_create_component( data->coma, name, func, num_notifications, ctx, &component );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IComaComponent );

     ret = IComaComponent_Construct( interface, data->coma, component, num_notifications );

     coma_component_unref( component );

     if (ret == DR_OK)
          *ret_interface = interface;

     return ret;
}

static DirectResult
IComa_GetComponent( IComa           *thiz,
                    const char      *name,
                    unsigned int     timeout,
                    IComaComponent **ret_interface )
{
     DirectResult    ret;
     ComaComponent  *component;
     IComaComponent *interface;

     DIRECT_INTERFACE_GET_DATA(IComa)

     /* Check arguments */
     if (!ret_interface)
          return DR_INVARG;

     /* Get the component. */
     ret = coma_get_component( data->coma, name, timeout, &component );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IComaComponent );

     ret = IComaComponent_Construct( interface, data->coma, component, component->num_notifications );

     coma_component_unref( component );

     if (ret == DR_OK)
          *ret_interface = interface;

     return DR_OK;
}

static DirectResult
IComa_Allocate( IComa         *thiz,
                unsigned int   bytes,
                void         **ret_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IComa)

     if (!bytes || !ret_ptr)
          return DR_INVARG;

     return coma_allocate( data->coma, bytes, ret_ptr );
}

static DirectResult
IComa_Deallocate( IComa *thiz,
                  void  *ptr )
{
     DIRECT_INTERFACE_GET_DATA(IComa)

     if (!ptr)
          return DR_INVARG;

     return coma_deallocate( data->coma, ptr );
}

static DirectResult
IComa_GetLocal( IComa         *thiz,
                unsigned int   bytes,
                void         **ret_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IComa)

     if (!bytes || !ret_ptr)
          return DR_INVARG;

     return coma_get_local( data->coma, bytes, ret_ptr );
}

static DirectResult
IComa_FreeLocal( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa)

     return coma_free_local( data->coma );
}

DirectResult
IComa_Construct( IComa *thiz, Coma *coma )
{
     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IComa );

     /* Initialize interface data. */
     data->ref  = 1;
     data->coma = coma;

     /* Assign interface pointers. */
     thiz->AddRef          = IComa_AddRef;
     thiz->Release         = IComa_Release;
     thiz->CreateComponent = IComa_CreateComponent;
     thiz->GetComponent    = IComa_GetComponent;
     thiz->Allocate        = IComa_Allocate;
     thiz->Deallocate      = IComa_Deallocate;
     thiz->GetLocal        = IComa_GetLocal;
     thiz->FreeLocal       = IComa_FreeLocal;

     return DR_OK;
}
