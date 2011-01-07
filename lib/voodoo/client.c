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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/play.h>

/**********************************************************************************************************************/

struct __V_VoodooClient {
     DirectLink     link;

     int            refs;

     int            fd;
     VoodooManager *manager;

     char          *host;
     int            port;
};

static DirectLink *m_clients;

/**********************************************************************************************************************/

DirectResult
voodoo_client_create( const char     *host,
                      int             port,
                      VoodooClient  **ret_client )
{
     DirectResult     ret;
     int              err, fd;
     struct addrinfo  hints;
     struct addrinfo *addr;
     VoodooClient    *client;
     int              fds[2];
     char             buf[100] = { 0 };
     const char      *hostname = host;
     char             portstr[10];

     D_ASSERT( ret_client != NULL );

     if (!port)
          port = 2323;

     direct_list_foreach (client, m_clients) {
          if (!strcmp( client->host, host ) && client->port == port) {
               D_INFO( "Voodoo/Client: Reconnecting to '%s', increasing ref count of existing connection!\n", host );

               client->refs++;

               *ret_client = client;

               return DR_OK;
          }
     }

     if (!hostname || !hostname[0]) {
          int           n;
          VoodooPlayer *player;

          ret = voodoo_player_create( NULL, &player );
          if (ret) {
               D_DERROR( ret, "Voodoo/Proxy: Could not create the player!\n" );
               return ret;
          }

          for (n=0; n<10; n++) {
               voodoo_player_broadcast( player );

               usleep( 20000 );

               if (voodoo_player_lookup( player, NULL, buf, sizeof(buf) ) == DR_OK)
                    break;

               usleep( 100000 );
          }

          voodoo_player_destroy( player );

          if (!buf[0]) {
               D_ERROR( "Voodoo/Play: Did not find any other player!\n" );
               return DR_ITEMNOTFOUND;
          }

          hostname = buf;
     }

     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = PF_UNSPEC;

     D_INFO( "Voodoo/Client: Looking up host '%s'...\n", hostname );

     snprintf( portstr, sizeof(portstr), "%d", port );

     err = getaddrinfo( hostname, portstr, &hints, &addr );
     if (err) {
          switch (err) {
               case EAI_FAMILY:
                    D_ERROR( "Direct/Log: Unsupported address family!\n" );
                    return DR_UNSUPPORTED;

               case EAI_SOCKTYPE:
                    D_ERROR( "Direct/Log: Unsupported socket type!\n" );
                    return DR_UNSUPPORTED;

               case EAI_NONAME:
                    D_ERROR( "Direct/Log: Host not found!\n" );
                    return DR_FAILURE;

               case EAI_SERVICE:
                    D_ERROR( "Direct/Log: Service is unreachable!\n" );
                    return DR_FAILURE;

#ifdef EAI_ADDRFAMILY
               case EAI_ADDRFAMILY:
#endif
               case EAI_NODATA:
                    D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                    return DR_FAILURE;

               case EAI_MEMORY:
                    return D_OOM();

               case EAI_FAIL:
                    D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                    return DR_FAILURE;

               case EAI_AGAIN:
                    D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                    return DR_TEMPUNAVAIL;

               default:
                    D_ERROR( "Direct/Log: Unknown error occured!?\n" );
                    return DR_FAILURE;
          }
     }

     /* Create the client socket. */
     fd = socket( addr->ai_family, SOCK_STREAM, 0 );
     if (fd < 0) {
          D_PERROR( "Voodoo/Client: Could not create the socket via socket()!\n" );
          freeaddrinfo( addr );
          return errno2result( errno );
     }

     D_INFO( "Voodoo/Client: Connecting to '%s:%d'...\n", addr->ai_canonname, port );

     /* Connect to the server. */
     err = connect( fd, addr->ai_addr, addr->ai_addrlen );
     freeaddrinfo( addr );

     if (err) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Client: Could not connect() to the server!\n" );
          close( fd );
          return ret;
     }

     /* Allocate client structure. */
     client = D_CALLOC( 1, sizeof(VoodooClient) );
     if (!client) {
          D_WARN( "out of memory" );
          close( fd );
          return DR_NOLOCALMEMORY;
     }

     /* Initialize client structure. */
     client->fd = fd;

     fds[0] = fd;
     fds[1] = fd;

     /* Create the manager. */
     ret = voodoo_manager_create( fds, client, NULL, &client->manager );
     if (ret) {
          D_FREE( client );
          close( fd );
          return ret;
     }

     client->refs = 1;
     client->host = D_STRDUP( host );
     client->port = port;

     direct_list_prepend( &m_clients, &client->link );

     /* Return the new client. */
     *ret_client = client;

     return DR_OK;
}

DirectResult
voodoo_client_destroy( VoodooClient *client )
{
     D_ASSERT( client != NULL );

     D_INFO( "Voodoo/Client: Decreasing ref count of connection...\n" );

     if (! --(client->refs)) {
     voodoo_manager_destroy( client->manager );

          D_INFO("closing socket\n");
     close( client->fd );

          direct_list_remove( &m_clients, &client->link );

          D_FREE( client->host );
     D_FREE( client );
     }

     return DR_OK;
}

VoodooManager *
voodoo_client_manager( const VoodooClient *client )
{
     D_ASSERT( client != NULL );

     return client->manager;
}

