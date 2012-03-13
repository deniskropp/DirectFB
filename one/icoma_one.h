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

#ifndef __ICOMA_ONE_H__
#define __ICOMA_ONE_H__

#include <One/One.h>

#include <direct/thread.h>

/*
 * private data struct of IComa_One
 */
typedef struct {
     int                  ref;      /* reference counter */

     OneQID               ns_qid;

     char                *name;

     DirectTLS            tlshm_key;

     OneThread           *thread;
} IComa_One_data;


typedef struct {
     void                *local;
     unsigned int         capacity;
     unsigned int         length;

     OneQID               response_qid;

     unsigned int         notification_length;
} ComaTLS;

static inline DirectResult
ComaTLS_Get( IComa    *thiz,
             ComaTLS **ret_tls )
{
     ComaTLS *coma_tls;

     DIRECT_INTERFACE_GET_DATA(IComa_One)

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_tls != NULL );

     coma_tls = direct_tls_get( data->tlshm_key );
     if (!coma_tls) {
          coma_tls = D_CALLOC( 1, sizeof(ComaTLS) );
          if (!coma_tls)
               return D_OOM();

          direct_tls_set( data->tlshm_key, coma_tls );
     }

     *ret_tls = coma_tls;

     return DR_OK;
}

static inline DirectResult
ComaTLS_GetResponseQID( IComa  *thiz,
                        OneQID *ret_qid )
{
     DirectResult  ret;
     ComaTLS      *coma_tls;

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_qid != NULL );

     ret = ComaTLS_Get( thiz, &coma_tls );
     if (ret)
          return ret;

     if (!coma_tls->response_qid) {
          char buf[99];

          ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &coma_tls->response_qid );
          if (ret)
               return ret;

          direct_snprintf( buf, sizeof(buf), "Coma/TLS %d of %d", direct_gettid(), getpid() );

          OneQueue_SetName( coma_tls->response_qid, buf );
     }

     *ret_qid = coma_tls->response_qid;

     return DR_OK;
}

static inline DirectResult
ComaTLS_SetNotificationLength( IComa        *thiz,
                               unsigned int  length )
{
     DirectResult  ret;
     ComaTLS      *coma_tls;

     D_ASSERT( thiz != NULL );

     ret = ComaTLS_Get( thiz, &coma_tls );
     if (ret)
          return ret;

     coma_tls->notification_length = length;

     return DR_OK;
}

static inline DirectResult
ComaTLS_GetNotificationLength( IComa        *thiz,
                               unsigned int *ret_length )
{
     DirectResult  ret;
     ComaTLS      *coma_tls;

     D_ASSERT( thiz != NULL );
     D_ASSERT( ret_length != NULL );

     ret = ComaTLS_Get( thiz, &coma_tls );
     if (ret)
          return ret;

     *ret_length = coma_tls->notification_length;

     return DR_OK;
}


#define COMA_COMPONENT_NAME_LENGTH 512

typedef enum {
     NS_CREATE_COMPONENT = 1,
     NS_GET_COMPONENT    = 2,
     NS_PING             = 3,
} NSRequestType;

typedef struct {
     OneQID               response_qid;

     char                 name[COMA_COMPONENT_NAME_LENGTH];

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

     char                 name[COMA_COMPONENT_NAME_LENGTH];
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


#endif

