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

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <coma/coma.h>

#include "ifusiondale_one.h"
#include "icoma_one.h"


static DirectResult Probe( void );
static DirectResult Construct( IFusionDale *dale,
                               IComa       *thiz,
                               OneQID       ns_qid,
                               const char  *name,
                               OneThread   *thread );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComa, One )


D_DEBUG_DOMAIN( IComa_One, "IComa/One", "IComa One" );

/**************************************************************************************************/


static void
IComa_One_Destruct( IComa *thiz )
{
     //IComa_One_data *data = thiz->priv;

     D_DEBUG_AT( IComa_One, "%s (%p)\n", __FUNCTION__, thiz );

     One_Shutdown();

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComa_One_AddRef( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_One)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComa_One_Release( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_One)

     if (--data->ref == 0)
          IComa_One_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IComa_One_CreateComponent( IComa           *thiz,
                           const char      *name,
                           ComaMethodFunc   func,
                           int              num_notifications,
                           void            *ctx,
                           IComaComponent **ret_component )
{
     DirectResult             ret;
     int                      i;
     NSRequestType            type;
     CreateComponentRequest   request;
     CreateComponentResponse  response;
     size_t                   length;
     void                    *datas[3];
     size_t                   lengths[3];
     OneQID                   notification_qids[num_notifications];
     DirectInterfaceFuncs    *funcs;
     void                    *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     D_DEBUG_AT( IComa_One, "%s( '%s' )\n", __FUNCTION__, name );

     D_ASSERT( ret_component != NULL );

     ret = FusionDaleTLS_GetResponseQID( data->dale, &request.response_qid );
     if (ret)
          return ret;

     direct_snprintf( request.name, sizeof(request.name), "%s/%s", data->name, name );

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &request.method_qid );
     if (ret)
          return ret;

     OneQueue_SetName( request.method_qid, request.name );

     for (i=0; i<num_notifications; i++) {
          char qname[99];

          ret = OneQueue_New( ONE_QUEUE_VIRTUAL, ONE_QID_NONE, &notification_qids[i] );
          if (ret)
               goto error;

          direct_snprintf( qname, sizeof(qname), "%s/%d", request.name, i );

          OneQueue_SetName( notification_qids[i], qname );
     }

     request.notifications = num_notifications;

     D_DEBUG_AT( IComa_One, "  -> register '%s' with QID %u (%d notifications)...\n", request.name, request.method_qid, num_notifications );

     type = NS_CREATE_COMPONENT;

     datas[0]   = &type;
     lengths[0] = sizeof(type);

     datas[1]   = &request;
     lengths[1] = sizeof(request);

     datas[2]   = notification_qids;
     lengths[2] = num_notifications * sizeof(OneQID);

     ret = OneQueue_DispatchV( data->ns_qid, datas, lengths, num_notifications ? 3 : 2 );
     if (ret)
          goto error;

     D_DEBUG_AT( IComa_One, "  -> dispatched, receiving response...\n" );

     ret = OneQueue_Receive( &request.response_qid, 1, &response, sizeof(response), &length, true, 0 );
     if (ret)
          goto error;

     D_DEBUG_AT( IComa_One, "  -> result is '%s'\n", DirectResultString( response.result ) );

     if (response.result) {
          D_DERROR( response.result, "IComa/One: CreateComponent() request failed!\n" );
          ret = response.result;
          goto error;
     }

     ret = DirectGetInterface( &funcs, "IComaComponent", "One", NULL, NULL );
     if (ret)
          goto error;

     ret = funcs->Allocate( &interface_ptr );
     if (ret)
          goto error;

     ret = funcs->Construct( interface_ptr, thiz, request.name, request.method_qid, func, ctx, notification_qids, num_notifications, data->thread );
     if (ret)
          goto error;

     *ret_component = interface_ptr;

     return DR_OK;


error:
     for (--i; i>=0; i--)
          OneQueue_Destroy( notification_qids[i] );

     OneQueue_Destroy( request.method_qid );

     return ret;
}

static DirectResult
IComa_One_GetComponent( IComa           *thiz,
                        const char      *name,
                        unsigned int     timeout,
                        IComaComponent **ret_component )
{
     DirectResult          ret;
     NSRequestType         type;
     GetComponentRequest   request;
     GetComponentResponse *response;
     size_t                length;
     void                 *datas[2];
     size_t                lengths[2];
     DirectInterfaceFuncs *funcs;
     void                 *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     D_DEBUG_AT( IComa_One, "%s( '%s' )\n", __FUNCTION__, name );

     D_ASSERT( ret_component != NULL );

     response = alloca( 9999 );

     ret = FusionDaleTLS_GetResponseQID( data->dale, &request.response_qid );
     if (ret)
          return ret;

     request.timeout_ms = timeout;

     direct_snprintf( request.name, sizeof(request.name), "%s/%s", data->name, name );

     D_DEBUG_AT( IComa_One, "  -> query '%s'...\n", request.name );

     type = NS_GET_COMPONENT;

     datas[0]   = &type;
     lengths[0] = sizeof(type);

     datas[1]   = &request;
     lengths[1] = sizeof(request);

     ret = OneQueue_DispatchV( data->ns_qid, datas, lengths, 2 );
     if (ret)
          return ret;

     D_DEBUG_AT( IComa_One, "  -> dispatched, receiving response...\n" );

     ret = OneQueue_Receive( &request.response_qid, 1, response, 9999, &length, true, 0 );
     if (ret)
          return ret;

     D_DEBUG_AT( IComa_One, "  -> result is '%s'\n", DirectResultString( response->result ) );

     if (response->result) {
          D_DERROR( response->result, "IComa/One: GetComponent() request failed!\n" );
          return response->result;
     }

     D_DEBUG_AT( IComa_One, "  -> received method QID %u, %u notifications\n", response->method_qid, response->notifications );

     ret = DirectGetInterface( &funcs, "IComaComponent", "One", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface_ptr );
     if (ret)
          return ret;

     ret = funcs->Construct( interface_ptr, thiz, request.name, response->method_qid, NULL, NULL,
                             response->notifications ? response + 1 : NULL, response->notifications,
                             data->thread );
     if (ret)
          return ret;

     *ret_component = interface_ptr;

     return ret;
}

static DirectResult
IComa_One_Allocate( IComa         *thiz,
                    unsigned int   bytes,
                    void         **ret_ptr )
{
     ComaAllocation *allocation;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     D_DEBUG_AT( IComa_One, "%s( %u )\n", __FUNCTION__, bytes );

     D_ASSERT( ret_ptr != NULL );

     allocation = D_CALLOC( 1, sizeof(ComaAllocation) + bytes );
     if (!allocation)
          return D_OOM();

     allocation->magic  = D_MAGIC( "ComaAllocation" );
     allocation->length = bytes;

     *ret_ptr = allocation + 1;

     D_DEBUG_AT( IComa_One, "  -> ptr %p\n", *ret_ptr );

     return DR_OK;
}

static DirectResult
IComa_One_Deallocate( IComa *thiz,
                      void  *ptr )
{
     ComaAllocation *allocation;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     D_DEBUG_AT( IComa_One, "%s( %p )\n", __FUNCTION__, ptr );

     D_ASSERT( ptr != NULL );

     allocation = ptr - sizeof(ComaAllocation);

     if (allocation->magic != D_MAGIC( "ComaAllocation" ))
          return DR_INVARG;

     D_DEBUG_AT( IComa_One, "  -> length %u\n", allocation->length );

     D_FREE( allocation );

     return DR_OK;
}

static DirectResult
IComa_One_GetLocal( IComa         *thiz,
                    unsigned int   length,
                    void         **ret_ptr )
{
     DirectResult   ret;
     FusionDaleTLS *fusiondale_tls;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     ret = FusionDaleTLS_Get( data->dale, &fusiondale_tls );
     if (ret)
          return ret;

     if (fusiondale_tls->capacity < length) {
          if (fusiondale_tls->local)
               D_FREE( fusiondale_tls->local );

          fusiondale_tls->local    = D_MALLOC( length );
          fusiondale_tls->capacity = length;
     }

     fusiondale_tls->length = length;

     *ret_ptr = fusiondale_tls->local;

     return DR_OK;
}

static DirectResult
IComa_One_FreeLocal( IComa *thiz )
{
     DirectResult   ret;
     FusionDaleTLS *fusiondale_tls;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     ret = FusionDaleTLS_Get( data->dale, &fusiondale_tls );
     if (ret)
          return ret;

     if (fusiondale_tls->local)
          D_FREE( fusiondale_tls->local );

     fusiondale_tls->local    = NULL;
     fusiondale_tls->capacity = 0;
     fusiondale_tls->length   = 0;

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
Construct( IFusionDale *dale,
           IComa       *thiz,
           OneQID       ns_qid,
           const char  *name,
           OneThread   *thread )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComa_One)

     ret = One_Initialize();
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->dale   = dale;
     data->ref    = 1;
     data->ns_qid = ns_qid;
     data->thread = thread;

     data->name = D_STRDUP( name );
     if (!data->name) {
          One_Shutdown();
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return D_OOM();
     }

     thiz->AddRef          = IComa_One_AddRef;
     thiz->Release         = IComa_One_Release;
     thiz->CreateComponent = IComa_One_CreateComponent;
     thiz->GetComponent    = IComa_One_GetComponent;
     thiz->Allocate        = IComa_One_Allocate;
     thiz->Deallocate      = IComa_One_Deallocate;
     thiz->GetLocal        = IComa_One_GetLocal;
     thiz->FreeLocal       = IComa_One_FreeLocal;

     return DR_OK;
}

