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

#include <ifusionsoundstream_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSoundStream *thiz,
                            IFusionSoundStream *real,
                            VoodooManager      *manager,
                            VoodooInstanceID    super,
                            void               *arg,
                            VoodooInstanceID   *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundStream, Dispatcher )

/**************************************************************************************************/

typedef struct {
     int                    ref;          /* reference counter */

     IFusionSoundStream    *real;

     VoodooInstanceID       self;
     
     FSSampleFormat         format;
     int                    channels;
     int                    bytes_per_frame;
} IFusionSoundStream_Dispatcher_data;

/***********************************************************************************************/

static void
IFusionSoundStream_Dispatcher_Destruct( IFusionSoundStream *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/***********************************************************************************************/

static DFBResult
IFusionSoundStream_Dispatcher_AddRef( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Dispatcher_Release( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     if (--data->ref == 0)
          IFusionSoundStream_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Dispatcher_GetDescription( IFusionSoundStream  *thiz,
                                              FSStreamDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Write( IFusionSoundStream *thiz,
                                     const void         *sample_data,
                                     int                 length )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Wait( IFusionSoundStream *thiz,
                                    int                 length )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_GetStatus( IFusionSoundStream *thiz,
                                         int                *filled,
                                         int                *total,
                                         int                *read_position,
                                         int                *write_position,
                                         DFBBoolean         *playing )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Flush( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Drop( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_GetPresentationDelay( IFusionSoundStream *thiz,
                                                    int                *delay )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_GetPlayback( IFusionSoundStream    *thiz,
                                           IFusionSoundPlayback **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Access( IFusionSoundStream  *thiz,
                                      void               **ret_data,
                                      int                 *ret_avail )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundStream_Dispatcher_Commit( IFusionSoundStream  *thiz,
                                      int                  length )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetDescription( IFusionSoundStream *thiz, IFusionSoundStream *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     FSStreamDescription desc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     ret = real->GetDescription( real, &desc );
     if (ret)
          return ret;
          
     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(FSStreamDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Write( IFusionSoundStream *thiz, IFusionSoundStream *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const void          *samples;
     int                  length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, samples );
     VOODOO_PARSER_GET_INT( parser, length );
     VOODOO_PARSER_END( parser );
     
     ret = real->Write( real, samples, length );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Write_DPACK( IFusionSoundStream *thiz, IFusionSoundStream *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const void          *source;
     s16                 *samples;
     int                  length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, source );
     VOODOO_PARSER_GET_INT( parser, length );
     VOODOO_PARSER_END( parser );
     
     samples = alloca( length * data->bytes_per_frame );
     
     dpack_decode( source, data->format, data->channels, length, samples );
     
     ret = real->Write( real, samples, length );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Wait( IFusionSoundStream *thiz, IFusionSoundStream *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     int                  length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, length );
     VOODOO_PARSER_END( parser );
     
     ret = real->Wait( real, length );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetStatus( IFusionSoundStream *thiz, IFusionSoundStream *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          filled;
     int          total;
     int          read_pos;
     int          write_pos;
     DFBBoolean   playing;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)

     ret = real->GetStatus( real, &filled, &total, &read_pos, &write_pos, &playing );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, filled,
                                    VMBT_INT, total,
                                    VMBT_INT, read_pos,
                                    VMBT_INT, write_pos,
                                    VMBT_INT, playing,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Flush( IFusionSoundStream *thiz, IFusionSoundStream *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     ret = real->Flush( real );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Drop( IFusionSoundStream *thiz, IFusionSoundStream *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     ret = real->Drop( real );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetPresentationDelay( IFusionSoundStream *thiz, IFusionSoundStream *real,
                               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          delay;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     ret = real->GetPresentationDelay( real, &delay );
     
     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, delay,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetPlayback( IFusionSoundStream *thiz, IFusionSoundStream *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     IFusionSoundPlayback *playback;
     VoodooInstanceID      instance;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Dispatcher)
     
     ret = real->GetPlayback( real, &playback );
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

static DirectResult
Dispatch_Access( IFusionSoundStream *thiz, IFusionSoundStream *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_UNIMPLEMENTED();
     
     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_Commit( IFusionSoundStream *thiz, IFusionSoundStream *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_UNIMPLEMENTED();
     
     return DFB_UNIMPLEMENTED;
}

#define HANDLE_CASE(name) \
     case IFUSIONSOUNDSTREAM_METHOD_ID_##name : \
          return Dispatch_##name ( dispatcher, real, manager, msg )

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     switch (msg->method) {
          HANDLE_CASE(GetDescription);
          
          HANDLE_CASE(Write);
          
          HANDLE_CASE(Write_DPACK);
          
          HANDLE_CASE(Wait);
          
          HANDLE_CASE(GetStatus);
          
          HANDLE_CASE(Flush);
          
          HANDLE_CASE(Drop);
          
          HANDLE_CASE(GetPresentationDelay);
          
          HANDLE_CASE(GetPlayback);
          
          HANDLE_CASE(Access);
          
          HANDLE_CASE(Commit);
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
Construct( IFusionSoundStream *thiz,
           IFusionSoundStream *real,
           VoodooManager      *manager,
           VoodooInstanceID    super,
           void               *arg,      /* Optional arguments to constructor */
           VoodooInstanceID   *ret_instance )
{
     DFBResult           ret;
     VoodooInstanceID    instance;
     FSStreamDescription dsc;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundStream_Dispatcher)

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

     thiz->AddRef               = IFusionSoundStream_Dispatcher_AddRef;
     thiz->Release              = IFusionSoundStream_Dispatcher_Release;
     thiz->GetDescription       = IFusionSoundStream_Dispatcher_GetDescription;
     thiz->Write                = IFusionSoundStream_Dispatcher_Write;
     thiz->Wait                 = IFusionSoundStream_Dispatcher_Wait;
     thiz->GetStatus            = IFusionSoundStream_Dispatcher_GetStatus;
     thiz->Flush                = IFusionSoundStream_Dispatcher_Flush;
     thiz->Drop                 = IFusionSoundStream_Dispatcher_Drop;  
     thiz->GetPresentationDelay = IFusionSoundStream_Dispatcher_GetPresentationDelay;
     thiz->GetPlayback          = IFusionSoundStream_Dispatcher_GetPlayback;    
     thiz->Access               = IFusionSoundStream_Dispatcher_Access;
     thiz->Commit               = IFusionSoundStream_Dispatcher_Commit;
     
     return DFB_OK;
}

