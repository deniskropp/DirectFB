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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/ivoodooplayer.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "ivoodooplayer_dispatcher.h"

static DirectResult Probe( void );
static DirectResult Construct( IVoodooPlayer    *thiz,
                               VoodooManager    *manager,
                               VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IVoodooPlayer, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IVoodooPlayer_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IVoodooPlayer         *real;

     VoodooInstanceID       self;         /* The instance of this dispatcher itself. */
} IVoodooPlayer_Dispatcher_data;

/**************************************************************************************************/

static void
IVoodooPlayer_Dispatcher_Destruct( IVoodooPlayer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IVoodooPlayer_Dispatcher_AddRef( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     data->ref++;

     return DR_OK;
}

static DirectResult
IVoodooPlayer_Dispatcher_Release( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     if (--data->ref == 0)
          IVoodooPlayer_Dispatcher_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IVoodooPlayer_Dispatcher_GetApps( IVoodooPlayer        *thiz,
                                  unsigned int          max_num,
                                  unsigned int         *ret_num,
                                  VoodooAppDescription *ret_applications )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IVoodooPlayer_Dispatcher_LaunchApp( IVoodooPlayer *thiz,
                                    const u8       app_uuid[16],
                                    const u8       player_uuid[16],
                                    u8             ret_instance_uuid[16] )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IVoodooPlayer_Dispatcher_StopInstance( IVoodooPlayer *thiz,
                                       const u8       instance_uuid[16] )
{
     return DR_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetApps( IVoodooPlayer *thiz, IVoodooPlayer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     unsigned int          max_num;
     VoodooMessageParser   parser;
     unsigned int          num;
     VoodooAppDescription *apps;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, max_num );
     VOODOO_PARSER_END( parser );

     if (max_num > 1000)
          return DR_LIMITEXCEEDED;

     apps = D_MALLOC( max_num * sizeof(VoodooAppDescription) );
     if (!apps)
          return D_OOM();

     ret = real->GetApps( real, max_num, &num, apps );
     if (ret == DR_OK) {
          if (num > 0) {
               ret = voodoo_manager_respond( manager, true, msg->header.serial,
                                             ret, VOODOO_INSTANCE_NONE,
                                             VMBT_UINT, num,
                                             VMBT_DATA, num * sizeof(VoodooAppDescription), apps,
                                             VMBT_NONE );
          }
          else {
               ret = voodoo_manager_respond( manager, true, msg->header.serial,
                                             ret, VOODOO_INSTANCE_NONE,
                                             VMBT_UINT, num,
                                             VMBT_NONE );
          }
     }

     D_FREE( apps );

     return ret;
}

static DirectResult
Dispatch_LaunchApp( IVoodooPlayer *thiz, IVoodooPlayer *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult             ret;
     VoodooMessageParser      parser;
     const u8                *app_uuid;
     const u8                *player_uuid;
     u8                       instance_uuid[16];

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, app_uuid );
     VOODOO_PARSER_GET_DATA( parser, player_uuid );
     VOODOO_PARSER_END( parser );

     ret = real->LaunchApp( real, app_uuid, player_uuid, instance_uuid );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, 16, instance_uuid,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_StopInstance( IVoodooPlayer *thiz, IVoodooPlayer *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const u8            *instance_uuid;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, instance_uuid );
     VOODOO_PARSER_END( parser );

     ret = real->StopInstance( real, instance_uuid );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_WaitInstance( IVoodooPlayer *thiz, IVoodooPlayer *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const u8            *instance_uuid;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, instance_uuid );
     VOODOO_PARSER_END( parser );

     ret = real->WaitInstance( real, instance_uuid );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetInstances( IVoodooPlayer *thiz, IVoodooPlayer *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                  ret;
     unsigned int                  max_num;
     VoodooMessageParser           parser;
     unsigned int                  num;
     VoodooAppInstanceDescription *instances;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, max_num );
     VOODOO_PARSER_END( parser );

     if (max_num > 1000)
          return DR_LIMITEXCEEDED;

     instances = D_MALLOC( max_num * sizeof(VoodooAppInstanceDescription) );
     if (!instances)
          return D_OOM();

     ret = real->GetInstances( real, max_num, &num, instances );
     if (ret == DR_OK) {
          if (num > 0) {
               ret = voodoo_manager_respond( manager, true, msg->header.serial,
                                             ret, VOODOO_INSTANCE_NONE,
                                             VMBT_UINT, num,
                                             VMBT_DATA, num * sizeof(VoodooAppInstanceDescription), instances,
                                             VMBT_NONE );
          }
          else {
               ret = voodoo_manager_respond( manager, true, msg->header.serial,
                                             ret, VOODOO_INSTANCE_NONE,
                                             VMBT_UINT, num,
                                             VMBT_NONE );
          }
     }

     D_FREE( instances );

     return ret;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IVoodooPlayer/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IVOODOOPLAYER_METHOD_ID_GetApps:
               return Dispatch_GetApps( dispatcher, real, manager, msg );

          case IVOODOOPLAYER_METHOD_ID_LaunchApp:
               return Dispatch_LaunchApp( dispatcher, real, manager, msg );

          case IVOODOOPLAYER_METHOD_ID_StopInstance:
               return Dispatch_StopInstance( dispatcher, real, manager, msg );

          case IVOODOOPLAYER_METHOD_ID_WaitInstance:
               return Dispatch_WaitInstance( dispatcher, real, manager, msg );

          case IVOODOOPLAYER_METHOD_ID_GetInstances:
               return Dispatch_GetInstances( dispatcher, real, manager, msg );
     }

     return DR_NOSUCHMETHOD;
}

/**************************************************************************************************/

static DirectResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DR_UNSUPPORTED;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DirectResult
Construct( IVoodooPlayer *thiz, VoodooManager *manager, VoodooInstanceID *ret_instance )
{
     DirectResult      ret;
     IVoodooPlayer    *real;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IVoodooPlayer_Dispatcher)

     /*
      * Create local IVoodooPlayer instance
      */
     ret = VoodooPlayerCreate( NULL, 0, &real );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = voodoo_manager_register_local( manager, VOODOO_INSTANCE_NONE, thiz, real, Dispatch, &instance );
     if (ret) {
          real->Release( real );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->ref  = 1;
     data->real = real;
     data->self = instance;

     thiz->AddRef       = IVoodooPlayer_Dispatcher_AddRef;
     thiz->Release      = IVoodooPlayer_Dispatcher_Release;
     thiz->GetApps      = IVoodooPlayer_Dispatcher_GetApps;
     thiz->LaunchApp    = IVoodooPlayer_Dispatcher_LaunchApp;
     thiz->StopInstance = IVoodooPlayer_Dispatcher_StopInstance;

     return DR_OK;
}

