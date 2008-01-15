/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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
#include <unistd.h>

#include <fusionsound.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <ifusionsoundplayback_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSoundPlayback *thiz,
                            IFusionSoundPlayback *real,
                            VoodooManager        *manager,
                            VoodooInstanceID      super,
                            void                 *arg,
                            VoodooInstanceID     *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundPlayback, Dispatcher )

/**************************************************************************************************/

typedef struct {
     int                    ref;          /* reference counter */

     IFusionSoundPlayback  *real;

     VoodooInstanceID       self;
} IFusionSoundPlayback_Dispatcher_data;

/***********************************************************************************************/

static void
IFusionSoundPlayback_Dispatcher_Destruct( IFusionSoundPlayback *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/***********************************************************************************************/

static DFBResult
IFusionSoundPlayback_Dispatcher_AddRef( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_Release( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     if (--data->ref == 0)
          IFusionSoundPlayback_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_Start( IFusionSoundPlayback *thiz,
                                       int                   start,
                                       int                   stop )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_Stop( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_Continue( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_Wait( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_GetStatus( IFusionSoundPlayback *thiz,
                                           DFBBoolean           *ret_playing,
                                           int                  *ret_position )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_SetVolume( IFusionSoundPlayback *thiz,
                                           float                 level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_SetPan( IFusionSoundPlayback *thiz,
                                        float                 value )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_SetPitch( IFusionSoundPlayback *thiz,
                                          float                 value )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_SetDirection( IFusionSoundPlayback *thiz,
                                              FSPlaybackDirection   direction )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundPlayback_Dispatcher_SetDownmixLevels( IFusionSoundPlayback *thiz,
                                                  float                 center,
                                                  float                 rear )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Start( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     int                  start;
     int                  stop;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, start );
     VOODOO_PARSER_GET_INT( parser, stop );
     VOODOO_PARSER_END( parser );
     
     ret = real->Start( real, start, stop );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Stop( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     ret = real->Stop( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Continue( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     ret = real->Continue( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Wait( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     ret = real->Wait( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetStatus( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     DFBBoolean   playing;
     int          position;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     ret = real->GetStatus( real, &playing, &position );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, playing,
                                    VMBT_INT, position,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetVolume( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     float               level;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &level, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetVolume( real, level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetPan( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     float               value;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &value, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetPan( real, value );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetPitch( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     float               value;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &value, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetPitch( real, value );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetDirection( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     FSPlaybackDirection direction;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, direction );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetDirection( real, direction );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetDownmixLevels( IFusionSoundPlayback *thiz, IFusionSoundPlayback *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     float               center;
     float               rear;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &center, sizeof(float) );
     VOODOO_PARSER_READ_DATA( parser, &rear, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetDownmixLevels( real, center, rear );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

#define HANDLE_CASE(name) \
     case IFUSIONSOUNDPLAYBACK_METHOD_ID_##name : \
          return Dispatch_##name ( dispatcher, real, manager, msg )

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     switch (msg->method) {
          HANDLE_CASE(Start);
          
          HANDLE_CASE(Stop);
          
          HANDLE_CASE(Continue);
          
          HANDLE_CASE(Wait);
          
          HANDLE_CASE(GetStatus);
          
          HANDLE_CASE(SetVolume);
          
          HANDLE_CASE(SetPan);
          
          HANDLE_CASE(SetPitch);
          
          HANDLE_CASE(SetDirection);
          
          HANDLE_CASE(SetDownmixLevels);
     }

     return DFB_NOSUCHMETHOD;
}

#undef HANDLE_CASE

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DFBResult
Construct( IFusionSoundPlayback *thiz,
           IFusionSoundPlayback *real,
           VoodooManager        *manager,
           VoodooInstanceID      super,
           void                 *arg,      /* Optional arguments to constructor */
           VoodooInstanceID     *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundPlayback_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }
     
     *ret_instance = instance;

     data->ref   = 1;
     data->real  = real;
     data->self  = instance;
     
     thiz->AddRef           = IFusionSoundPlayback_Dispatcher_AddRef;
     thiz->Release          = IFusionSoundPlayback_Dispatcher_Release;
     thiz->Start            = IFusionSoundPlayback_Dispatcher_Start;
     thiz->Stop             = IFusionSoundPlayback_Dispatcher_Stop;
     thiz->Continue         = IFusionSoundPlayback_Dispatcher_Continue;
     thiz->Wait             = IFusionSoundPlayback_Dispatcher_Wait; 
     thiz->GetStatus        = IFusionSoundPlayback_Dispatcher_GetStatus;
     thiz->SetVolume        = IFusionSoundPlayback_Dispatcher_SetVolume;
     thiz->SetPan           = IFusionSoundPlayback_Dispatcher_SetPan;
     thiz->SetPitch         = IFusionSoundPlayback_Dispatcher_SetPitch;
     thiz->SetDirection     = IFusionSoundPlayback_Dispatcher_SetDirection;
     thiz->SetDownmixLevels = IFusionSoundPlayback_Dispatcher_SetDownmixLevels;
     
     return DFB_OK;
}

