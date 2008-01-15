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

#include <ifusionsoundbuffer_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSoundBuffer *thiz,
                            VoodooManager      *manager,
                            VoodooInstanceID    instance,
                            void               *arg );
                            
                            
#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundBuffer, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IFusionSoundBuffer_Requestor
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
     
     int                  position;
     
     bool                 locked;
} IFusionSoundBuffer_Requestor_data;

#define MAX_DATA_SIZE (16 * 1024)

/**************************************************************************************************/

static void
IFusionSoundBuffer_Requestor_Destruct( IFusionSoundBuffer *thiz )
{
     IFusionSoundBuffer_Requestor_data *data = thiz->priv;
     
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );
     
     if (data->buffer)
          D_FREE( data->buffer );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IFusionSoundBuffer_Requestor_AddRef( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)

     data->ref++;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundBuffer_Requestor_Release( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)

     if (--data->ref == 0)
          IFusionSoundBuffer_Requestor_Destruct( thiz );

     return DFB_UNIMPLEMENTED;
}


static DFBResult
IFusionSoundBuffer_Requestor_GetDescription( IFusionSoundBuffer  *thiz,
                                             FSBufferDescription *desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (!desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDBUFFER_METHOD_ID_GetDescription,
                                   VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, desc, sizeof(FSBufferDescription) );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundBuffer_Requestor_SetPosition( IFusionSoundBuffer *thiz,
                                          int                 position )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (position < 0 || position >= data->frames)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDBUFFER_METHOD_ID_SetPosition, VREQ_RESPOND, &response,
                                   VMBT_INT, position,
                                   VMBT_NONE );
     if (ret)
          return ret;
          
     ret = response->result;
     if (ret == DFB_OK)
          data->position = position;
          
     voodoo_manager_finish_request( data->manager, response );
    
     return ret;
}

static DFBResult
IFusionSoundBuffer_Requestor_Lock( IFusionSoundBuffer  *thiz,
                                   void               **ret_data,
                                   int                 *ret_frames,
                                   int                 *ret_bytes )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (!ret_data)
          return DFB_INVARG;
     
     if (data->locked)
          return DFB_LOCKED;
          
     *ret_data = data->buffer;
     
     if (ret_frames)
          *ret_frames = data->frames;
          
     if (ret_bytes)
          *ret_bytes = data->frames * data->bytes_per_frame;

     data->locked = true;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Requestor_Unlock( IFusionSoundBuffer *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     u8                    *src;
     int                    offset;
     int                    length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (!data->locked)
          return DFB_OK;
          
     src = data->buffer + data->position * data->bytes_per_frame;
     offset = 0;
     length = data->frames - data->position;
     while (length) {
          int len  = MIN( length, MAX_DATA_SIZE/data->bytes_per_frame );
          int size = len * data->bytes_per_frame;
          
          ret = voodoo_manager_request( data->manager, data->instance,
                                        IFUSIONSOUNDBUFFER_METHOD_ID_Unlock, VREQ_RESPOND, &response,
                                        VMBT_DATA, size, src+offset,
                                        VMBT_INT, offset,
                                        VMBT_INT, len,
                                        VMBT_NONE );
          if (ret)
               return ret;

          ret = response->result;

          voodoo_manager_finish_request( data->manager, response );
          
          if (ret)
               return ret;
          
          offset += size;
          length -= len;
     }
     
     data->locked = false;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Requestor_Unlock_DPACK( IFusionSoundBuffer *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     u8                    *src;
     int                    offset;
     int                    length;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (!data->locked)
          return DFB_OK;
          
     src = data->buffer + data->position * data->bytes_per_frame;
     offset = 0;
     length = data->frames - data->position;
     while (length) {
          int len = MIN( length, MAX_DATA_SIZE/data->bytes_per_frame );
          u8  buf[DPACK_MAX_PACKET(len, data->channels, data->bytes_per_frame)];
          int size;
          
          size = dpack_encode( src+offset, data->format, data->channels, len, buf );
          D_ASSERT( size <= sizeof(buf) );
          
          ret = voodoo_manager_request( data->manager, data->instance,
                                        IFUSIONSOUNDBUFFER_METHOD_ID_Unlock_DPACK, VREQ_RESPOND, &response,
                                        VMBT_DATA, size, buf,
                                        VMBT_INT, offset,
                                        VMBT_INT, len,
                                        VMBT_NONE );
          if (ret)
               return ret;

          ret = response->result;

          voodoo_manager_finish_request( data->manager, response );
          
          if (ret)
               return ret;
          
          offset += len * data->bytes_per_frame;
          length -= len;
     }
     
     data->locked = false;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Requestor_Play( IFusionSoundBuffer *thiz,
                                   FSBufferPlayFlags   flags )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (flags & ~FSPLAY_ALL)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDBUFFER_METHOD_ID_Play, VREQ_RESPOND, &response,
                                   VMBT_UINT, flags,
                                   VMBT_NONE );
     if (ret)
          return ret;
          
     ret = response->result;
          
     voodoo_manager_finish_request( data->manager, response );
    
     return ret;
}

static DFBResult
IFusionSoundBuffer_Requestor_Stop( IFusionSoundBuffer *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDBUFFER_METHOD_ID_Stop, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;
          
     ret = response->result;
          
     voodoo_manager_finish_request( data->manager, response );
    
     return ret;
}

static DFBResult
IFusionSoundBuffer_Requestor_CreatePlayback( IFusionSoundBuffer    *thiz,
                                             IFusionSoundPlayback **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundBuffer_Requestor)
     
     if (!ret_interface)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDBUFFER_METHOD_ID_CreatePlayback, VREQ_RESPOND, &response,
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
           VoodooManager      *manager,
           VoodooInstanceID    instance,
           void               *arg )
{
     const FSBufferDescription *dsc = arg;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundBuffer_Requestor)

     data->ref = 1;
     data->manager = manager;
     data->instance = instance;
     data->frames = dsc->length;
     data->format = dsc->sampleformat;
     data->channels = dsc->channels;
     data->bytes_per_frame = dsc->channels * FS_BYTES_PER_SAMPLE(dsc->sampleformat);
     
     data->buffer = D_MALLOC( data->frames * data->bytes_per_frame );
     if (!data->buffer) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return D_OOM();
     }
     
     thiz->AddRef         = IFusionSoundBuffer_Requestor_AddRef;
     thiz->Release        = IFusionSoundBuffer_Requestor_Release;
     thiz->GetDescription = IFusionSoundBuffer_Requestor_GetDescription; 
     thiz->SetPosition    = IFusionSoundBuffer_Requestor_SetPosition;
     thiz->Lock           = IFusionSoundBuffer_Requestor_Lock;
     thiz->Unlock         = fs_config->remote_compression
                            ? IFusionSoundBuffer_Requestor_Unlock_DPACK
                            : IFusionSoundBuffer_Requestor_Unlock;
     thiz->Play           = IFusionSoundBuffer_Requestor_Play;
     thiz->Stop           = IFusionSoundBuffer_Requestor_Stop;
     thiz->CreatePlayback = IFusionSoundBuffer_Requestor_CreatePlayback;

     return DFB_OK;
}


