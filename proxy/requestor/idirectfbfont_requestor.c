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

#include <idirectfbfont_dispatcher.h>

#include "idirectfbfont_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBFont    *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance,
                            void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFont, Requestor )


/**************************************************************************************************/

static void
IDirectFBFont_Requestor_Destruct( IDirectFBFont *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBFont_Requestor_AddRef( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Requestor_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (--data->ref == 0)
          IDirectFBFont_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Requestor_GetAscender( IDirectFBFont *thiz, int *ascender )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Requestor_GetDescender( IDirectFBFont *thiz, int *descender )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!descender)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Requestor_GetHeight( IDirectFBFont *thiz,
                                   int           *ret_height )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     int                    height;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!ret_height)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBFONT_METHOD_ID_GetHeight, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, height );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_height = height;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Requestor_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!maxadvance)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Requestor_GetKerning( IDirectFBFont *thiz,
                                    unsigned int prev_index, unsigned int current_index,
                                    int *kern_x, int *kern_y)
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!kern_x && !kern_y)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Requestor_GetStringExtents( IDirectFBFont *thiz,
                                          const char *text, int bytes,
                                          DFBRectangle *logical_rect,
                                          DFBRectangle *ink_rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!text || (!logical_rect && !ink_rect))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Requestor_GetStringWidth( IDirectFBFont *thiz,
                                        const char    *text,
                                        int            bytes,
                                        int           *ret_width )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     int                    width;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!text || !ret_width)
          return DFB_INVARG;


     if (bytes < 0)
          bytes = strlen (text);

     if (bytes == 0)
          return DFB_OK;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBFONT_METHOD_ID_GetStringWidth, VREQ_RESPOND, &response,
                                   VMBT_DATA, bytes, text,
                                   VMBT_INT, bytes,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, width );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_width = width;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Requestor_GetGlyphExtents( IDirectFBFont *thiz,
                                         unsigned int   index,
                                         DFBRectangle  *rect,
                                         int           *ret_advance )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     const DFBRectangle    *extents;
     int                    advance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Requestor)

     if (!rect && !ret_advance)
          return DFB_INVARG;


     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBFONT_METHOD_ID_GetGlyphExtents, VREQ_RESPOND, &response,
                                   VMBT_UINT, index,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_DATA( parser, extents );
     VOODOO_PARSER_GET_INT( parser, advance );
     VOODOO_PARSER_END( parser );

     if (rect)
          *rect = *extents;

     if (advance)
          *ret_advance = advance;

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBFont    *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFont_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef           = IDirectFBFont_Requestor_AddRef;
     thiz->Release          = IDirectFBFont_Requestor_Release;
     thiz->GetAscender      = IDirectFBFont_Requestor_GetAscender;
     thiz->GetDescender     = IDirectFBFont_Requestor_GetDescender;
     thiz->GetHeight        = IDirectFBFont_Requestor_GetHeight;
     thiz->GetMaxAdvance    = IDirectFBFont_Requestor_GetMaxAdvance;
     thiz->GetKerning       = IDirectFBFont_Requestor_GetKerning;
     thiz->GetStringWidth   = IDirectFBFont_Requestor_GetStringWidth;
     thiz->GetStringExtents = IDirectFBFont_Requestor_GetStringExtents;
     thiz->GetGlyphExtents  = IDirectFBFont_Requestor_GetGlyphExtents;

     return DFB_OK;
}

