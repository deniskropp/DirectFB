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
                            VoodooManager        *manager,
                            VoodooInstanceID      instance,
                            void                 *arg );
                            
                            
#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundPlayback, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IFusionSoundPlayback_Requestor_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} IFusionSoundPlayback_Requestor_data;

/**************************************************************************************************/

static void
IFusionSoundPlayback_Requestor_Destruct( IFusionSoundPlayback *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IFusionSoundPlayback_Requestor_AddRef( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Requestor_Release( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)

     if (--data->ref == 0)
          IFusionSoundPlayback_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundPlayback_Requestor_Start( IFusionSoundPlayback *thiz,
                                      int                   start,
                                      int                   stop )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (start < 0)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_Start, VREQ_RESPOND, &response,
                                   VMBT_INT, start,
                                   VMBT_INT, stop,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}


static DFBResult
IFusionSoundPlayback_Requestor_Stop( IFusionSoundPlayback *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_Stop, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_Continue( IFusionSoundPlayback *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_Continue, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_Wait( IFusionSoundPlayback *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_Wait, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_GetStatus( IFusionSoundPlayback *thiz,
                                          DFBBoolean           *ret_playing,
                                          int                  *ret_position )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     DFBBoolean             playing  = DFB_FALSE;
     int                    position = 0; 
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (!ret_playing && !ret_position)
          return DFB_OK;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_GetStatus, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_INT( parser, playing );
          VOODOO_PARSER_GET_INT( parser, position );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );
     
     if (ret_playing)
          *ret_playing = playing;
          
     if (ret_position)
          *ret_position = position;

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_SetVolume( IFusionSoundPlayback *thiz,
                                          float                 level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (level < 0.0f || level > 64.f)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_SetVolume, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &level,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_SetPan( IFusionSoundPlayback *thiz,
                                       float                 value )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (value < -1.0f || value > 1.0f)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_SetPan, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &value,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_SetPitch( IFusionSoundPlayback *thiz,
                                         float                 value )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (value < 0.0f || value > 64.0f)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_SetPitch, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &value,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_SetDirection( IFusionSoundPlayback *thiz,
                                             FSPlaybackDirection   direction )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     switch (direction) {
          case FSPD_FORWARD:
          case FSPD_BACKWARD:
               break;
          default:
               return DFB_INVARG;
     }

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_SetDirection, VREQ_RESPOND, &response,
                                   VMBT_INT, direction,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSoundPlayback_Requestor_SetDownmixLevels( IFusionSoundPlayback *thiz,
                                       float                 center,
                                       float                 rear )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSoundPlayback_Requestor)
     
     if (center < 0.0f || center > 1.0f)
          return DFB_INVARG;
          
     if (rear < 0.0f || rear > 1.0f)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUNDPLAYBACK_METHOD_ID_SetDownmixLevels, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &center,
                                   VMBT_DATA, sizeof(float), &rear,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

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
Construct( IFusionSoundPlayback *thiz,
           VoodooManager        *manager,
           VoodooInstanceID      instance,
           void                 *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundPlayback_Requestor)

     data->ref = 1;
     data->manager = manager;
     data->instance = instance;
     
     thiz->AddRef           = IFusionSoundPlayback_Requestor_AddRef;
     thiz->Release          = IFusionSoundPlayback_Requestor_Release;
     thiz->Start            = IFusionSoundPlayback_Requestor_Start;
     thiz->Stop             = IFusionSoundPlayback_Requestor_Stop;
     thiz->Continue         = IFusionSoundPlayback_Requestor_Continue;
     thiz->Wait             = IFusionSoundPlayback_Requestor_Wait; 
     thiz->GetStatus        = IFusionSoundPlayback_Requestor_GetStatus;
     thiz->SetVolume        = IFusionSoundPlayback_Requestor_SetVolume;
     thiz->SetPan           = IFusionSoundPlayback_Requestor_SetPan;
     thiz->SetPitch         = IFusionSoundPlayback_Requestor_SetPitch;
     thiz->SetDirection     = IFusionSoundPlayback_Requestor_SetDirection;
     thiz->SetDownmixLevels = IFusionSoundPlayback_Requestor_SetDownmixLevels;

     return DFB_OK;
}

