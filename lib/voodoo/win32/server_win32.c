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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/server.h>

#ifndef htons
#if WORDS_BIGENDIAN
#define htons(x) (x)
#else
#define htons(x) ((((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00))
#endif
#endif


typedef struct {
//     sem_t          sem;

     pid_t          single_pid;
} ServerShared;

typedef struct {
     DirectLink     link;

     VoodooLink     vl;
     VoodooManager *manager;
} Connection;

typedef struct {
     const char           *name;
     VoodooSuperConstruct  func;
     void                 *ctx;

     IAny                 *interface_ptr;
} Super;

#define MAX_SUPER   8

struct __V_VoodooServer {
//     int         fd;

     bool        fork;
     bool        quit;
     DirectLink *connections;

     int         num_super;
     Super       supers[MAX_SUPER];

     ServerShared *shared;
};

D_DEBUG_DOMAIN( Voodoo_Server, "Voodoo/Server", "Voodoo Server" );

/**********************************************************************************************************************/

static DirectResult accept_connection( VoodooServer *server, int fd );

/**********************************************************************************************************************/

static const int one = 1;

/**********************************************************************************************************************/

DirectResult
voodoo_server_create( const char    *_addr,
                      int            port,
                      bool           fork,
                      VoodooServer **ret_server )
{
     DirectResult        ret;
//     struct sockaddr_in  addr;
//     int                 fd     = -1;
     VoodooServer       *server = NULL;

     D_ASSERT( ret_server != NULL );
#if 0
     /* Create the server socket. */
     fd = socket( PF_INET, SOCK_STREAM, 0 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not create the socket via socket()!\n" );
          goto error;
     }

     /* Allow reuse of local address. */
     if (setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Server: Could not set SO_REUSEADDR!\n" );

     /* Bind the socket to the local port. */
     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = inet_addr( _addr ?: "0.0.0.0" );
     addr.sin_port        = htons( port ?: 2323 );

     if (bind( fd, &addr, sizeof(addr) )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not bind() the socket!\n" );
          goto error;
     }

     /* Start listening. */
     if (listen( fd, 4 )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not listen() to the socket!\n" );
          goto error;
     }
#endif

     /* Allocate server structure. */
     server = D_CALLOC( 1, sizeof(VoodooServer) );
     if (!server) {
          ret = D_OOM();
          D_WARN( "out of memory" );
          goto error;
     }

     /* Initialize server structure. */
//     server->fd = fd;
#if 0
     {
          int zfd;

          zfd = open( "/dev/zero", O_RDWR );
          if (zfd < 0) {
               ret = errno2result( errno );
               D_PERROR( "Voodoo/Server: Failed to open /dev/zero!\n" );
               goto error;
          }

          server->shared = mmap( NULL, sizeof(ServerShared), PROT_READ | PROT_WRITE, MAP_SHARED, zfd, 0 );

          close( zfd );

          if (server->shared == MAP_FAILED) {
               ret = errno2result( errno );
               D_PERROR( "Voodoo/Server: Failed to mmap /dev/zero!\n" );
               server->shared = NULL;
               goto error;
          }

          if (sem_init( &server->shared->sem, 1, 1 )) {
               ret = errno2result( errno );
               D_PERROR( "Voodoo/Server: Failed to create process shared semaphore!\n" );
               goto error;
          }
     }
#endif

     /* Return the new server. */
     *ret_server = server;

     return DR_OK;


error:
     if (server) {
//          if (server->shared)
//               munmap( server->shared, sizeof(ServerShared) );

          D_FREE( server );
     }

//     if (fd >= 0)
//          close( fd );

     return ret;
}

DirectResult
voodoo_server_register( VoodooServer         *server,
                        const char           *name,
                        VoodooSuperConstruct  func,
                        void                 *ctx )
{
     Super *super;

     D_ASSERT( server != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( func != NULL );

     if (server->num_super == MAX_SUPER)
          return DR_LIMITEXCEEDED;

     super = &server->supers[server->num_super++];

     super->name = name;
     super->func = func;
     super->ctx  = ctx;

     return DR_OK;
}

static __inline__ Super *
lookup_super( VoodooServer *server,
              const char   *name )
{
     int i;

     D_ASSERT( server != NULL );
     D_ASSERT( name != NULL );

     for (i=0; i<server->num_super; i++) {
          Super *super = &server->supers[i];

          if (! strcmp( name, super->name ))
               return super;
     }

     return NULL;
}

DirectResult
voodoo_server_construct( VoodooServer      *server,
                         VoodooManager     *manager,
                         const char        *name,
                         VoodooInstanceID  *ret_instance )
{
     DirectResult      ret;
     Super            *super;
     VoodooInstanceID  instance;

     D_ASSERT( server != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     super = lookup_super( server, name );
     if (!super) {
          D_ERROR( "Voodoo/Server: Super interface '%s' is not available!\n", name );
          return DR_UNSUPPORTED;
     }

#if 0
     if (voodoo_config->server_single && !strcmp( name, voodoo_config->server_single )) {
          sem_wait( &server->shared->sem );

          if (server->shared->single_pid) {
               D_INFO( "Voodoo/Server: Killing previous single process with pid %d\n", server->shared->single_pid );
               kill( server->shared->single_pid, SIGTERM );
          }

          server->shared->single_pid = getpid();

          D_INFO( "Voodoo/Server: New single process has pid %d\n", server->shared->single_pid );

          sem_post( &server->shared->sem );
     }
#endif

     ret = super->func( server, manager, name, super->ctx, &instance );
     if (ret) {
          D_ERROR( "Voodoo/Server: "
                   "Creating super interface '%s' failed (%s)!\n", name, DirectResultString(ret) );
          return ret;
     }

     *ret_instance = instance;

     return DR_OK;
}

DirectResult
voodoo_server_run( VoodooServer *server )
{
     DirectLink    *l, *n;
//     struct pollfd  pf;
     bool           listener = true;

     D_ASSERT( server != NULL );

     while (!server->quit) {
          /* Cleanup dead connections. */
          direct_list_foreach_safe (l, n, server->connections) {
               Connection *connection = (Connection*) l;

               if (voodoo_manager_is_closed( connection->manager )) {
                    D_DEBUG_AT( Voodoo_Server, "  -> closed manager\n" );

#if 0
                    sem_wait( &server->shared->sem );

                    if (server->shared->single_pid == getpid()) {
                         D_INFO( "Voodoo/Server: Closing single process with pid %d\n", server->shared->single_pid );
                         server->shared->single_pid = 0;
                    }

                    sem_post( &server->shared->sem );
#endif

                    voodoo_manager_destroy( connection->manager );

//                    close( connection->fd );

                    direct_list_remove( &server->connections, l );

                    D_INFO( "Voodoo/Server: Closed connection.\n" );

                    D_FREE( connection );

                    if (!server->connections)
                         return DR_OK;
               }
          }
#if 0
          if (listener) {
               int                fd;
               struct sockaddr_in addr;
               socklen_t          addrlen = sizeof(addr);
               char               buf[100];

               pf.fd     = server->fd;
               pf.events = POLLIN;

               switch (poll( &pf, 1, 20000 )) {
                    default:
                         fd = accept( server->fd, &addr, &addrlen );
                         if (fd < 0) {
                              D_PERROR( "Voodoo/Server: Could not accept() incoming connection!\n" );
                              break;
                         }

                         inet_ntop( AF_INET, &addr.sin_addr, buf, sizeof(buf) );

                         D_INFO( "Voodoo/Server: Accepted connection from '%s'\n", buf );

                         if (server->fork) {
                              pid_t pid;

                              D_DEBUG_AT( Voodoo_Server, "  -> forking...\n" );

                              pid = fork();
                              switch (pid) {
                                   case 0:
                                        D_DEBUG_AT( Voodoo_Server, "  ===> In child process\n" );

                                        listener = false;

                                        close( server->fd );

                                        accept_connection( server, fd );
                                        break;

                                   case -1:
                                        D_PERROR( "Voodoo/Server: Could not fork()!\n" );
                                        break;

                                   default:
                                        D_DEBUG_AT( Voodoo_Server, "  => Child process has pid %d\n", pid );

                                        close( fd );
                                        break;
                              }
                         }
                         else {
                              D_DEBUG_AT( Voodoo_Server, "  -> running threaded only...\n" );

                              accept_connection( server, fd );
                         }

                         break;

                    case 0:
                         waitpid( -1, NULL, WNOHANG );

                         D_DEBUG_AT( Voodoo_Server, "  -> Timeout during poll()\n" );
                         break;

                    case -1:
                         if (errno != EINTR) {
                              D_PERROR( "Voodoo/Server: Could not poll() the socket!\n" );
                              server->quit = true;
                         }
                         break;
               }
          }
          else
#endif
               direct_thread_sleep( 2000000 );
     }

     return DR_OK;
}

DirectResult
voodoo_server_destroy( VoodooServer *server )
{
     DirectLink *l;

     D_ASSERT( server != NULL );

//     close( server->fd );

     /* Close all connections. */
     direct_list_foreach (l, server->connections) {
          Connection *connection = (Connection*) l;

          voodoo_manager_destroy( connection->manager );

//          close( connection->fd );

          D_FREE( connection );
     }

//     if (server->shared)
//          munmap( server->shared, sizeof(ServerShared) );

     D_FREE( server );

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
accept_connection( VoodooServer *server, int fd )
{
     DirectResult     ret;
     Connection      *connection;
//     int              fds[2];

     connection = D_CALLOC( 1, sizeof(Connection) );
     if (!connection) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

//     connection->fd = fd;

//     fds[0] = connection->fd;
//     fds[1] = connection->fd;

     ret = voodoo_manager_create( &connection->vl, NULL, server, &connection->manager );
     if (ret) {
//          close( connection->fd );
          D_FREE( connection );
          return ret;
     }

     direct_list_prepend( &server->connections, &connection->link );

     return DR_OK;
}

