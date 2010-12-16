/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <icoma_dispatcher.h>

#include "icoma_requestor.h"


static DirectResult Probe( void );
static DirectResult Construct( IComa            *thiz,
                               VoodooManager    *manager,
                               VoodooInstanceID  instance,
                               void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComa, Requestor )


/**************************************************************************************************/

static void tlshm_destroy( void *arg );

/**********************************************************************************************************************/


static void
IComa_Requestor_Destruct( IComa *thiz )
{
     IComa_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             ICOMA_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComa_Requestor_AddRef( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_Requestor)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComa_Requestor_Release( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_Requestor)

     if (--data->ref == 0)
          IComa_Requestor_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IComa_Requestor_CreateComponent( IComa           *thiz,
                                 const char      *name,
                                 ComaMethodFunc   func,
                                 int              num_notifications,
                                 void            *ctx,
                                 IComaComponent **ret_interface )
{
     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

static DirectResult
IComa_Requestor_GetComponent( IComa           *thiz,
                              const char      *name,
                              unsigned int     timeout,
                              IComaComponent **ret_component )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IComa_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   ICOMA_METHOD_ID_GetComponent, VREQ_RESPOND, &response,
                                   VMBT_STRING, name,
                                   VMBT_UINT, timeout,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IComaComponent",
                                            response->instance, thiz, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_component = interface;

     return ret;
}

static DirectResult
IComa_Requestor_Allocate( IComa         *thiz,
                          unsigned int   bytes,
                          void         **ret_ptr )
{
     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

static DirectResult
IComa_Requestor_Deallocate( IComa *thiz,
                            void  *ptr )
{
     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

static DirectResult
IComa_Requestor_GetLocal( IComa         *thiz,
                          unsigned int   length,
                          void         **ret_ptr )
{
     ComaTLS *coma_tls;

     DIRECT_INTERFACE_GET_DATA(IComa_Requestor)

     coma_tls = pthread_getspecific( data->tlshm_key );
     if (!coma_tls) {
          coma_tls = D_CALLOC( 1, sizeof(ComaTLS) );
          if (!coma_tls)
               return D_OOM();

          pthread_setspecific( data->tlshm_key, coma_tls );
     }

     if (coma_tls->capacity < length) {
          if (coma_tls->local)
               D_FREE( coma_tls->local );

          coma_tls->local    = D_MALLOC( length );
          coma_tls->capacity = length;
     }

     coma_tls->length = length;

     *ret_ptr = coma_tls->local;

     return DR_OK;
}

static DirectResult
IComa_Requestor_FreeLocal( IComa *thiz )
{
     ComaTLS *coma_tls;

     DIRECT_INTERFACE_GET_DATA(IComa_Requestor)

     coma_tls = pthread_getspecific( data->tlshm_key );
     if (!coma_tls)
          return DR_BUG;

     if (coma_tls->local)
          D_FREE( coma_tls->local );

     coma_tls->local    = NULL;
     coma_tls->capacity = 0;
     coma_tls->length   = 0;

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
Probe( void )
{
     /* This implementation has to be loaded explicitly. */
     return DR_UNSUPPORTED;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DirectResult
Construct( IComa            *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComa_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     pthread_key_create( &data->tlshm_key, tlshm_destroy );

     thiz->AddRef          = IComa_Requestor_AddRef;
     thiz->Release         = IComa_Requestor_Release;
     thiz->CreateComponent = IComa_Requestor_CreateComponent;
     thiz->GetComponent    = IComa_Requestor_GetComponent;
     thiz->Allocate        = IComa_Requestor_Allocate;
     thiz->Deallocate      = IComa_Requestor_Deallocate;
     thiz->GetLocal        = IComa_Requestor_GetLocal;
     thiz->FreeLocal       = IComa_Requestor_FreeLocal;

     return DR_OK;
}

/**********************************************************************************************************************/

static void
tlshm_destroy( void *arg )
{
     ComaTLS *coma_tls = arg;

     if (coma_tls->local)
          D_FREE( coma_tls->local );

     D_FREE( coma_tls );
}

