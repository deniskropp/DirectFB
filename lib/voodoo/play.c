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

#ifndef WIN32
#include <unistd.h>
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
#endif

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
#include <voodoo/play.h>
#include <voodoo/play_internal.h>


D_DEBUG_DOMAIN( Voodoo_Play, "Voodoo/Play", "Voodoo Play" );

/**********************************************************************************************************************/

typedef struct {
     DirectLink          link;

     VoodooPlayVersion   version;
     VoodooPlayInfo      info;

     long long           last_seen;

     char                addr[64];
} PlayerNode;

/**********************************************************************************************************************/

static void  player_send_info( VoodooPlayer    *player,
                               const in_addr_t *in_addr,
                               bool             discover );

static void *player_main_loop( DirectThread    *thread,
                               void            *arg );

/**********************************************************************************************************************/

static const int one = 1;

/**********************************************************************************************************************/

static VoodooPlayer *g_VoodooPlayer;

/**********************************************************************************************************************/

/*
 * FIXME
 */
static void
generate_uuid( u8 *buf )
{
     int i;

     srand( (unsigned int) direct_clock_get_abs_micros() );

     for (i=0; i<16; i++) {
          buf[i] = rand();
     }
}

/**********************************************************************************************************************/

#ifdef WIN32

#define close(x) do {} while (0)   // FIXME
extern int StartWinsock( void );

void __Voodoo_play_init()
{
     StartWinsock();
}
void __Voodoo_play_deinit( void )
{

}

#else

#define SOCKET int
#define INVALID_SOCKET -1

void __Voodoo_play_init()
{

}
void __Voodoo_play_deinit()
{

}

#endif


DirectResult
voodoo_player_create( const VoodooPlayInfo  *info,
                      VoodooPlayer         **ret_player )
{
     DirectResult        ret;
     SOCKET                 fd;
     struct sockaddr_in  addr;
     VoodooPlayer       *player;

     D_ASSERT( ret_player != NULL );

     if (g_VoodooPlayer) {
          *ret_player = g_VoodooPlayer;
          return DR_OK;
     }

     /* Create the player socket. */
     fd = socket( AF_INET, SOCK_DGRAM, 0 );
     if (fd == INVALID_SOCKET) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Player: Could not create the socket via socket()!\n" );
          return ret;
     }

#ifndef WIN32
     /* Allow reuse of local address. */
     if (setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Player: Could not set SO_REUSEADDR!\n" );

     if (setsockopt( fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Player: Could not set SO_BROADCAST!\n" );
#endif

     /* Bind the socket to the local port. */
     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = inet_addr( "0.0.0.0" );
     addr.sin_port        = htons( 2323 );

     if (bind( fd, (struct sockaddr*) &addr, sizeof(addr) )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Player: Could not bind() the socket!\n" );
          close( fd );
          return ret;
     }

     /* Allocate player structure. */
     player = D_CALLOC( 1, sizeof(VoodooPlayer) );
     if (!player) {
          D_WARN( "out of memory" );
          close( fd );
          return DR_NOLOCALMEMORY;
     }

     direct_recursive_mutex_init( &player->lock );

     /* Initialize player structure. */
     player->fd = fd;

     /* Fill version struct */
     player->version.v[0] = VPVF_LITTLE_ENDIAN | VPVF_32BIT_SERIALS;
     player->version.v[1] = DIRECTFB_MAJOR_VERSION;
     player->version.v[2] = DIRECTFB_MINOR_VERSION;
     player->version.v[3] = DIRECTFB_MICRO_VERSION;

     /* Fill info struct */
     direct_snputs( player->info.name,   voodoo_config->play_info.name,    VOODOO_PLAYER_NAME_LENGTH );
     direct_snputs( player->info.vendor, voodoo_config->play_info.vendor,  VOODOO_PLAYER_VENDOR_LENGTH );
     direct_snputs( player->info.model,  voodoo_config->play_info.model,   VOODOO_PLAYER_MODEL_LENGTH );
     direct_memcpy( player->info.uuid,   voodoo_config->play_info.uuid,    16 );

     if (info)
          player->info = *info;

     if (!player->info.name[0])
          direct_snputs( player->info.name, "Unnamed Player", VOODOO_PLAYER_NAME_LENGTH );

     if (!player->info.uuid[0])
          generate_uuid( player->info.uuid );

     player->info.flags |= VPIF_LINK;

     D_MAGIC_SET( player, VoodooPlayer );


     char buf[33];

     snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               player->info.uuid[0], player->info.uuid[1], player->info.uuid[2], player->info.uuid[3], player->info.uuid[4],
               player->info.uuid[5], player->info.uuid[6], player->info.uuid[7], player->info.uuid[8], player->info.uuid[9],
               player->info.uuid[10], player->info.uuid[11], player->info.uuid[12], player->info.uuid[13], player->info.uuid[14],
               player->info.uuid[15] );

     D_INFO( "Running player '%s' with UUID %s!\n", player->info.name, buf );

     /* Start messaging thread */
     player->thread = direct_thread_create( DTT_DEFAULT, player_main_loop, player, "Voodoo/Player" );

     /* Return the new player. */
     *ret_player = player;

     if (!g_VoodooPlayer)
          g_VoodooPlayer = player;

     return DR_OK;
}

DirectResult
voodoo_player_destroy( VoodooPlayer *player )
{
     D_MAGIC_ASSERT( player, VoodooPlayer );

     if (g_VoodooPlayer == player)
          return DR_OK;

     player->quit = true;

     direct_thread_join( player->thread );
     direct_thread_destroy( player->thread );

     close( player->fd );

     direct_mutex_deinit( &player->lock );

     D_MAGIC_CLEAR( player );

     D_FREE( player );

     return DR_OK;
}

DirectResult
voodoo_player_broadcast( VoodooPlayer *player )
{
     DirectResult ret;

     if (voodoo_config->play_broadcast) {
          in_addr_t addr = inet_addr( voodoo_config->play_broadcast );

          player_send_info( player, &addr, true );
     }
     else {
          VoodooPlayAddress *broadcasts;
          size_t             i, num;

          ret = voodoo_play_get_broadcast( &broadcasts, &num );
          if (ret) {
               D_DERROR( ret, "Voodoo/Play: Unable to retrieve list of broadcast addresses!\n" );
               return ret;
          }

          for (i=0; i<num; i++) {
               in_addr_t addr = VOODOO_PLAY_INET_ADDR( broadcasts[i] );
               
               player_send_info( player, &addr, true );
          }

          D_FREE( broadcasts );
     }

     return DR_OK;
}

DirectResult
voodoo_player_lookup( VoodooPlayer   *player,
                      const u8        uuid[16],
                      VoodooPlayInfo *ret_info,
                      char           *ret_addr,
                      int             max_addr )
{
     PlayerNode *node;

     D_MAGIC_ASSERT( player, VoodooPlayer );

     direct_mutex_lock( &player->lock );

     direct_list_foreach (node, player->nodes) {
          if (!uuid || !memcmp( node->info.uuid, uuid, 16 )) {
               if (ret_info)
                    direct_memcpy( ret_info, &node->info, sizeof(VoodooPlayInfo) );

               if (ret_addr)
                    direct_snputs( ret_addr, node->addr, max_addr );

               direct_mutex_unlock( &player->lock );
               return DR_OK;
          }
     }

     if (uuid && !memcmp( player->info.uuid, uuid, 16 )) {
          if (ret_info)
               direct_memcpy( ret_info, &player->info, sizeof(VoodooPlayInfo) );

          if (ret_addr)
               direct_snputs( ret_addr, "127.0.0.1", max_addr );

          direct_mutex_unlock( &player->lock );
          return DR_OK;
     }

     direct_mutex_unlock( &player->lock );

     return DR_ITEMNOTFOUND;
}

DirectResult
voodoo_player_lookup_by_address( VoodooPlayer   *player,
                                 const char     *addr,
                                 VoodooPlayInfo *ret_info )
{
     PlayerNode *node;

     D_MAGIC_ASSERT( player, VoodooPlayer );

     direct_mutex_lock( &player->lock );

     direct_list_foreach (node, player->nodes) {
          if (!addr || !strcmp( node->addr, addr )) {
               direct_memcpy( ret_info, &node->info, sizeof(VoodooPlayInfo) );

               direct_mutex_unlock( &player->lock );
               return DR_OK;
          }
     }

     if (addr && !strcmp( "127.0.0.1", addr )) {
          direct_memcpy( ret_info, &player->info, sizeof(VoodooPlayInfo) );

          direct_mutex_unlock( &player->lock );
          return DR_OK;
     }

     direct_mutex_unlock( &player->lock );

     return DR_ITEMNOTFOUND;
}

DirectResult
voodoo_player_enumerate( VoodooPlayer          *player,
                         VoodooPlayerCallback   callback,
                         void                  *ctx )
{
     PlayerNode *node;
     long long   now = direct_clock_get_abs_millis();


     D_MAGIC_ASSERT( player, VoodooPlayer );

     direct_mutex_lock( &player->lock );

     direct_list_foreach (node, player->nodes) {
          if (callback( ctx, &node->info, &node->version,
                        node->addr, (unsigned int) (now - node->last_seen) ) == DENUM_CANCEL)
               break;
     }

     direct_mutex_unlock( &player->lock );

     return DR_OK;
}

/**********************************************************************************************************************/

static void
player_send_info( VoodooPlayer    *player,
                  const in_addr_t *in_addr,
                  bool             discover )
{
     int                 ret;
     struct sockaddr_in  addr;
     VoodooPlayMessage   msg;
     PlayerNode         *node;
     char                buf[100];

     D_MAGIC_ASSERT( player, VoodooPlayer );

     msg.version = player->version;
     msg.type    = discover ? VPMT_DISCOVER : VPMT_SENDINFO;
     msg.info    = player->info;

     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = *in_addr;
     addr.sin_port        = htons( 2323 );


     direct_inet_ntop( AF_INET, &addr.sin_addr, buf, sizeof(buf) );

     D_INFO( "Voodoo/Player: Sending %s to %s\n", discover ? "DISCOVER" : "SENDINFO", buf );


     ret = sendto( player->fd, (const char*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr) );
     if (ret < 0) {
          D_PERROR( "Voodoo/Player: sendto() failed!\n" );
          return;
     }

     if (!discover && voodoo_config->forward_nodes) {
          direct_list_foreach (node, player->nodes) {
               VoodooPlayInfo info = node->info;
     
               info.flags |= VPIF_LEVEL2;
     
               msg.version = node->version;
               msg.type    = discover ? VPMT_DISCOVER : VPMT_SENDINFO;
               msg.info    = info;
     
               ret = sendto( player->fd, (const char*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr) );
               if (ret < 0) {
                    D_PERROR( "Voodoo/Player: sendto() failed!\n" );
                    return;
               }
          }
     }
}

static void
player_save_info( VoodooPlayer            *player,
                  const VoodooPlayMessage *msg,
                  const char              *addr )
{
     PlayerNode *node;

     D_MAGIC_ASSERT( player, VoodooPlayer );

     direct_list_foreach (node, player->nodes) {
          if (!memcmp( node->info.uuid, msg->info.uuid, 16 )) {
               if (msg->info.flags & VPIF_LEVEL2 && !(node->info.flags & VPIF_LEVEL2)) {
                    node->version = msg->version;
                    node->info    = msg->info;

                    direct_snputs( node->addr, addr, sizeof(node->addr) );
               }

               node->last_seen = direct_clock_get_abs_millis();

               return;
          }
     }

     node = D_CALLOC( 1, sizeof(PlayerNode) );
     if (!node) {
          D_OOM();
          return;
     }

     node->version   = msg->version;
     node->info      = msg->info;

     node->last_seen = direct_clock_get_abs_millis();

     direct_snputs( node->addr, addr, sizeof(node->addr) );


     direct_list_append( &player->nodes, &node->link );
}

static void *
player_main_loop( DirectThread *thread, void *arg )
{
     VoodooPlayer       *player = arg;
     int                 ret;
     struct sockaddr_in  addr;
     socklen_t           addr_len = sizeof(addr);
     VoodooPlayMessage   msg;
     char                buf[100];

     D_MAGIC_ASSERT( player, VoodooPlayer );

     while (!player->quit) {
          fd_set         rfds;
          struct timeval tv;
          int            retval;

          FD_ZERO( &rfds );
          FD_SET( player->fd, &rfds );

          tv.tv_sec  = 0;
          tv.tv_usec = 100000;

          retval = select( player->fd+1, &rfds, NULL, NULL, &tv );

          switch (retval) {
               default:
                    ret = recvfrom( player->fd, (char*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, &addr_len );
                    if (ret < 0) {
                         D_PERROR( "Voodoo/Player: recvfrom() failed!\n" );
                         direct_thread_sleep( 500000 );
                         continue;
                    }

                    direct_inet_ntop( AF_INET, &addr.sin_addr, buf, sizeof(buf) );

                    direct_mutex_lock( &player->lock );

                    /* Send reply if message is not from ourself */
                    if (memcmp( msg.info.uuid, player->info.uuid, 16 )) {
                         switch (msg.type) {
                              case VPMT_DISCOVER:
                                   D_INFO( "Voodoo/Player: Received DISCOVER from '%s' (%-15s) %s "
                                           "=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x= "
                                           "(vendor: %s, model: %s)\n",
                                           msg.info.name, buf,
                                           (msg.info.flags & VPIF_LEVEL2) ? "*" : " ",
                                           msg.info.uuid[0], msg.info.uuid[1], msg.info.uuid[2], msg.info.uuid[3], msg.info.uuid[4],
                                           msg.info.uuid[5], msg.info.uuid[6], msg.info.uuid[7], msg.info.uuid[8], msg.info.uuid[9],
                                           msg.info.uuid[10], msg.info.uuid[11], msg.info.uuid[12], msg.info.uuid[13], msg.info.uuid[14],
                                           msg.info.uuid[15],
                                           msg.info.vendor, msg.info.model );

                                   player_send_info( player, &addr.sin_addr.s_addr, false );
                                   break;

                              case VPMT_SENDINFO:
                                   D_INFO( "Voodoo/Player: Received SENDINFO from '%s' (%-15s) %s "
                                           "=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x= "
                                           "(vendor: %s, model: %s)\n",
                                           msg.info.name, buf,
                                           (msg.info.flags & VPIF_LEVEL2) ? "*" : " ",
                                           msg.info.uuid[0], msg.info.uuid[1], msg.info.uuid[2], msg.info.uuid[3], msg.info.uuid[4],
                                           msg.info.uuid[5], msg.info.uuid[6], msg.info.uuid[7], msg.info.uuid[8], msg.info.uuid[9],
                                           msg.info.uuid[10], msg.info.uuid[11], msg.info.uuid[12], msg.info.uuid[13], msg.info.uuid[14],
                                           msg.info.uuid[15],
                                           msg.info.vendor, msg.info.model );

                                   player_save_info( player, &msg, buf );
                                   break;

                              default:
                                   D_ERROR( "Voodoo/Player: Received unknown message (%s)\n", buf );
                                   break;
                         }
                    }
                    else
                         D_INFO( "Voodoo/Player: Received message from ourself (%s)\n", buf );

                    direct_mutex_unlock( &player->lock );
                    break;

               case 0:
                    D_DEBUG( "Voodoo/Player: Timeout during select()\n" );
                    break;

               case -1:
                    if (errno && errno != EINTR) {
                         D_PERROR( "Voodoo/Player: select() on socket failed!\n" );
                         player->quit = true;
                    }
                    break;
          }
     }

     return NULL;
}

