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

#ifndef __IFUSIONDALE_ONE_H__
#define __IFUSIONDALE_ONE_H__

#include <One/One.h>

#include <direct/thread.h>

#include <ifusiondale.h>

/*
 * private data struct of IFusionDale_One
 */
typedef struct {
     IFusionDale_data      base;

     OneQID                ns_qid;

     DirectMap            *ns_events_map;
     DirectMap            *ns_components_map;
     DirectMutex           ns_lock;
     DirectWaitQueue       ns_wq;

     DirectThread         *ns_thread;
     bool                  ns_stop;

     DirectLink           *ns_getcomponent_calls;

     OneThread            *thread;
     
     DirectTLS             tlshm_key;

     CoreDale             *core;
} IFusionDale_One_data;

#define FUSIONDALE_NAME_LENGTH 512

typedef enum {
     NS_CREATE_COMPONENT = 1,
     NS_GET_COMPONENT    = 2,
     NS_PING             = 3,
     NS_REGISTER_EVENT   = 4,
     NS_UNREGISTER_EVENT = 5,
     NS_GET_EVENT,
} NSRequestType;

typedef struct {
     OneQID               response_qid;

     char                 name[FUSIONDALE_NAME_LENGTH];

     OneQID               method_qid;

     unsigned int         notifications;

     /* notification QIDs follow */
} CreateComponentRequest;

typedef struct {
     DirectResult         result;
} CreateComponentResponse;


typedef struct {
     OneQID               response_qid;

     int                  timeout_ms;

     char                 name[FUSIONDALE_NAME_LENGTH];
} GetComponentRequest;

typedef struct {
     DirectResult         result;

     OneQID               method_qid;

     unsigned int         notifications;

     /* notification QIDs follow */
} GetComponentResponse;


typedef struct {
     OneQID               response_qid;

     long long            stamp_us;
} PingRequest;

typedef struct {
     long long            stamp_us;
} PingResponse;


typedef struct {
     OneQID               response_qid;

     char                 name[FUSIONDALE_NAME_LENGTH];
} RegisterEventRequest;

typedef struct {
     DirectResult         result;

     OneQID               event_qid;
} RegisterEventResponse;


typedef struct {
     OneQID               response_qid;

     char                 name[FUSIONDALE_NAME_LENGTH];
} UnregisterEventRequest;

typedef struct {
     DirectResult         result;
} UnregisterEventResponse;


typedef struct {
     OneQID               response_qid;

     char                 name[FUSIONDALE_NAME_LENGTH];
} GetEventRequest;

typedef struct {
     DirectResult         result;
} GetEventResponse;


typedef struct {
     void                *local;
     unsigned int         capacity;
     unsigned int         length;

     OneQID               response_qid;

     unsigned int         notification_length;
} FusionDaleTLS;

static inline DirectResult
FusionDaleTLS_Get( IFusionDale    *thiz,
                   FusionDaleTLS **ret_tls )
{
     FusionDaleTLS *fusiondale_tls;

     DIRECT_INTERFACE_GET_DATA( IFusionDale_One )

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_tls != NULL );

     fusiondale_tls = direct_tls_get( data->tlshm_key );
     if (!fusiondale_tls) {
          fusiondale_tls = D_CALLOC( 1, sizeof(FusionDaleTLS) );
          if (!fusiondale_tls)
               return D_OOM();

          direct_tls_set( data->tlshm_key, fusiondale_tls );
     }

     *ret_tls = fusiondale_tls;

     return DR_OK;
}

static inline DirectResult
FusionDaleTLS_GetResponseQID( IFusionDale *thiz,
                              OneQID      *ret_qid )
{
     DirectResult   ret;
     FusionDaleTLS *fusiondale_tls;

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_qid != NULL );

     ret = FusionDaleTLS_Get( thiz, &fusiondale_tls );
     if (ret)
          return ret;

     if (!fusiondale_tls->response_qid) {
          char buf[99];

          ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &fusiondale_tls->response_qid );
          if (ret)
               return ret;

          direct_snprintf( buf, sizeof(buf), "FusionDale/TLS %d of %d", direct_gettid(), getpid() );

          OneQueue_SetName( fusiondale_tls->response_qid, buf );
     }

     *ret_qid = fusiondale_tls->response_qid;

     return DR_OK;
}

static inline DirectResult
FusionDaleTLS_SetNotificationLength( IFusionDale  *thiz,
                                     unsigned int  length )
{
     DirectResult   ret;
     FusionDaleTLS *fusiondale_tls;

     D_ASSERT( thiz != NULL );

     ret = FusionDaleTLS_Get( thiz, &fusiondale_tls );
     if (ret)
          return ret;

     fusiondale_tls->notification_length = length;

     return DR_OK;
}

static inline DirectResult
FusionDaleTLS_GetNotificationLength( IFusionDale  *thiz,
                                     unsigned int *ret_length )
{
     DirectResult   ret;
     FusionDaleTLS *fusiondale_tls;

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_length != NULL );

     ret = FusionDaleTLS_Get( thiz, &fusiondale_tls );
     if (ret)
          return ret;

     *ret_length = fusiondale_tls->notification_length;

     return DR_OK;
}


#endif

