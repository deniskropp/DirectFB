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

#include <dpack.h>

#include <ifusionsoundbuffer_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSoundBuffer *thiz,
                            IFusionSoundBuffer *real,
                            VoodooManager      *manager,
                            VoodooInstanceID    super,
                            void               *arg,
                            VoodooInstanceID   *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundBuffer, Dispatcher )

/**************************************************************************************************/

typedef struct {
     int                    ref;          /* reference counter */

     IFusionSoundBuffer    *real;

     VoodooInstanceID       self;
     
     FSSampleFormat         format;
     int                    channels;
     int                    bytes_per_frame;
} IFusionSoundBuffer_Dispatcher_data;

/***********************************************************************************************/

static void
IFusionSoundBuffer_Dispatcher_Destruct( IFusionSoundBuffer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/***********************************************************************************************/

static DFBResult
IFusionSoundBuffer_Dispatcher_AddRef( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_Release( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     if (--data->ref == 0)
          IFusionSoundBuffer_Dispatcher_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IFusionSoundBuffer_Dispatcher_GetDescription( IFusionSoundBuffer  *thiz,
                                              FSBufferDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_SetPosition( IFusionSoundBuffer *thiz,
                                           int                 position )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
    
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_Lock( IFusionSoundBuffer  *thiz,
                                    void               **ret_data,
                                    int                 *ret_frames,
                                    int                 *ret_bytes )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_Unlock( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_Play( IFusionSoundBuffer *thiz,
                                    FSBufferPlayFlags   flags )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_Stop( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Dispatcher_CreatePlayback( IFusionSoundBuffer    *thiz,
                                              IFusionSoundPlayback **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

/***********************************************************************************************/

static DirectResult
Dispatch_GetDescription( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     FSBufferDescription desc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     ret = real->GetDescription( real, &desc );
     if (ret)
          return ret;
          
     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(FSBufferDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetPosition( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     int                 position;
     VoodooMessageParser parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, position );
     VOODOO_PARSER_END( parser );
     
     ret = real->SetPosition( real, position );
          
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Lock( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     /* Nothing to do. */
     return DFB_OK;
}

static DirectResult
Dispatch_Unlock( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const void          *src;
     void                *dst;
     int                  offset;
     int                  length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, src );
     VOODOO_PARSER_GET_INT( parser, offset );
     VOODOO_PARSER_GET_INT( parser, length );
     VOODOO_PARSER_END( parser );
     
     ret = real->Lock( real, &dst, NULL, NULL );
     if (ret)
          return ret;
          
     direct_memcpy( dst+offset, src, length*data->bytes_per_frame );
     
     real->Unlock( real );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Unlock_DPACK( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const void          *src;
     void                *dst;
     int                  offset;
     int                  length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, src );
     VOODOO_PARSER_GET_INT( parser, offset );
     VOODOO_PARSER_GET_INT( parser, length );
     VOODOO_PARSER_END( parser );
     
     ret = real->Lock( real, &dst, NULL, NULL );
     if (ret)
          return ret;
          
     dpack_decode( src, data->format, data->channels, length, dst+offset );
     
     real->Unlock( real );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Play( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     FSBufferPlayFlags   flags;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, flags );
     VOODOO_PARSER_END( parser );
     
     ret = real->Play( real, flags );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Stop( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     ret = real->Stop( real );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}
     
static DirectResult
Dispatch_CreatePlayback( IFusionSoundBuffer *thiz, IFusionSoundBuffer *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     IFusionSoundPlayback *playback;
     VoodooInstanceID      instance;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Dispatcher)
     
     ret = real->CreatePlayback( real, &playback );
     if (ret)
          return ret;
          
     ret = voodoo_construct_dispatcher( manager, "IFusionSoundPlayback",
                                        playback, data->self, NULL, &instance, NULL );
     if (ret) {
          playback->Release( playback );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

#define HANDLE_CASE(name) \
     case IFUSIONSOUNDBUFFER_METHOD_ID_##name : \
          return Dispatch_##name ( dispatcher, real, manager, msg )

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     switch (msg->method) {
          HANDLE_CASE(GetDescription);
          
          HANDLE_CASE(SetPosition);
          
          HANDLE_CASE(Lock);
          
          HANDLE_CASE(Unlock);
          
          HANDLE_CASE(Unlock_DPACK);
          
          HANDLE_CASE(Play);
          
          HANDLE_CASE(Stop);
          
          HANDLE_CASE(CreatePlayback);
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
Construct( IFusionSoundBuffer *thiz,
           IFusionSoundBuffer *real,
           VoodooManager      *manager,
           VoodooInstanceID    super,
           void               *arg,      /* Optional arguments to constructor */
           VoodooInstanceID   *ret_instance )
{
     DFBResult           ret;
     VoodooInstanceID    instance;
     FSBufferDescription dsc;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundBuffer_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }
     
     *ret_instance = instance;

     data->ref   = 1;
     data->real  = real;
     data->self  = instance;
     
     real->GetDescription( real, &dsc );
     data->format = dsc.sampleformat;
     data->channels = dsc.channels;
     data->bytes_per_frame = data->channels * FS_BYTES_PER_SAMPLE(data->format);

     thiz->AddRef         = IFusionSoundBuffer_Dispatcher_AddRef;
     thiz->Release        = IFusionSoundBuffer_Dispatcher_Release;
     thiz->GetDescription = IFusionSoundBuffer_Dispatcher_GetDescription; 
     thiz->SetPosition    = IFusionSoundBuffer_Dispatcher_SetPosition;
     thiz->Lock           = IFusionSoundBuffer_Dispatcher_Lock;
     thiz->Unlock         = IFusionSoundBuffer_Dispatcher_Unlock;
     thiz->Play           = IFusionSoundBuffer_Dispatcher_Play;
     thiz->Stop           = IFusionSoundBuffer_Dispatcher_Stop;
     thiz->CreatePlayback = IFusionSoundBuffer_Dispatcher_CreatePlayback;
     
     return DFB_OK;
}


