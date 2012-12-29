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

#ifndef __FUSION__FUSION_INTERNAL_H__
#define __FUSION__FUSION_INTERNAL_H__

#include <sys/types.h>
//#include <sys/param.h>

#include <string.h>

#include <direct/list.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/lock.h>
#include <fusion/ref.h>
#include <fusion/shm/shm_internal.h>

#if FUSION_BUILD_MULTI
# if FUSION_BUILD_KERNEL
#  include <sys/ioctl.h>
#  include <linux/fusion.h>
# else
#  include <fusion/protocol.h>
# endif
#endif

#define FUSION_MAX_WORLDS     8

#define EXECUTE3_BIN_FLUSH_MILLIS    16

/***************************************
 *  Fusion internal type declarations  *
 ***************************************/

struct __Fusion_FusionWorldShared {
     int                  magic;
     
     int                  refs;     /* Increased by the master on fork(). */

     int                  world_index;

     int                  world_abi;

     long long            start_time;

     DirectLink          *arenas;
     FusionSkirmish       arenas_lock;

     FusionSkirmish       reactor_globals;

     FusionSHMShared      shm;

     FusionSHMPoolShared *main_pool;
     
     DirectLink          *fusionees;   /* Connected fusionees. */
     FusionSkirmish       fusionees_lock;
    
     unsigned int         call_ids;    /* Generates call ids. */
     unsigned int         lock_ids;    /* Generates locks ids. */
     unsigned int         ref_ids;     /* Generates refs ids. */
     unsigned int         reactor_ids; /* Generates reactors ids. */
     unsigned int         pool_ids;    /* Generates pools ids. */

     void                *pool_base;   /* SHM pool allocation base. */ 
     void                *pool_max;    /* SHM pool max address. */

     void                *world_root;
     
     FusionWorld         *world;
};

#if !FUSION_BUILD_MULTI

#include "reactor.h"

#define EVENT_DISPATCHER_BUFFER_LENGTH (64 * 1024)

typedef struct {
     DirectLink link;

     int magic;

     char       buffer[EVENT_DISPATCHER_BUFFER_LENGTH];
     int        read_pos;
     int        write_pos;
     int        can_free;
     int        sync_calls;
     int        pending;
} FusionEventDispatcherBuffer;

typedef struct
{
     int                  reaction;
     FusionCallHandler    call_handler;
     FusionCallHandler3   call_handler3;
     void                *call_ctx;
     FusionCallExecFlags  flags;
     int                  call_arg;
     void                *ptr;
     unsigned int         length;
     int                  ret_val;
     void                *ret_ptr;
     unsigned int         ret_size;
     unsigned int         ret_length;
     int                  processed;
     
} FusionEventDispatcherCall;

//pass fusion calls to single-app dispatcher thread
DirectResult _fusion_event_dispatcher_process( FusionWorld *world, const FusionEventDispatcherCall *call, FusionEventDispatcherCall **ret );
DirectResult _fusion_event_dispatcher_process_reactions( FusionWorld *world, FusionReactor *reactor, int channel, void *msg_data, int msg_size );
DirectResult _fusion_event_dispatcher_process_reactor_free( FusionWorld *world, FusionReactor *reactor );
#endif /* !FUSION_BUILD_MULTI */

struct __Fusion_FusionWorld {
     int                  magic;

     int                  refs;

     FusionWorldShared   *shared;

     int                  fusion_fd;
     FusionID             fusion_id;

     DirectThread        *dispatch_loop;
     bool                 dispatch_stop;

     /*
      * List of reactors with at least one local reaction attached.
      */
     DirectLink          *reactor_nodes;
     DirectMutex          reactor_nodes_lock;

     FusionSHM            shm;

     FusionForkAction     fork_action;
     FusionForkCallback   fork_callback;

     void                *fusionee;

     struct {
          DirectThread        *thread;
          pthread_cond_t       queue;
          pthread_mutex_t      lock;
          DirectLink          *list;
     }                    deferred;

     FusionLeaveCallback  leave_callback;
     void                *leave_ctx;

     DirectLink          *dispatch_cleanups;

#if !FUSION_BUILD_MULTI
     DirectThread        *event_dispatcher_thread;
     DirectMutex          event_dispatcher_mutex;
     DirectWaitQueue      event_dispatcher_cond;
     DirectWaitQueue      event_dispatcher_process_cond;
     DirectLink          *event_dispatcher_buffers;
     DirectLink          *event_dispatcher_buffers_remove;
     DirectMutex          event_dispatcher_call_mutex;
     DirectWaitQueue      event_dispatcher_call_cond;
#endif
};

#if FUSION_BUILD_MULTI
# if FUSION_BUILD_KERNEL
typedef struct {
     DirectLink          link;

     FusionReadMessage   header;

     /* message data follows */
} DeferredCall;
# endif
#endif

/*******************************************
 *  Fusion internal function declarations  *
 *******************************************/

int      _fusion_fd( const FusionWorldShared *shared );
FusionID _fusion_id( const FusionWorldShared *shared );

FusionWorld *_fusion_world( const FusionWorldShared *shared );

/*
 * from reactor.c
 */
void _fusion_reactor_free_all       ( FusionWorld   *world );
void _fusion_reactor_process_message( FusionWorld   *world,
                                      int            reactor_id,
                                      int            channel,
                                      const void    *msg_data );


#if FUSION_BUILD_MULTI
# if FUSION_BUILD_KERNEL
static __inline__ void
fusion_entry_add_permissions( const FusionWorld *world,
                              FusionType         type,
                              int                entry_id,
                              FusionID           fusion_id,
                              ... )
{
     FusionEntryPermissions permissions;
     va_list                args;
     int                    arg;

     permissions.type        = type;
     permissions.id          = entry_id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     va_start( args, fusion_id );

     while ((arg = va_arg( args, int )) != 0)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, arg );

     va_end( args );

     while (ioctl( world->fusion_fd, FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion: FUSION_ENTRY_ADD_PERMISSIONS( type %d, id %d ) failed!\n", type, entry_id );
               break;
          }
     }
}
# endif
#endif


#if FUSION_BUILD_MULTI
/*
 * from call.c
 */
void _fusion_call_process ( FusionWorld        *world,
                            int                 call_id,
                            FusionCallMessage  *call,
                            void               *ptr );
void _fusion_call_process3( FusionWorld        *world,
                            int                 call_id,
                            FusionCallMessage3 *msg,
                            void               *ptr );

#if FUSION_BUILD_KERNEL
/*
 * from shm.c
 */
void _fusion_shmpool_process( FusionWorld          *world,
                              int                   pool_id,
                              FusionSHMPoolMessage *msg );
#else
/*
 * form fusion.c
 */ 
void _fusion_add_local( FusionWorld *world,
                        FusionRef   *ref,
                        int          add );

void _fusion_check_locals( FusionWorld *world,
                           FusionRef   *ref );

void _fusion_remove_all_locals( FusionWorld     *world,
                                const FusionRef *ref );
                               
DirectResult _fusion_send_message( int                  fd, 
                                   const void          *msg, 
                                   size_t               msg_size,
                                   struct sockaddr_un  *addr );                                   
DirectResult _fusion_recv_message( int                  fd, 
                                   void                *msg,
                                   size_t               msg_size,
                                   struct sockaddr_un  *addr );

/*
 * from ref.c
 */
DirectResult _fusion_ref_change( FusionRef *ref, int add, bool global );
                                   
#endif /* FUSION_BUILD_KERNEL */
#endif /* FUSION_BUILD_MULTI */

#endif
