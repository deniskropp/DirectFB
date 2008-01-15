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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "ifusionsound_dispatcher.h"

static DFBResult Probe();
static DFBResult Construct( IFusionSound     *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSound, Dispatcher )

/**************************************************************************************************/

typedef struct {
     int                    ref;          /* reference counter */

     IFusionSound          *real;

     VoodooInstanceID       self;         /* The instance of this dispatcher itself. */
} IFusionSound_Dispatcher_data;

/***********************************************************************************************/

static void
IFusionSound_Dispatcher_Destruct( IFusionSound *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/***********************************************************************************************/

static DFBResult
IFusionSound_Dispatcher_AddRef( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSound_Dispatcher_Release( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)

     if (--data->ref == 0)
          IFusionSound_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSound_Dispatcher_GetDeviceDescription( IFusionSound        *thiz,
                                              FSDeviceDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_CreateBuffer( IFusionSound               *thiz,
                                      const FSBufferDescription  *desc,
                                      IFusionSoundBuffer        **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_CreateStream( IFusionSound               *thiz,
                                      const FSStreamDescription  *desc,
                                      IFusionSoundStream        **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_CreateMusicProvider( IFusionSound               *thiz,
                                             const char                 *filename,
                                             IFusionSoundMusicProvider **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_GetMasterVolume( IFusionSound *thiz, 
                                         float        *level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_SetMasterVolume( IFusionSound *thiz,
                                         float         level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_GetLocalVolume( IFusionSound *thiz, 
                                        float        *level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_SetLocalVolume( IFusionSound *thiz,
                                        float         level )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_Suspend( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSound_Dispatcher_Resume( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetDeviceDescription( IFusionSound *thiz, IFusionSound *real,
                               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     FSDeviceDescription desc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     ret = real->GetDeviceDescription( real, &desc );
     if (ret)
          return ret;
          
     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(FSDeviceDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateBuffer( IFusionSound *thiz, IFusionSound *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult               ret;
     const FSBufferDescription *desc;
     IFusionSoundBuffer        *buffer;
     VoodooInstanceID           instance;
     VoodooMessageParser        parser;
     FSBufferDescription        dsc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );
     
     ret = real->CreateBuffer( real, desc, &buffer );
     if (ret)
          return ret;
          
     buffer->GetDescription( buffer, &dsc );
          
     ret = voodoo_construct_dispatcher( manager, "IFusionSoundBuffer",
                                        buffer, data->self, NULL, &instance, NULL );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_DATA, sizeof(FSBufferDescription), &dsc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateStream( IFusionSound *thiz, IFusionSound *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult               ret;
     const FSStreamDescription *desc;
     IFusionSoundStream        *stream;
     VoodooInstanceID           instance;
     VoodooMessageParser        parser;
     FSStreamDescription        dsc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ODATA( parser, desc );
     VOODOO_PARSER_END( parser );
     
     ret = real->CreateStream( real, desc, &stream );
     if (ret)
          return ret;
          
     stream->GetDescription( stream, &dsc );
          
     ret = voodoo_construct_dispatcher( manager, "IFusionSoundStream",
                                        stream, data->self, NULL, &instance, NULL );
     if (ret) {
          stream->Release( stream );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_DATA, sizeof(FSStreamDescription), &dsc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateMusicProvider( IFusionSound *thiz, IFusionSound *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult               ret;
     const char                *filename;
     IFusionSoundMusicProvider *provider;
     VoodooInstanceID           instance;
     VoodooMessageParser        parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_STRING( parser, filename );
     VOODOO_PARSER_END( parser );
     
     ret = real->CreateMusicProvider( real, filename, &provider );
     if (ret)
          return ret;
          
     ret = voodoo_construct_dispatcher( manager, "IFusionSoundMusicProvider",
                                        provider, data->self, NULL, &instance, NULL );
     if (ret) {
          provider->Release( provider );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetMasterVolume( IFusionSound *thiz, IFusionSound *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     float        level;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     ret = real->GetMasterVolume( real, &level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(float), &level, 
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetMasterVolume( IFusionSound *thiz, IFusionSound *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     float               level;
     VoodooMessageParser parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &level, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetMasterVolume( real, level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetLocalVolume( IFusionSound *thiz, IFusionSound *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     float        level;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     ret = real->GetLocalVolume( real, &level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(float), &level, 
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetLocalVolume( IFusionSound *thiz, IFusionSound *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     float               level;
     VoodooMessageParser parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_READ_DATA( parser, &level, sizeof(float) );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetLocalVolume( real, level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Suspend( IFusionSound *thiz, IFusionSound *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     ret = real->Suspend( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Resume( IFusionSound *thiz, IFusionSound *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Dispatcher)
     
     ret = real->Resume( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

#define HANDLE_CASE(name) \
     case IFUSIONSOUND_METHOD_ID_##name : \
          return Dispatch_##name ( dispatcher, real, manager, msg )

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     switch (msg->method) {
          HANDLE_CASE(GetDeviceDescription);
          
          HANDLE_CASE(CreateBuffer);
          
          HANDLE_CASE(CreateStream);
          
          HANDLE_CASE(CreateMusicProvider);
          
          HANDLE_CASE(GetMasterVolume);
          
          HANDLE_CASE(SetMasterVolume);
          
          HANDLE_CASE(GetLocalVolume);
          
          HANDLE_CASE(SetLocalVolume);
          
          HANDLE_CASE(Suspend);
          
          HANDLE_CASE(Resume);
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
Construct( IFusionSound *thiz, VoodooManager *manager, VoodooInstanceID *ret_instance )
{
     DFBResult         ret;
     IFusionSound     *real;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSound_Dispatcher)

     ret = FusionSoundCreate( &real );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = voodoo_manager_register_local( manager, true, thiz, real, Dispatch, &instance );
     if (ret) {
          real->Release( real );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->ref  = 1;
     data->real = real;
     data->self = instance;

     thiz->AddRef               = IFusionSound_Dispatcher_AddRef;
     thiz->Release              = IFusionSound_Dispatcher_Release;
     thiz->GetDeviceDescription = IFusionSound_Dispatcher_GetDeviceDescription;
     thiz->CreateBuffer         = IFusionSound_Dispatcher_CreateBuffer;
     thiz->CreateStream         = IFusionSound_Dispatcher_CreateStream;
     thiz->CreateMusicProvider  = IFusionSound_Dispatcher_CreateMusicProvider;
     thiz->GetMasterVolume      = IFusionSound_Dispatcher_GetMasterVolume;
     thiz->SetMasterVolume      = IFusionSound_Dispatcher_SetMasterVolume;
     thiz->GetLocalVolume       = IFusionSound_Dispatcher_GetLocalVolume;
     thiz->SetLocalVolume       = IFusionSound_Dispatcher_SetLocalVolume;
     thiz->Suspend              = IFusionSound_Dispatcher_Suspend;
     thiz->Resume               = IFusionSound_Dispatcher_Resume;

     return DFB_OK;
}

