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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <fusion/build.h>

#if FUSION_BUILD_MULTI
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/types.h>
#include <fusion/call.h>

#include "fusion_internal.h"


#if FUSION_BUILD_MULTI

FusionResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx)
{
     FusionCallNew call_new;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler == NULL );
     D_ASSERT( handler != NULL );

     /* Called from others. */
     call_new.handler = handler;
     call_new.ctx     = ctx;

     while (ioctl (_fusion_fd, FUSION_CALL_NEW, &call_new)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_NEW");

          return FUSION_FAILURE;
     }

     /* Called locally. */
     call->handler = handler;
     call->ctx     = ctx;

     /* Store call and fusion id for local (direct) calls. */
     call->call_id   = call_new.call_id;
     call->fusion_id = _fusion_id;

     return FUSION_SUCCESS;
}

FusionResult
fusion_call_execute (FusionCall *call,
                     int         call_arg,
                     void       *call_ptr,
                     int        *ret_val)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( call != NULL );

     if (!call->handler)
          return FUSION_DESTROYED;

     if (call->fusion_id == _fusion_id) {
          int ret = call->handler( _fusion_id, call_arg, call_ptr, call->ctx );

          if (ret_val)
               *ret_val = ret;
     }
     else {
          FusionCallExecute execute;

          execute.call_id  = call->call_id;
          execute.call_arg = call_arg;
          execute.call_ptr = call_ptr;

          while (ioctl (_fusion_fd, FUSION_CALL_EXECUTE, &execute)) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call\n");
                         /* fall through */
                    case EIDRM:
                         return FUSION_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE");

               return FUSION_FAILURE;
          }

          if (ret_val)
               *ret_val = execute.ret_val;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_call_return (int call_id, int val)
{
     FusionCallReturn call_ret = { call_id, val };

     D_ASSERT( _fusion_fd != -1 );

     while (ioctl (_fusion_fd, FUSION_CALL_RETURN, &call_ret)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Call: invalid call\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_RETURN");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL );

     while (ioctl (_fusion_fd, FUSION_CALL_DESTROY, &call->call_id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Call: invalid call\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_DESTROY");

          return FUSION_FAILURE;
     }

     call->handler = NULL;

     return FUSION_SUCCESS;
}

void
_fusion_call_process( int call_id, FusionCallMessage *call )
{
     FusionCallHandler handler;

     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL );

     handler = call->handler;

     fusion_call_return( call_id, handler( call->caller, call->call_arg,
                                           call->call_ptr, call->ctx ) );
}

#else  /* FUSION_BUILD_MULTI */

FusionResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler == NULL );
     D_ASSERT( handler != NULL );

     /* Called locally. */
     call->handler = handler;
     call->ctx     = ctx;

     return FUSION_SUCCESS;
}

FusionResult
fusion_call_execute (FusionCall *call,
                     int         call_arg,
                     void       *call_ptr,
                     int        *ret_val)
{
     int ret;

     D_ASSERT( call != NULL );

     if (!call->handler)
          return FUSION_DESTROYED;

     ret = call->handler( 1, call_arg, call_ptr, call->ctx );

     if (ret_val)
          *ret_val = ret;

     return FUSION_SUCCESS;
}

FusionResult
fusion_call_return (int call_id,
                    int ret_val)
{
     return FUSION_SUCCESS;
}

FusionResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL );

     call->handler = NULL;

     return FUSION_SUCCESS;
}

#endif

