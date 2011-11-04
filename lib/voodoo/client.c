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

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/conf.h>
#include <voodoo/internal.h>
#include <voodoo/link.h>
#include <voodoo/manager.h>
#include <voodoo/play.h>


D_DEBUG_DOMAIN( Voodoo_Client, "Voodoo/Client", "Voodoo Client" );

/**********************************************************************************************************************/

struct __V_VoodooClient {
     DirectLink     link;

     int            refs;

     VoodooLink     vl;
     VoodooManager *manager;

     char          *host;
     int            port;
};

static DirectLink *m_clients; // FIXME: add lock

/**********************************************************************************************************************/

static DirectResult
send_discover_and_receive_info( VoodooLink        *link,
                                VoodooPlayVersion *ret_version,
                                VoodooPlayInfo    *ret_info )
{
     int                 ret;
     VoodooMessageHeader header;

     D_INFO( "Voodoo/Player: Sending VMSG_DISCOVER message via Voodoo TCP port...\n" );

     header.size   = sizeof(VoodooMessageHeader);
     header.serial = 0;
     header.type   = VMSG_DISCOVER;

     ret = link->Write( link, &header, sizeof(header) );
     if (ret < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Player: Failed to send VMSG_DISCOVER message via Voodoo TCP port!\n" );
          return ret;
     }


     // wait for up to one second (old server will not reply anything, so we have to timeout)
     ret = link->WaitForData( link, 1000 );
     if (ret) {
          D_DERROR( ret, "Voodoo/Player: Failed to wait for reply after sending VMSG_DISCOVER message via Voodoo TCP port!\n" );
          return ret;
     }

     D_INFO( "Voodoo/Player: New Voodoo Server with VMSG_DISCOVER support, reading version/info (SENDINFO) reply...\n" );


     struct {
          VoodooMessageHeader header;
          VoodooPlayVersion   version;
          VoodooPlayInfo      info;
     } msg;

     size_t got = 0;

     while (got < sizeof(msg)) {
          ret = link->Read( link, (void*) &msg + got, sizeof(msg) - got );
          if (ret < 0) {
               ret = errno2result( errno );
               D_PERROR( "Voodoo/Player: Failed to read after sending VMSG_DISCOVER message via Voodoo TCP port!\n" );
               return ret;
          }

          got += ret;
     }


     if (msg.header.type != VMSG_SENDINFO) {
          D_ERROR( "Voodoo/Player: Received message after sending VMSG_DISCOVER message via Voodoo TCP port is no VMSG_SENDINFO!\n");
          return DR_INVARG;
     }

     *ret_version = msg.version;
     *ret_info    = msg.info;

     D_INFO( "Voodoo/Player: Voodoo Server sent name '%s', version %d.%d.%d\n",
             msg.info.name, msg.version.v[1], msg.version.v[2], msg.version.v[3] );

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
voodoo_client_create( const char     *host,
                      int             port,
                      VoodooClient  **ret_client )
{
     DirectResult    ret;
     VoodooPlayInfo  info;
     VoodooClient   *client;
     VoodooPlayer   *player;
     char            buf[100] = { 0 };
     const char     *hostname = host;
     bool            raw = true;

     D_ASSERT( ret_client != NULL );

     if (!host)
          host = "";

     if (!port)
          port = 2323;

     D_DEBUG_AT( Voodoo_Client, "%s( '%s', %d )\n", __FUNCTION__, host, port );

     if (port != 2323) {
          D_DEBUG_AT( Voodoo_Client, "  -> port != 2323, using PACKET mode right away\n" );

          raw = false;
     }

     direct_list_foreach (client, m_clients) {
          if (!strcmp( client->host, host ) && client->port == port) {
               D_INFO( "Voodoo/Client: Reconnecting to '%s', increasing ref count of existing connection!\n", host );

               client->refs++;

               *ret_client = client;

               return DR_OK;
          }
     }


     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_DERROR( ret, "Voodoo/Client: Could not create the player!\n" );
          return ret;
     }

     // FIXME: resolve first, not late in voodoo_link_init_connect
     if (hostname && hostname[0]) {
          ret = voodoo_player_lookup_by_address( player, hostname, &info );
          if (ret == DR_OK) {
               if (info.flags & VPIF_PACKET)
                    raw = false;
          }
     }
     else {
          ret = voodoo_player_lookup( player, NULL, &info, buf, sizeof(buf) );
          if (ret == DR_OK) {
               if (info.flags & VPIF_PACKET)
                    raw = false;

               hostname = buf;
          }
     }

     voodoo_player_destroy( player );

     if (!hostname || !hostname[0]) {
          D_ERROR( "Voodoo/Client: Did not find any other player!\n" );
          return DR_ITEMNOTFOUND;
     }


     /* Allocate client structure. */
     client = D_CALLOC( 1, sizeof(VoodooClient) );
     if (!client)
          return D_OOM();


     raw = !voodoo_config->link_packet && (voodoo_config->link_raw || raw);

     /* Create a link to the other player. */
     ret = voodoo_link_init_connect( &client->vl, hostname, port, raw );
     if (ret) {
          D_DERROR( ret, "Voodoo/Client: Failed to initialize Voodoo Link!\n" );
          D_FREE( client );
          return ret;
     }

     D_INFO( "Voodoo/Client: Fetching player information...\n" );

     if (raw) {     // FIXME: send_discover_and_receive_info() only does RAW, but we don't need it for packet connection, yet
          VoodooPlayVersion server_version;
          VoodooPlayInfo    server_info;

          ret = send_discover_and_receive_info( &client->vl, &server_version, &server_info );
          if (ret) {
               D_DEBUG_AT( Voodoo_Client, "  -> Failed to receive player info via TCP!\n" );

               D_INFO( "Voodoo/Client: No player information from '%s'!\n", host );
          }
          else {
               D_INFO( "Voodoo/Client: Connected to '%s' (%-15s) %s "
                       "=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x= "
                       "(vendor: %s, model: %s)\n",
                       server_info.name, host,
                       (server_info.flags & VPIF_LEVEL2) ? "*" : " ",
                       server_info.uuid[0], server_info.uuid[1], server_info.uuid[2], server_info.uuid[3], server_info.uuid[4],
                       server_info.uuid[5], server_info.uuid[6], server_info.uuid[7], server_info.uuid[8], server_info.uuid[9],
                       server_info.uuid[10], server_info.uuid[11], server_info.uuid[12], server_info.uuid[13], server_info.uuid[14],
                       server_info.uuid[15],
                       server_info.vendor, server_info.model );

               if (raw && !voodoo_config->link_raw) {
                    /*
                     * Switch to packet mode?
                     */
                    if (server_info.flags & VPIF_PACKET) {
                         D_INFO( "Voodoo/Client: Switching to packet mode!\n" );

                         client->vl.Close( &client->vl );

                         /* Create another link to the other player. */
                         ret = voodoo_link_init_connect( &client->vl, hostname, port, false );
                         if (ret) {
                              D_DERROR( ret, "Voodoo/Client: Failed to initialize second Voodoo Link!\n" );
                              D_FREE( client );
                              return ret;
                         }
                    }
               }
          }
     }


     /* Create the manager. */
     ret = voodoo_manager_create( &client->vl, client, NULL, &client->manager );
     if (ret) {
          client->vl.Close( &client->vl );
          D_FREE( client );
          return ret;
     }

     client->refs = 1;
     client->host = D_STRDUP( host );
     client->port = port;

     direct_list_prepend( &m_clients, &client->link );

     /* Return the new client. */
     *ret_client = client;

     D_DEBUG_AT( Voodoo_Client, "  => client %p\n", client );

     return DR_OK;
}

DirectResult
voodoo_client_destroy( VoodooClient *client )
{
     D_ASSERT( client != NULL );

     D_DEBUG_AT( Voodoo_Client, "%s( %p )\n", __FUNCTION__, client );

     D_INFO( "Voodoo/Client: Decreasing ref count of connection...\n" );

     if (! --(client->refs)) {
          voodoo_manager_destroy( client->manager );

          //client->vl.Close( &client->vl );

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

