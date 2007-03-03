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

DirectResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx,
                  const FusionWorld *world)
{
     FusionCallNew call_new;

     D_ASSERT( call != NULL );
     D_ASSERT( call->handler == NULL );
     D_ASSERT( handler != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     /* Called from others. */
     call_new.handler = handler;
     call_new.ctx     = ctx;

     while (ioctl( world->fusion_fd, FUSION_CALL_NEW, &call_new )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_NEW");

          return DFB_FAILURE;
     }

     memset( call, 0, sizeof(FusionCall) );

     /* Store handler, called directly when called by ourself. */
     call->handler = handler;
     call->ctx     = ctx;

     /* Store call and own fusion id. */
     call->call_id   = call_new.call_id;
     call->fusion_id = fusion_id( world );

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     return DFB_OK;
}

DirectResult
fusion_call_execute (FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     int                 *ret_val)
{
     D_ASSERT( call != NULL );

     if (!call->handler)
          return DFB_DESTROYED;

     if (!(flags & FCEF_NODIRECT) && call->fusion_id == _fusion_id( call->shared )) {
          int ret = call->handler( _fusion_id( call->shared ), call_arg, call_ptr, call->ctx );

          if (ret_val)
               *ret_val = ret;
     }
     else {
          FusionCallExecute execute;

          execute.call_id  = call->call_id;
          execute.call_arg = call_arg;
          execute.call_ptr = call_ptr;
          execute.flags    = flags;

          while (ioctl( _fusion_fd( call->shared ), FUSION_CALL_EXECUTE, &execute )) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call\n");
                         /* fall through */
                    case EIDRM:
                         return DFB_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE");

               return DFB_FAILURE;
          }

          if (ret_val)
               *ret_val = execute.ret_val;
     }

     return DFB_OK;
}

DirectResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL );

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_DESTROY, &call->call_id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Call: invalid call\n");
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_DESTROY");

          return DFB_FAILURE;
     }

     call->handler = NULL;

     return DFB_OK;
}

void
_fusion_call_process( FusionWorld *world, int call_id, FusionCallMessage *call )
{
     FusionCallHandler call_handler;
     FusionCallReturn  call_ret;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( call != NULL );

     call_handler = call->handler;

     D_ASSERT( call_handler != NULL );

     call_ret.call_id = call_id;
     call_ret.val     = call_handler( call->caller, call->call_arg, call->call_ptr, call->ctx );

     while (ioctl (world->fusion_fd, FUSION_CALL_RETURN, &call_ret)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Call: invalid call\n");
                    return;
               default:
                    D_PERROR ("FUSION_CALL_RETURN");
                    return;
          }
     }
}

#else  /* FUSION_BUILD_MULTI */

DirectResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx,
                  const FusionWorld *world)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler == NULL );
     D_ASSERT( handler != NULL );

     /* Called locally. */
     call->handler = handler;
     call->ctx     = ctx;

     return DFB_OK;
}

DirectResult
fusion_call_execute (FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     int                 *ret_val)
{
     int ret;

     D_ASSERT( call != NULL );

     if (!call->handler)
          return DFB_DESTROYED;

     ret = call->handler( 1, call_arg, call_ptr, call->ctx );

     if (ret_val)
          *ret_val = ret;

     return DFB_OK;
}

DirectResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL );

     call->handler = NULL;

     return DFB_OK;
}

#endif

