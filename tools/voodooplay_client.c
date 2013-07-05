/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/play.h>
#include <voodoo/ivoodooplayer.h>

static const char *m_name;
static bool        m_launch_app;
static u8          m_launch_app_uuid[16];
static u8          m_launch_player_uuid[16];
static bool        m_stop_instance;
static u8          m_stop_instance_uuid[16];
static bool        m_wait_instance;
static u8          m_wait_instance_uuid[16];

/**********************************************************************************************************************/

static DFBBoolean parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

static DirectEnumerationResult
player_callback( void                    *ctx,
                 const VoodooPlayInfo    *info,
                 const VoodooPlayVersion *version,
                 const char              *address,
                 unsigned int             ms_since_last_seen )
{
     DirectResult ret;
     int          i;
     char         buf[33];
     char         buf2[33];
     char         buf3[33];

     snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               info->uuid[0], info->uuid[1], info->uuid[2], info->uuid[3], info->uuid[4],
               info->uuid[5], info->uuid[6], info->uuid[7], info->uuid[8], info->uuid[9],
               info->uuid[10], info->uuid[11], info->uuid[12], info->uuid[13], info->uuid[14],
               info->uuid[15] );

     D_INFO( "Voodoo/Play: <%4ums> [ %-30s ] { %s }  %s%s\n",
             ms_since_last_seen, info->name, buf, address, (info->flags & VPIF_LEVEL2) ? " *" : "" );

     IVoodooPlayer *player;

     ret = VoodooPlayerCreate( address, 0, &player );
     if (ret) {
          D_DERROR( ret, "Voodoo/Play/Client: VoodooPlayerCreate() failed!\n" );
          return DENUM_OK;
     }



     VoodooAppDescription apps[100];
     unsigned int         num;

     ret = player->GetApps( player, 100, &num, apps );
     if (ret) {
          D_DERROR( ret, "Voodoo/Play/Client: IVoodooPlayer::GetApps() failed!\n" );
          player->Release( player );
          return DENUM_OK;
     }

     for (i=0; i<num; i++) {
          snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    apps[i].uuid[0], apps[i].uuid[1], apps[i].uuid[2], apps[i].uuid[3], apps[i].uuid[4],
                    apps[i].uuid[5], apps[i].uuid[6], apps[i].uuid[7], apps[i].uuid[8], apps[i].uuid[9],
                    apps[i].uuid[10], apps[i].uuid[11], apps[i].uuid[12], apps[i].uuid[13], apps[i].uuid[14],
                    apps[i].uuid[15] );

          D_INFO( "            [%2d] { %s }  %s\n", i, buf, apps[i].name );

          if (m_launch_app && !memcmp( m_launch_app_uuid, apps[i].uuid, 16 )) {
               u8 instance_uuid[16];

               snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                         m_launch_player_uuid[0], m_launch_player_uuid[1], m_launch_player_uuid[2], m_launch_player_uuid[3], m_launch_player_uuid[4],
                         m_launch_player_uuid[5], m_launch_player_uuid[6], m_launch_player_uuid[7], m_launch_player_uuid[8], m_launch_player_uuid[9],
                         m_launch_player_uuid[10], m_launch_player_uuid[11], m_launch_player_uuid[12], m_launch_player_uuid[13], m_launch_player_uuid[14],
                         m_launch_player_uuid[15] );

               D_INFO( "               -> launching on %s!\n", buf );

               ret = player->LaunchApp( player, m_launch_app_uuid, m_launch_player_uuid, instance_uuid );
               if (ret)
                    D_DERROR( ret, "Voodoo/Play/Client: IVoodooPlayer::LaunchApp() failed!\n" );

               snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                         instance_uuid[0], instance_uuid[1], instance_uuid[2], instance_uuid[3], instance_uuid[4],
                         instance_uuid[5], instance_uuid[6], instance_uuid[7], instance_uuid[8], instance_uuid[9],
                         instance_uuid[10], instance_uuid[11], instance_uuid[12], instance_uuid[13], instance_uuid[14],
                         instance_uuid[15] );

               D_INFO( "               => instance UUID is %s!\n", buf );
          }
     }



     VoodooAppInstanceDescription instances[100];

     ret = player->GetInstances( player, 100, &num, instances );
     if (ret) {
          D_DERROR( ret, "Voodoo/Play/Client: IVoodooPlayer::GetInstances() failed!\n" );
          player->Release( player );
          return DENUM_OK;
     }

     for (i=0; i<num; i++) {
          snprintf( buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    instances[i].uuid[0], instances[i].uuid[1], instances[i].uuid[2], instances[i].uuid[3], instances[i].uuid[4],
                    instances[i].uuid[5], instances[i].uuid[6], instances[i].uuid[7], instances[i].uuid[8], instances[i].uuid[9],
                    instances[i].uuid[10], instances[i].uuid[11], instances[i].uuid[12], instances[i].uuid[13], instances[i].uuid[14],
                    instances[i].uuid[15] );

          snprintf( buf2, sizeof(buf2), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    instances[i].app.uuid[0], instances[i].app.uuid[1], instances[i].app.uuid[2], instances[i].app.uuid[3], instances[i].app.uuid[4],
                    instances[i].app.uuid[5], instances[i].app.uuid[6], instances[i].app.uuid[7], instances[i].app.uuid[8], instances[i].app.uuid[9],
                    instances[i].app.uuid[10], instances[i].app.uuid[11], instances[i].app.uuid[12], instances[i].app.uuid[13], instances[i].app.uuid[14],
                    instances[i].app.uuid[15] );

          snprintf( buf3, sizeof(buf3), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    instances[i].player_uuid[0], instances[i].player_uuid[1], instances[i].player_uuid[2], instances[i].player_uuid[3], instances[i].player_uuid[4],
                    instances[i].player_uuid[5], instances[i].player_uuid[6], instances[i].player_uuid[7], instances[i].player_uuid[8], instances[i].player_uuid[9],
                    instances[i].player_uuid[10], instances[i].player_uuid[11], instances[i].player_uuid[12], instances[i].player_uuid[13], instances[i].player_uuid[14],
                    instances[i].player_uuid[15] );

          D_INFO( "            [%2d] { instance %s } { app %s } { player %s }  %s\n", i, buf, buf2, buf3, instances[i].app.name );

          if (m_stop_instance && !memcmp( m_stop_instance_uuid, instances[i].uuid, 16 )) {
               D_INFO( "               -> stopping!\n" );

               ret = player->StopInstance( player, m_stop_instance_uuid );
               if (ret)
                    D_DERROR( ret, "Voodoo/Play/Client: IVoodooPlayer::StopInstance() failed!\n" );
          }

          if (m_wait_instance && !memcmp( m_wait_instance_uuid, instances[i].uuid, 16 )) {
               D_INFO( "               -> waiting!\n" );

               ret = player->WaitInstance( player, m_wait_instance_uuid );
               if (ret)
                    D_DERROR( ret, "Voodoo/Play/Client: IVoodooPlayer::WaitInstance() failed!\n" );
          }
     }


     player->Release( player );

     return DENUM_OK;
}

int
main( int argc, char *argv[] )
{
     DFBResult       ret;
     VoodooPlayInfo  info;
     VoodooPlayer   *player = NULL;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     if (m_name) {
          direct_snputs( info.name, m_name, VOODOO_PLAYER_NAME_LENGTH );
     }

     ret = voodoo_player_create( m_name ? &info : NULL, &player );
     if (ret) {
          D_ERROR( "Voodoo/Play: Could not create the player (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }


     voodoo_player_broadcast( player );

     sleep( 1 );

     voodoo_player_enumerate( player, player_callback, NULL );


out:
     if (player)
          voodoo_player_destroy( player );

     return ret;
}

/**********************************************************************************************************************/

static DFBBoolean
print_usage( const char *name )
{
     fprintf( stderr, "= Voodoo Play Test Client =\n"
                      "\n"
                      "This utility uses Voodoo Play to discover all players and queries them for IVoodooPlayer\n"
                      "to list and launch applications as well as stop or wait for running instances.\n"
                      "\n"
                      "Usage: %s [options]\n"
                      "\n"
                      "Options:\n"
                      "  -n <name>                     Set local player name (of this process)\n"
                      "  -l <app UUID> <player UUID>   Launch application (from app server) on target player (remote display)\n"
                      "  -s <instance UUID>            Stop instance\n"
                      "  -w <instance UUID>            Wait for instance to terminate\n",
              name );

     return DFB_FALSE;
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int i;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-l" )) {
               if (++i == argc)
                    return print_usage( argv[0] );

               sscanf( argv[i], "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                       (unsigned int*)&m_launch_app_uuid[0], (unsigned int*)&m_launch_app_uuid[1], (unsigned int*)&m_launch_app_uuid[2], (unsigned int*)&m_launch_app_uuid[3], (unsigned int*)&m_launch_app_uuid[4],
                       (unsigned int*)&m_launch_app_uuid[5], (unsigned int*)&m_launch_app_uuid[6], (unsigned int*)&m_launch_app_uuid[7], (unsigned int*)&m_launch_app_uuid[8], (unsigned int*)&m_launch_app_uuid[9],
                       (unsigned int*)&m_launch_app_uuid[10], (unsigned int*)&m_launch_app_uuid[11], (unsigned int*)&m_launch_app_uuid[12], (unsigned int*)&m_launch_app_uuid[13], (unsigned int*)&m_launch_app_uuid[14],
                       (unsigned int*)&m_launch_app_uuid[15] );


               if (++i == argc)
                    return print_usage( argv[0] );


               sscanf( argv[i], "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                       (unsigned int*)&m_launch_player_uuid[0], (unsigned int*)&m_launch_player_uuid[1], (unsigned int*)&m_launch_player_uuid[2], (unsigned int*)&m_launch_player_uuid[3], (unsigned int*)&m_launch_player_uuid[4],
                       (unsigned int*)&m_launch_player_uuid[5], (unsigned int*)&m_launch_player_uuid[6], (unsigned int*)&m_launch_player_uuid[7], (unsigned int*)&m_launch_player_uuid[8], (unsigned int*)&m_launch_player_uuid[9],
                       (unsigned int*)&m_launch_player_uuid[10], (unsigned int*)&m_launch_player_uuid[11], (unsigned int*)&m_launch_player_uuid[12], (unsigned int*)&m_launch_player_uuid[13], (unsigned int*)&m_launch_player_uuid[14],
                       (unsigned int*)&m_launch_player_uuid[15] );


               m_launch_app = true;
          }
          else if (!strcmp( argv[i], "-s" )) {
               if (++i == argc)
                    return print_usage( argv[0] );

               sscanf( argv[i], "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                       (unsigned int*)&m_stop_instance_uuid[0], (unsigned int*)&m_stop_instance_uuid[1], (unsigned int*)&m_stop_instance_uuid[2], (unsigned int*)&m_stop_instance_uuid[3], (unsigned int*)&m_stop_instance_uuid[4],
                       (unsigned int*)&m_stop_instance_uuid[5], (unsigned int*)&m_stop_instance_uuid[6], (unsigned int*)&m_stop_instance_uuid[7], (unsigned int*)&m_stop_instance_uuid[8], (unsigned int*)&m_stop_instance_uuid[9],
                       (unsigned int*)&m_stop_instance_uuid[10], (unsigned int*)&m_stop_instance_uuid[11], (unsigned int*)&m_stop_instance_uuid[12], (unsigned int*)&m_stop_instance_uuid[13], (unsigned int*)&m_stop_instance_uuid[14],
                       (unsigned int*)&m_stop_instance_uuid[15] );

               m_stop_instance = true;
          }
          else if (!strcmp( argv[i], "-w" )) {
               if (++i == argc)
                    return print_usage( argv[0] );

               sscanf( argv[i], "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                       (unsigned int*)&m_wait_instance_uuid[0], (unsigned int*)&m_wait_instance_uuid[1], (unsigned int*)&m_wait_instance_uuid[2], (unsigned int*)&m_wait_instance_uuid[3], (unsigned int*)&m_wait_instance_uuid[4],
                       (unsigned int*)&m_wait_instance_uuid[5], (unsigned int*)&m_wait_instance_uuid[6], (unsigned int*)&m_wait_instance_uuid[7], (unsigned int*)&m_wait_instance_uuid[8], (unsigned int*)&m_wait_instance_uuid[9],
                       (unsigned int*)&m_wait_instance_uuid[10], (unsigned int*)&m_wait_instance_uuid[11], (unsigned int*)&m_wait_instance_uuid[12], (unsigned int*)&m_wait_instance_uuid[13], (unsigned int*)&m_wait_instance_uuid[14],
                       (unsigned int*)&m_wait_instance_uuid[15] );

               m_wait_instance = true;
          }
          else if (!strcmp( argv[i], "-n" )) {
               if (++i == argc)
                    return print_usage( argv[0] );

               m_name = argv[i];
          }
          else
               return print_usage( argv[0] );
     }

     return DFB_TRUE;
}

