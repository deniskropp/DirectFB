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

#include <directfb_version.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/interface.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/play_server.h>
#include <voodoo/ivoodooplayer.h>


D_DEBUG_DOMAIN( IVoodooPlayer_, "IVoodooPlayer", "IVoodooPlayer" );

/**********************************************************************************************************************/

static DirectResult CreateRemote( const char *host, int session, IVoodooPlayer **ret_interface );

/**********************************************************************************************************************/

typedef struct {
     int            ref;
} IVoodooPlayer_data;

static DirectResult
IVoodooPlayer_AddRef( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IVoodooPlayer );

     data->ref++;

     return DR_OK;
}

static DirectResult
IVoodooPlayer_Release( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IVoodooPlayer );

     if (!--data->ref)
          DIRECT_DEALLOCATE_INTERFACE( thiz );

     return DR_OK;
}

static DirectResult
IVoodooPlayer_GetApps( IVoodooPlayer        *thiz,
                       unsigned int          max_num,
                       unsigned int         *ret_num,
                       VoodooAppDescription *ret_applications )
{
     D_DEBUG_AT( IVoodooPlayer_, "%s()\n", __func__ );

     if (!max_num || !ret_num || !ret_applications)
          return DR_INVARG;

     return voodoo_player_get_apps( voodoo_player, max_num, ret_num, ret_applications );
}

static DirectResult
IVoodooPlayer_LaunchApp( IVoodooPlayer *thiz,
                         const u8       app_uuid[16],
                         const u8       player_uuid[16],
                         u8             ret_instance_uuid[16] )
{
     D_DEBUG_AT( IVoodooPlayer_, "%s()\n", __func__ );

     if (!app_uuid || !player_uuid || !ret_instance_uuid)
          return DR_INVARG;

     return voodoo_player_launch_app( voodoo_player, app_uuid, player_uuid, ret_instance_uuid );
}

static DirectResult
IVoodooPlayer_StopInstance( IVoodooPlayer *thiz,
                            const u8       instance_uuid[16] )
{
     D_DEBUG_AT( IVoodooPlayer_, "%s()\n", __func__ );

     if (!instance_uuid)
          return DR_INVARG;

     return voodoo_player_stop_instance( voodoo_player, instance_uuid );
}

static DirectResult
IVoodooPlayer_WaitInstance( IVoodooPlayer *thiz,
                            const u8       instance_uuid[16] )
{
     D_DEBUG_AT( IVoodooPlayer_, "%s()\n", __func__ );

     if (!instance_uuid)
          return DR_INVARG;

     return voodoo_player_wait_instance( voodoo_player, instance_uuid );
}

static DirectResult
IVoodooPlayer_GetInstances( IVoodooPlayer                *thiz,
                            unsigned int                  max_num,
                            unsigned int                 *ret_num,
                            VoodooAppInstanceDescription *ret_instances )
{
     D_DEBUG_AT( IVoodooPlayer_, "%s()\n", __func__ );

     if (!max_num || !ret_num || !ret_instances)
          return DR_INVARG;

     return voodoo_player_get_instances( voodoo_player, max_num, ret_num, ret_instances );
}

static DirectResult
IVoodooPlayer_Construct( IVoodooPlayer *thiz )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IVoodooPlayer );

     data->ref = 1;

     thiz->AddRef       = IVoodooPlayer_AddRef;
     thiz->Release      = IVoodooPlayer_Release;
     thiz->GetApps      = IVoodooPlayer_GetApps;
     thiz->LaunchApp    = IVoodooPlayer_LaunchApp;
     thiz->StopInstance = IVoodooPlayer_StopInstance;
     thiz->WaitInstance = IVoodooPlayer_WaitInstance;
     thiz->GetInstances = IVoodooPlayer_GetInstances;

     return DR_OK;
}

DirectResult
VoodooPlayerCreate( const char     *host,
                    int             port,
                    IVoodooPlayer **ret_interface )
{
     DirectResult   ret;
     IVoodooPlayer *player;

     if (!ret_interface)
          return DR_INVARG;

     if (host)
          return CreateRemote( host, port, ret_interface );

     if (!voodoo_player) {
          D_ERROR( "Voodoo/Player: Global voodoo_player is NULL!\n" );
          return DR_NOSUCHINSTANCE;
     }

     DIRECT_ALLOCATE_INTERFACE( player, IVoodooPlayer );

     ret = IVoodooPlayer_Construct( player );
     if (ret)
          return ret;

     *ret_interface = player;

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
CreateRemote( const char *host, int session, IVoodooPlayer **ret_interface )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *interface_ptr;

     D_ASSERT( host != NULL );
     D_ASSERT( ret_interface != NULL );

     ret = DirectGetInterface( &funcs, "IVoodooPlayer", "Requestor", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface_ptr );
     if (ret)
          return ret;

     ret = funcs->Construct( interface_ptr, host, session );
     if (ret)
          return ret;

     *ret_interface = interface_ptr;

     return DR_OK;
}

