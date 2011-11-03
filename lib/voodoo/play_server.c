/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <directfb_version.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/internal.h>
#include <voodoo/play_server.h>
#include <voodoo/play_internal.h>
#include <voodoo/server.h>

D_DEBUG_DOMAIN( Voodoo_Play_Server, "Voodoo/Play/Server", "Voodoo Play Server" );

/**********************************************************************************************************************/

VoodooPlayer *voodoo_player;

/**********************************************************************************************************************/
/*
 * FIXME
 */
static void
generate_uuid( u8 *buf )
{
     int i;

     for (i=0; i<16; i++) {
          buf[i] = rand();
     }
}

static DirectResult
ConstructDispatcher( VoodooServer     *server,
                     VoodooManager    *manager,
                     const char       *name,
                     void             *ctx,
                     VoodooInstanceID *ret_instance )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *interface;
     VoodooInstanceID      instance;

     D_ASSERT( server != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     ret = DirectGetInterface( &funcs, name, "Dispatcher", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface );
     if (ret)
          return ret;

     ret = funcs->Construct( interface, manager, &instance );
     if (ret)
          return ret;

     *ret_instance = instance;

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
voodoo_player_run_server( VoodooPlayer               *player,
                          const VoodooAppDescription *apps,
                          unsigned int                num_apps,
                          VoodooPlayerLaunchFunc      launch_func,
                          VoodooPlayerStopFunc        stop_func,
                          void                       *ctx )
{
     DirectResult  ret;
     VoodooServer *server;

     D_ASSERT( player != NULL );
     D_ASSERT( apps != NULL );
     D_ASSERT( num_apps > 0 );
     D_ASSERT( launch_func != NULL );

     if (voodoo_player) {
          D_ERROR( "Voodoo/Play: Already running as a server!\n" );
          return DR_BUSY;
     }

     ret = voodoo_server_create( NULL, 0, false, &server );
     if (ret)
          return ret;

     ret = voodoo_server_register( server, "IVoodooPlayer", ConstructDispatcher, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Player: Could not register super interface 'IVoodooPlayer'!\n" );
          voodoo_server_destroy( server );
          return ret;
     }

     player->server      = server;
     player->apps        = apps;
     player->num_apps    = num_apps;
     player->launch_func = launch_func;
     player->stop_func   = stop_func;
     player->ctx         = ctx;

     direct_mutex_init( &player->instances_lock );
     direct_waitqueue_init( &player->instances_cond );

     voodoo_player = player;

     ret = voodoo_server_run( server );
     if (ret)
          D_DERROR( ret, "Voodoo/Player: Server exiting!\n" );

     voodoo_player = NULL;

     player->server      = NULL;
     player->apps        = NULL;
     player->num_apps    = 0;
     player->launch_func = NULL;
     player->stop_func   = NULL;
     player->ctx         = NULL;
     player->instances   = NULL;

     direct_mutex_deinit( &player->instances_lock );
     direct_waitqueue_deinit( &player->instances_cond );

     voodoo_server_destroy( server );

     return ret;
}

DirectResult
voodoo_player_get_apps( VoodooPlayer         *player,
                        unsigned int          max_num,
                        unsigned int         *ret_num,
                        VoodooAppDescription *ret_apps )
{
     D_ASSERT( player != NULL );
     D_ASSERT( ret_num != NULL );
     D_ASSERT( ret_apps != NULL );

     ///

     unsigned int num = max_num;

     if (num > player->num_apps)
          num = player->num_apps;

     *ret_num = num;

     direct_memcpy( ret_apps, player->apps, sizeof(VoodooAppDescription) * num );

     return DR_OK;
}

DirectResult
voodoo_player_launch_app( VoodooPlayer *player,
                          const u8      app_uuid[16],
                          const u8      player_uuid[16],
                          u8            ret_instance_uuid[16] )
{
     DirectResult ret;
     int          i;

     D_ASSERT( player != NULL );
     D_ASSERT( app_uuid != NULL );
     D_ASSERT( player_uuid != NULL );
     D_ASSERT( ret_instance_uuid != NULL );


     char buf1[33];
     char buf2[33];

     snprintf( buf1, sizeof(buf1), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               app_uuid[0], app_uuid[1], app_uuid[2], app_uuid[3], app_uuid[4],
               app_uuid[5], app_uuid[6], app_uuid[7], app_uuid[8], app_uuid[9],
               app_uuid[10], app_uuid[11], app_uuid[12], app_uuid[13], app_uuid[14],
               app_uuid[15] );

     snprintf( buf2, sizeof(buf2), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               player_uuid[0], player_uuid[1], player_uuid[2], player_uuid[3], player_uuid[4],
               player_uuid[5], player_uuid[6], player_uuid[7], player_uuid[8], player_uuid[9],
               player_uuid[10], player_uuid[11], player_uuid[12], player_uuid[13], player_uuid[14],
               player_uuid[15] );

     D_INFO( "Voodoo/Player: Launching application %s on player %s...\n", buf1, buf2 );


     const VoodooAppDescription *app = NULL;

     for (i=0; i<player->num_apps; i++) {
          if (!memcmp( app_uuid, player->apps[i].uuid, 16 )) {
               app = &player->apps[i];
               break;
          }
     }

     if (!app) {
          D_ERROR( "Voodoo/Player: Could not lookup application with UUID %s!\n", buf1 );
          return DR_ITEMNOTFOUND;
     }

     D_INFO( "Voodoo/Player: Found application '%s'\n\n", buf1, buf2 );


     VoodooPlayInfo info;
     char           addr[1000];

     ret = voodoo_player_lookup( player, player_uuid, &info, addr, sizeof(addr) );
     if (ret) {
          D_DERROR( ret, "Voodoo/Player: Could not lookup player with UUID %s!\n", buf2 );
          return ret;
     }


     VoodooAppInstance *instance;

     instance = D_CALLOC( 1, sizeof(VoodooAppInstance) );
     if (!instance)
          return D_OOM();

     ret = player->launch_func( player, player->ctx, app, &info, addr, &instance->data );
     if (ret) {
          D_DERROR( ret, "Voodoo/Player: Could not launch application '%s'\n", app->name );
          D_FREE( instance );
          return ret;
     }

     generate_uuid( instance->uuid );

     direct_memcpy( &instance->app, app, sizeof(VoodooAppDescription) );
     direct_memcpy( instance->player_uuid, player_uuid, 16 );
     


     direct_mutex_lock( &player->instances_lock );

     direct_list_append( &player->instances, &instance->link );

     direct_memcpy( ret_instance_uuid, instance->uuid, 16 );

     direct_mutex_unlock( &player->instances_lock );


     return DR_OK;
}

DirectResult
voodoo_player_stop_instance( VoodooPlayer *player,
                             const u8      instance_uuid[16] )
{
     DirectResult       ret;
     char               buf1[33];
     VoodooAppInstance *instance;

     snprintf( buf1, sizeof(buf1), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               instance_uuid[0], instance_uuid[1], instance_uuid[2], instance_uuid[3], instance_uuid[4],
               instance_uuid[5], instance_uuid[6], instance_uuid[7], instance_uuid[8], instance_uuid[9],
               instance_uuid[10], instance_uuid[11], instance_uuid[12], instance_uuid[13], instance_uuid[14],
               instance_uuid[15] );

     D_INFO( "Voodoo/Player: Stopping instance %s...\n", buf1 );

     direct_mutex_lock( &player->instances_lock );

     direct_list_foreach (instance, player->instances) {
          if (!memcmp( instance->uuid, instance_uuid, 16 ))
               break;
     }

     if (!instance) {
          D_ERROR( "Voodoo/Player: Could not find instance with UUID %s!\n", buf1 );
          direct_mutex_unlock( &player->instances_lock );
          return DR_NOSUCHINSTANCE;
     }

     ret = player->stop_func( player, player->ctx, instance->data );
     if (ret) {
          D_DERROR( ret, "Voodoo/Player: Could not stop instance with UUID %s!\n", buf1 );
          direct_mutex_unlock( &player->instances_lock );
          return ret;
     }

     direct_list_remove( &player->instances, &instance->link );

     direct_waitqueue_broadcast( &player->instances_cond );

     direct_mutex_unlock( &player->instances_lock );

     D_FREE( instance );

     return DR_OK;
}

DirectResult
voodoo_player_wait_instance( VoodooPlayer *player,
                             const u8      instance_uuid[16] )
{
     char               buf1[33];
     VoodooAppInstance *instance;

     snprintf( buf1, sizeof(buf1), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               instance_uuid[0], instance_uuid[1], instance_uuid[2], instance_uuid[3], instance_uuid[4],
               instance_uuid[5], instance_uuid[6], instance_uuid[7], instance_uuid[8], instance_uuid[9],
               instance_uuid[10], instance_uuid[11], instance_uuid[12], instance_uuid[13], instance_uuid[14],
               instance_uuid[15] );

     D_INFO( "Voodoo/Player: Waiting for instance %s...\n", buf1 );


     do {
          direct_mutex_lock( &player->instances_lock );

          direct_list_foreach (instance, player->instances) {
               if (!memcmp( instance->uuid, instance_uuid, 16 )) {
                    direct_waitqueue_wait( &player->instances_cond, &player->instances_lock );
                    break;
               }
          }

          direct_mutex_unlock( &player->instances_lock );
     } while (instance);

     return DR_OK;
}

DirectResult
voodoo_player_get_instances( VoodooPlayer                 *player,
                             unsigned int                  max_num,
                             unsigned int                 *ret_num,
                             VoodooAppInstanceDescription *ret_instances )
{
     VoodooAppInstance *instance;

     D_ASSERT( player != NULL );
     D_ASSERT( ret_num != NULL );
     D_ASSERT( ret_instances != NULL );

     ///

     direct_mutex_lock( &player->instances_lock );

     unsigned int i = 0;

     direct_list_foreach (instance, player->instances) {
          if (i == max_num)
               break;

          direct_memcpy( ret_instances[i].uuid,        instance->uuid,        16 );
          direct_memcpy( &ret_instances[i].app,        &instance->app,        sizeof(VoodooAppDescription) );
          direct_memcpy( ret_instances[i].player_uuid, instance->player_uuid, 16 );

          i++;
     }

     *ret_num = i;

     direct_mutex_unlock( &player->instances_lock );

     return DR_OK;
}

