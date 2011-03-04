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

DirectResult
voodoo_client_create( const char     *host,
                      int             port,
                      VoodooClient  **ret_client )
{
     DirectResult    ret;
     VoodooClient   *client;
     VoodooPlayer   *player;
     int             bc_num  = 10;
     int             bc_wait = 4000;
     char            buf[100] = { 0 };
     const char     *hostname = host;
     bool            raw = true;

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


     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_DERROR( ret, "Voodoo/Client: Could not create the player!\n" );
          return ret;
     }

     while (bc_num--) {
          VoodooPlayInfo info;

          // FIXME: resolve first, not late in voodoo_link_init_connect
          if (hostname && hostname[0]) {
               ret = voodoo_player_lookup_by_address( player, hostname, &info );
               if (ret == DR_OK) {
                    if (info.flags & VPIF_LINK)
                         raw = false;
                    
                    break;
               }
          }
          else {
               ret = voodoo_player_lookup( player, NULL, &info, buf, sizeof(buf) );
               if (ret == DR_OK) {
                    if (info.flags & VPIF_LINK)
                         raw = false;

                    hostname = buf;

                    break;
               }
          }

          voodoo_player_broadcast( player );

          direct_thread_sleep( bc_wait );

          bc_wait += bc_wait;
     }

     voodoo_player_destroy( player );

     if (!hostname || !hostname[0]) {
          D_ERROR( "Voodoo/Play: Did not find any other player!\n" );
          return DR_ITEMNOTFOUND;
     }


     /* Allocate client structure. */
     client = D_CALLOC( 1, sizeof(VoodooClient) );
     if (!client)
          return D_OOM();


     /* Initialize client structure. */
     ret = voodoo_link_init_connect( &client->vl, hostname, port, voodoo_config->link_raw || raw );
     if (ret) {
          D_DERROR( ret, "Voodoo/Client: Failed to initialize Voodoo Link!\n" );
          D_FREE( client );
          return ret;
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

     return DR_OK;
}

DirectResult
voodoo_client_destroy( VoodooClient *client )
{
     D_ASSERT( client != NULL );

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

