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

#include <misc/sound_conf.h>

#include <dpack.h>

#include <ifusionsoundstream_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSoundStream *thiz,
                            VoodooManager      *manager,
                            VoodooInstanceID    instance,
                            void               *arg );
                            
                            
#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundStream, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IFusionSoundStream_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
     
     int                  frames;
     FSSampleFormat       format;
     int                  channels;
     int                  bytes_per_frame;
     
     void                *buffer;
} IFusionSoundStream_Requestor_data;

#define MAX_DATA_SIZE (16 * 1024)

/**************************************************************************************************/

static void
IFusionSoundStream_Requestor_Destruct( IFusionSoundStream *thiz )
{
     IFusionSoundStream_Requestor_data *data = thiz->priv;
     
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );
     
     if (data->buffer)
          D_FREE( data->buffer );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IFusionSoundStream_Requestor_AddRef( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Requestor_Release( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)

     if (--data->ref == 0)
          IFusionSoundStream_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Requestor_GetDescription( IFusionSoundStream  *thiz,
                                             FSStreamDescription *desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_GetDescription,
                                   VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, desc, sizeof(FSStreamDescription) );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_Write( IFusionSoundStream *thiz,
                                    const void         *samples,
                                    int                 length )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     u8                    *src;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!samples || length < 1)
          return DFB_INVARG;
          
     src = (u8*) samples;
     while (length) {
          int len  = MIN( length, MAX_DATA_SIZE/data->bytes_per_frame );
          int size = len * data->bytes_per_frame; 
          
          ret = voodoo_manager_request( data->manager, data->instance,
                                        IFUSIONSOUNDSTREAM_METHOD_ID_Write, VREQ_RESPOND, &response,
                                        VMBT_DATA, size, src,
                                        VMBT_INT, len,
                                        VMBT_NONE );
          if (ret)
               return ret;

          ret = response->result;

          voodoo_manager_finish_request( data->manager, response );
          
          if (ret)
               return ret;
          
          src += size;
          length -= len;
     }          

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Requestor_Write_DPACK( IFusionSoundStream *thiz,
                                          const void         *samples,
                                          int                 length )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     u8                    *src;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!samples || length < 1)
          return DFB_INVARG;
          
     src = (u8*) samples;
     while (length) {
          int len = MIN( length, MAX_DATA_SIZE/data->bytes_per_frame );
          u8  buf[DPACK_MAX_PACKET(len, data->channels, data->bytes_per_frame)];
          int size;
          
          size = dpack_encode( src, data->format, data->channels, len, buf );
          D_ASSERT( size <= sizeof(buf) );
          
          ret = voodoo_manager_request( data->manager, data->instance,
                                        IFUSIONSOUNDSTREAM_METHOD_ID_Write_DPACK, VREQ_RESPOND, &response,
                                        VMBT_DATA, size, buf,
                                        VMBT_INT, len,
                                        VMBT_NONE );
          if (ret)
               return ret;

          ret = response->result;

          voodoo_manager_finish_request( data->manager, response );
          
          if (ret)
               return ret;
          
          src += len * data->bytes_per_frame;
          length -= len;
     }          

     return DFB_OK;
}

static DFBResult
IFusionSoundStream_Requestor_Wait( IFusionSoundStream *thiz,
                                   int                 length )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (length < 1 || length > data->frames)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_Wait, VREQ_RESPOND, &response,
                                   VMBT_INT, length,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_GetStatus( IFusionSoundStream *thiz,
                                        int                *ret_filled,
                                        int                *ret_total,
                                        int                *ret_read_pos,
                                        int                *ret_write_pos,
                                        DFBBoolean         *ret_playing )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     int                    filled;
     int                    total;
     int                    read_pos;
     int                    write_pos;
     DFBBoolean             playing;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_GetStatus, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, filled );
     VOODOO_PARSER_GET_INT( parser, total );
     VOODOO_PARSER_GET_INT( parser, read_pos );
     VOODOO_PARSER_GET_INT( parser, write_pos );
     VOODOO_PARSER_GET_INT( parser, playing );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );
     
     if (ret_filled)
          *ret_filled = filled;
          
     if (ret_total)
          *ret_total = total;
          
     if (ret_read_pos)
          *ret_read_pos = read_pos;
          
     if (ret_write_pos)
          *ret_write_pos = write_pos;
          
     if (ret_playing)
          *ret_playing = playing;

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_Flush( IFusionSoundStream *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_Flush, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_Drop( IFusionSoundStream *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_Drop, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_GetPresentationDelay( IFusionSoundStream *thiz,
                                                   int                *delay )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!delay)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_GetPresentationDelay, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, *delay );
     VOODOO_PARSER_END( parser );
     
     voodoo_manager_finish_request( data->manager, response );
     
     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_GetPlayback( IFusionSoundStream    *thiz,
                                          IFusionSoundPlayback **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!ret_interface)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDSTREAM_METHOD_ID_GetPlayback, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IFusionSoundPlayback",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IFusionSoundStream_Requestor_Access( IFusionSoundStream  *thiz,
                                     void               **ret_data,
                                     int                 *ret_avail )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (!ret_data || !ret_avail)
          return DFB_INVARG;
          
     if (!data->buffer)
          data->buffer = D_MALLOC( data->frames * data->bytes_per_frame );
     
     *ret_data = data->buffer;
     *ret_avail = data->buffer ? data->frames : 0;
     
     return data->buffer ? DFB_OK : DFB_NOSYSTEMMEMORY;
}

static DFBResult
IFusionSoundStream_Requestor_Commit( IFusionSoundStream  *thiz,
                                     int                  length )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundStream_Requestor)
     
     if (length < 0 || length > data->frames)
          return DFB_INVARG;
     
     if (length)
          return thiz->Write( thiz, data->buffer, length );
          
     return DFB_OK;
}

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
           VoodooManager      *manager,
           VoodooInstanceID    instance,
           void               *arg )
{
     const FSStreamDescription *dsc = arg;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundStream_Requestor)

     data->ref = 1;
     data->manager = manager;
     data->instance = instance;
     data->frames = dsc->buffersize;
     data->format = dsc->sampleformat;
     data->channels = dsc->channels;
     data->bytes_per_frame = dsc->channels * FS_BYTES_PER_SAMPLE(dsc->sampleformat);
     
     thiz->AddRef               = IFusionSoundStream_Requestor_AddRef;
     thiz->Release              = IFusionSoundStream_Requestor_Release;
     thiz->GetDescription       = IFusionSoundStream_Requestor_GetDescription;
     thiz->Write                = fs_config->remote_compression 
                                  ? IFusionSoundStream_Requestor_Write_DPACK
                                  : IFusionSoundStream_Requestor_Write;
     thiz->Wait                 = IFusionSoundStream_Requestor_Wait;
     thiz->GetStatus            = IFusionSoundStream_Requestor_GetStatus;
     thiz->Flush                = IFusionSoundStream_Requestor_Flush;
     thiz->Drop                 = IFusionSoundStream_Requestor_Drop;  
     thiz->GetPresentationDelay = IFusionSoundStream_Requestor_GetPresentationDelay;
     thiz->GetPlayback          = IFusionSoundStream_Requestor_GetPlayback;    
     thiz->Access               = IFusionSoundStream_Requestor_Access;
     thiz->Commit               = IFusionSoundStream_Requestor_Commit;

     return DFB_OK;
}

