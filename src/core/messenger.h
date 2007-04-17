/*
   (c) Copyright 2006-2007  directfb.org

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

#ifndef __FUSIONDALE_CORE_MESSENGER_H__
#define __FUSIONDALE_CORE_MESSENGER_H__

#include <fusiondale.h>

#include <fusion/object.h>

#include <core/dale_types.h>


typedef enum {
     CMNF_NONE     = 0x00000000,

     CMNF_DISPATCH = 0x00000001,

     CMNF_ALL      = 0x00000001
} CoreMessengerNotificationFlags;

typedef struct {
     CoreMessengerNotificationFlags  flags;
     CoreMessenger                  *messenger;

     CoreMessengerDispatch          *dispatch;
} CoreMessengerNotification;

struct __FD_CoreMessenger {
     FusionObject         object;

     int                  magic;

     FusionSHMPoolShared *shmpool;

     FusionSkirmish       lock;
     FusionHash          *hash;
     FDMessengerEventID   last_event;
};

struct __FD_CoreMessengerEvent {
     int                  magic;

     CoreMessenger       *messenger;

     FDMessengerEventID   id;
     char                *name;
//     unsigned int         nodes;
     DirectLink          *nodes;

     DirectLink          *dispatches;
};

struct __FD_CoreMessengerDispatch {
     DirectLink           link;

     int                  magic;

     int                  count;

     FDMessengerEventID   event_id;

     int                  param;
     void                *data;
     unsigned int         data_size;
};

/*
 * Creates a pool of messenger objects.
 */
FusionObjectPool *fd_messenger_pool_create( const FusionWorld *world );

/*
 * Generates fd_messenger_ref(), fd_messenger_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreMessenger, fd_messenger )



/*
 * Creation
 */

DirectResult fd_messenger_create( CoreDale       *core,
                                  CoreMessenger **ret_messenger );

/*
 * Events
 */

DirectResult fd_messenger_create_event ( CoreMessenger       *messenger,
                                         const char          *name,
                                         CoreMessengerEvent **ret_event );

DirectResult fd_messenger_destroy_event( CoreMessenger      *messenger,
                                         CoreMessengerEvent *event );

DirectResult fd_messenger_lookup_event ( CoreMessenger       *messenger,
                                         const char          *name,
                                         CoreMessengerEvent **ret_event );

/*
 * Dispatch
 */

DirectResult fd_messenger_dispatch_event( CoreMessenger      *messenger,
                                          CoreMessengerEvent *event,
                                          int                 param,
                                          void               *data_ptr,
                                          unsigned int        data_size );

/*
 * Locking
 */

static inline DirectResult
fd_messenger_lock( CoreMessenger *messenger )
{
     D_MAGIC_ASSERT( messenger, CoreMessenger );

     return fusion_skirmish_prevail( &messenger->lock );
}

static inline DirectResult
fd_messenger_unlock( CoreMessenger *messenger )
{
     D_MAGIC_ASSERT( messenger, CoreMessenger );

     return fusion_skirmish_dismiss( &messenger->lock );
}

/*
 * Global reactions
 */
typedef enum {
     FD_MESSENGER_PORT_MESSENGER_LISTENER
} FD_MESSENGER_GLOBALS;


#endif
