/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <idirectfbpalette_dispatcher.h>

#include "idirectfbpalette_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBPalette *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance,
                            void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBPalette, Requestor )


/**************************************************************************************************/

static void
IDirectFBPalette_Requestor_Destruct( IDirectFBPalette *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBPalette_Requestor_AddRef( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_Release( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (--data->ref == 0)
          IDirectFBPalette_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_GetCapabilities( IDirectFBPalette       *thiz,
                                            DFBPaletteCapabilities *ret_caps )
{
     DirectResult            ret;
     VoodooResponseMessage  *response;
     VoodooMessageParser     parser;
     DFBPaletteCapabilities  caps;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!ret_caps)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBPALETTE_METHOD_ID_GetCapabilities, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, caps );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_caps = caps;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_GetSize( IDirectFBPalette *thiz,
                                    unsigned int     *ret_size )
{
     DirectResult            ret;
     VoodooResponseMessage  *response;
     VoodooMessageParser     parser;
     unsigned int            size;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!ret_size)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBPALETTE_METHOD_ID_GetSize, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, size );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_size = size;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_SetEntries( IDirectFBPalette *thiz,
                                       const DFBColor   *entries,
                                       unsigned int      num_entries,
                                       unsigned int      offset )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!entries)
          return DFB_INVARG;

     if (!num_entries)
          return DFB_OK;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBPALETTE_METHOD_ID_SetEntries, VREQ_RESPOND, &response,
                                   VMBT_DATA, num_entries * sizeof(DFBColor), entries,
                                   VMBT_UINT, num_entries,
                                   VMBT_UINT, offset,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBPalette_Requestor_GetEntries( IDirectFBPalette *thiz,
                                       DFBColor         *entries,
                                       unsigned int      num_entries,
                                       unsigned int      offset )
{
     DirectResult            ret;
     VoodooResponseMessage  *response;
     VoodooMessageParser     parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!entries)
          return DFB_INVARG;

     if (!num_entries)
          return DFB_OK;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBPALETTE_METHOD_ID_GetEntries, VREQ_RESPOND, &response,
                                   VMBT_UINT, num_entries,
                                   VMBT_UINT, offset,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, entries, num_entries * sizeof(DFBColor) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_FindBestMatch( IDirectFBPalette *thiz,
                                          __u8              r,
                                          __u8              g,
                                          __u8              b,
                                          __u8              a,
                                          unsigned int     *ret_index )
{
     DirectResult            ret;
     VoodooResponseMessage  *response;
     VoodooMessageParser     parser;
     unsigned int            index;
     DFBColor                color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!ret_index)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBPALETTE_METHOD_ID_FindBestMatch, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(color), &color,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_index = index;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Requestor_CreateCopy( IDirectFBPalette  *thiz,
                                       IDirectFBPalette **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBPalette *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBPalette_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef          = IDirectFBPalette_Requestor_AddRef;
     thiz->Release         = IDirectFBPalette_Requestor_Release;
     thiz->GetCapabilities = IDirectFBPalette_Requestor_GetCapabilities;
     thiz->GetSize         = IDirectFBPalette_Requestor_GetSize;
     thiz->SetEntries      = IDirectFBPalette_Requestor_SetEntries;
     thiz->GetEntries      = IDirectFBPalette_Requestor_GetEntries;
     thiz->FindBestMatch   = IDirectFBPalette_Requestor_FindBestMatch;
     thiz->CreateCopy      = IDirectFBPalette_Requestor_CreateCopy;

     return DFB_OK;
}

