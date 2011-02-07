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

#ifndef __VOODOO__MANAGER_H__
#define __VOODOO__MANAGER_H__

#include <voodoo/types.h>
#include <voodoo/message.h>


#ifdef __cplusplus
extern "C" {
#include <direct/thread.h>
}

#include <map>


typedef struct {
     bool                        super;
     IAny                       *proxy;
     IAny                       *real;
     VoodooDispatch              dispatch;
} VoodooInstance;

typedef std::map<VoodooInstanceID,VoodooInstance*> InstanceMap;


class VoodooManager {
public:
     int                         magic;

     bool                        is_quit;

private:
     VoodooLink                 *link;
     VoodooConnection           *connection;

     long long                   millis;

     VoodooClient               *client;     /* Either client ... */
     VoodooServer               *server;     /* ... or server is valid. */

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



public:
     VoodooManager( VoodooLink   *link,
                    VoodooClient *client,
                    VoodooServer *server );
     ~VoodooManager();


     void         quit                 ();


     void         handle_disconnect    ();
     void         handle_super         ( VoodooSuperMessage      *super );

     void         handle_request       ( VoodooRequestMessage    *request );
     void         handle_response      ( VoodooResponseMessage   *response );


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
                                         VoodooMessageBlockType   block_type,
                                         va_list                  args );

     DirectResult next_response        ( VoodooResponseMessage   *response,
                                         VoodooResponseMessage  **ret_response );

     DirectResult finish_request       ( VoodooResponseMessage   *response );

     DirectResult do_respond           ( bool                     flush,
                                         VoodooMessageSerial      request,
                                         DirectResult             result,
                                         VoodooInstanceID         instance,
                                         VoodooMessageBlockType   block_type,
                                         va_list                  args );

private:
     inline int   calc_blocks          ( VoodooMessageBlockType   type,
                                         va_list                  args );

     inline void  write_blocks         ( void                    *dst,
                                         VoodooMessageBlockType   type,
                                         va_list                  args );

     DirectResult lock_response        ( VoodooMessageSerial      request,
                                         VoodooResponseMessage  **ret_response );

     DirectResult unlock_response      ( VoodooResponseMessage   *response );


public:
     DirectResult register_local       ( bool                     super,
                                         void                    *dispatcher,
                                         void                    *real,
                                         VoodooDispatch           dispatch,
                                         VoodooInstanceID        *ret_instance );

     DirectResult unregister_local     ( VoodooInstanceID         instance_id );

     DirectResult lookup_local         ( VoodooInstanceID         instance_id,
                                         void                   **ret_dispatcher,
                                         void                   **ret_real );

     DirectResult register_remote      ( bool                     super,
                                         void                    *requestor,
                                         VoodooInstanceID         instance_id );

     DirectResult lookup_remote        ( VoodooInstanceID         instance_id,
                                         void                   **ret_requestor );
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
                                                        VoodooResponseMessage  **ret_response,
                                                        VoodooMessageBlockType   block_type, ... );

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
                                                        VoodooInstanceID         instance,
                                                        VoodooMessageBlockType   block_type, ... );


/* Instances */

DirectResult VOODOO_API voodoo_manager_register_local ( VoodooManager           *manager,
                                                        bool                     super,
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

#ifdef __cplusplus
}
#endif


#endif
