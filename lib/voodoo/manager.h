/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __VOODOO__MANAGER_H__
#define __VOODOO__MANAGER_H__

#include <voodoo/types.h>
#include <voodoo/message.h>


#ifdef __cplusplus
extern "C" {
#include <direct/interface.h>
#include <direct/thread.h>
}

#include <map>

#include <voodoo/instance.h>


typedef struct {
     VoodooMessageBlockType  type;
     unsigned int            len;
     void                   *ptr;
     u32                     val;
} VoodooMessageBlock;


typedef std::map<VoodooInstanceID,VoodooInstance*> InstanceMap;


class VoodooDispatcher;


class VoodooContext {
public:
     virtual DirectResult HandleSuper( VoodooManager    *manager,
                                       const char       *name,
                                       VoodooInstanceID *ret_instance ) = 0;
};


class VoodooManager {
public:
     int                         magic;

     bool                        is_quit;

private:
     friend class VoodooConnection;
     friend class VoodooDispatcher;

     VoodooConnection           *connection;

     VoodooContext              *context;

     size_t                      msg_count;
     VoodooMessageSerial         msg_serial;

     struct {
          DirectMutex            lock;
          InstanceMap            local;
          InstanceMap            remote;
          VoodooInstanceID       last;
     } instances;

     struct {
          DirectMutex            lock;
          DirectWaitQueue        wait_get;
          DirectWaitQueue        wait_put;
          VoodooResponseMessage *current;
     } response;


     VoodooDispatcher           *dispatcher;

     VoodooInstanceID            local_time_service_id;
     VoodooInstanceID            remote_time_service_id;


public:
     VoodooManager( VoodooConnection *connection,
                    VoodooContext    *context );
     ~VoodooManager();


     /** New API **/

     void         DispatchPacket       ( VoodooPacket            *packet );
     bool         DispatchReady        ();   // FIXME: will be obsolete with GetPacket() method, called by connection code to read directly into packet



     /** Old API **/

     void         quit                 ();


     void         handle_disconnect    ();
     void         handle_super         ( VoodooSuperMessage      *super );

     void         handle_request       ( VoodooRequestMessage    *request );
     void         handle_response      ( VoodooResponseMessage   *response );
     void         handle_discover      ( VoodooMessageHeader     *header );

     long long    connection_delay     ();

     long long    clock_to_local       ( long long                remote );
     long long    clock_to_remote      ( long long                local );


private:
     static void *dispatch_async_thread( DirectThread            *thread,
                                         void                    *arg );




public:
     DirectResult do_super             ( const char              *name,
                                         VoodooInstanceID        *ret_instance );

     DirectResult do_request           ( VoodooInstanceID         instance,
                                         VoodooMethodID           method,
                                         VoodooRequestFlags       flags,
                                         VoodooResponseMessage  **ret_response,
                                         VoodooMessageBlock      *blocks = NULL,
                                         size_t                   num_blocks = 0,
                                         size_t                   data_size = 0 );

     DirectResult next_response        ( VoodooResponseMessage   *response,
                                         VoodooResponseMessage  **ret_response );

     DirectResult finish_request       ( VoodooResponseMessage   *response );

     DirectResult do_respond           ( bool                     flush,
                                         VoodooMessageSerial      request,
                                         DirectResult             result,
                                         VoodooInstanceID         instance = VOODOO_INSTANCE_NONE,
                                         VoodooMessageBlock      *blocks = NULL,
                                         size_t                   num_blocks = 0,
                                         size_t                   data_size = 0 );

private:
     inline void  write_blocks         ( void                     *dst,
                                         const VoodooMessageBlock *blocks,
                                         size_t                    num_blocks );

     DirectResult lock_response        ( VoodooMessageSerial      request,
                                         VoodooResponseMessage  **ret_response );

     DirectResult unlock_response      ( VoodooResponseMessage   *response );


public:
     DirectResult register_local       ( VoodooInstance          *instance,
                                         VoodooInstanceID        *ret_instance );

     DirectResult unregister_local     ( VoodooInstanceID         instance_id );

     DirectResult lookup_local         ( VoodooInstanceID         instance_id,
                                         VoodooInstance         **ret_instance );

     DirectResult register_remote      ( VoodooInstance          *instance,
                                         VoodooInstanceID         instance_id );

     DirectResult unregister_remote    ( VoodooInstanceID         instance_id );

     DirectResult lookup_remote        ( VoodooInstanceID         instance_id,
                                         VoodooInstance         **ret_instance );
};
#endif



#ifdef __cplusplus
extern "C" {
#endif

DirectResult VOODOO_API voodoo_manager_create         ( VoodooLink              *link,
                                                        VoodooClient            *client,
                                                        VoodooServer            *server,
                                                        VoodooManager          **ret_manager );

DirectResult VOODOO_API voodoo_manager_quit           ( VoodooManager           *manager );

bool         VOODOO_API voodoo_manager_is_closed      ( const VoodooManager     *manager );

DirectResult VOODOO_API voodoo_manager_destroy        ( VoodooManager           *manager );


/* Super */

DirectResult VOODOO_API voodoo_manager_super          ( VoodooManager           *manager,
                                                        const char              *name,
                                                        VoodooInstanceID        *ret_instance );


/* Request */

DirectResult VOODOO_API voodoo_manager_request        ( VoodooManager           *manager,
                                                        VoodooInstanceID         instance,
                                                        VoodooMethodID           method,
                                                        VoodooRequestFlags       flags,
                                                        VoodooResponseMessage  **ret_response, ... );

DirectResult VOODOO_API voodoo_manager_next_response  ( VoodooManager           *manager,
                                                        VoodooResponseMessage   *response,
                                                        VoodooResponseMessage  **ret_response );

DirectResult VOODOO_API voodoo_manager_finish_request ( VoodooManager           *manager,
                                                        VoodooResponseMessage   *response );


/* Response */

DirectResult VOODOO_API voodoo_manager_respond        ( VoodooManager           *manager,
                                                        bool                     flush,
                                                        VoodooMessageSerial      request,
                                                        DirectResult             result,
                                                        VoodooInstanceID         instance, ... );


/* Instances */

DirectResult VOODOO_API voodoo_manager_register_local ( VoodooManager           *manager,
                                                        VoodooInstanceID         super,
                                                        void                    *dispatcher,
                                                        void                    *real,
                                                        VoodooDispatch           dispatch,
                                                        VoodooInstanceID        *ret_instance_id );

DirectResult VOODOO_API voodoo_manager_unregister_local( VoodooManager           *manager,
                                                         VoodooInstanceID         instance_id );

DirectResult VOODOO_API voodoo_manager_lookup_local   ( VoodooManager           *manager,
                                                        VoodooInstanceID         instance,
                                                        void                   **ret_dispatcher,
                                                        void                   **ret_real );

DirectResult VOODOO_API voodoo_manager_register_remote( VoodooManager           *manager,
                                                        bool                     super,
                                                        void                    *requestor,
                                                        VoodooInstanceID         instance );

DirectResult VOODOO_API voodoo_manager_lookup_remote  ( VoodooManager           *manager,
                                                        VoodooInstanceID         instance,
                                                        void                   **ret_requestor );


/* Security */

DirectResult VOODOO_API voodoo_manager_check_allocation( VoodooManager           *manager,
                                                         unsigned int             amount );


/* Time */

long long    VOODOO_API voodoo_manager_connection_delay( VoodooManager           *manager );

long long    VOODOO_API voodoo_manager_clock_to_local  ( VoodooManager           *manager,
                                                         long long                remote );

long long    VOODOO_API voodoo_manager_clock_to_remote ( VoodooManager           *manager,
                                                         long long                local );

#ifdef __cplusplus
}
#endif


#endif
