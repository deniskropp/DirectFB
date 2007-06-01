/*
   (c) Copyright 2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __COMA__COMPONENT_H__
#define __COMA__COMPONENT_H__

#include <fusiondale.h>

#include <fusion/object.h>

#include <coma/coma_types.h>


struct __COMA_ComaComponent {
     FusionObject         object;

     int                  magic;

     FusionSHMPoolShared *shmpool;

     FusionSkirmish       lock;

     char                *name;               /* Name of the component. */

     ComaNotification    *notifications;      /* Shared notification table. */
     unsigned int         num_notifications;  /* Number of notifications. */

     FusionID             provider;           /* Creator of the component. */

     FusionCall           method_call;        /* Call used for method invocation. */
     FusionCall           notify_call;        /* Call used for dispatch callbacks. */

     ComaMethodFunc       method_func;        /* Function pointer for invocations. */
     void                *method_ctx;         /* Context of invocation handler. */
};

/*
 * Creates a pool of component objects.
 */
FusionObjectPool *coma_component_pool_create( Coma *coma );

/*
 * Generates coma_component_ref(), coma_component_attach() etc.
 */
FUSION_OBJECT_METHODS( ComaComponent, coma_component )



/*
 * Object initialization
 */

DirectResult coma_component_init             ( ComaComponent                  *component,
                                               Coma                           *coma,
                                               const char                     *name,
                                               ComaMethodFunc                  func,
                                               int                             num_notifications,
                                               void                           *ctx );

DirectResult coma_component_lock             ( ComaComponent                  *component );
DirectResult coma_component_unlock           ( ComaComponent                  *component );

DirectResult coma_component_init_notification( ComaComponent                  *component,
                                               ComaNotificationID              id,
                                               ComaNotifyFunc                  func,
                                               void                           *ctx );

DirectResult coma_component_call             ( ComaComponent                  *component,
                                               ComaMethodID                    method,
                                               void                           *arg,
                                               int                            *ret_val );

DirectResult coma_component_return           ( ComaComponent                  *component,
                                               unsigned int                    serial,
                                               int                             val );

DirectResult coma_component_notify           ( ComaComponent                  *component,
                                               ComaNotificationID              id,
                                               void                           *arg );

#endif
