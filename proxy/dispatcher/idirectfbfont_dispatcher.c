/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include "idirectfbfont_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBFont    *thiz,
                            IDirectFBFont    *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFont, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBFont_Dispatcher
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBFont       *real;
} IDirectFBFont_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBFont_Dispatcher_Destruct( IDirectFBFont *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBFont_Dispatcher_AddRef( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Dispatcher_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (--data->ref == 0)
          IDirectFBFont_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_Dispatcher_GetAscender( IDirectFBFont *thiz, int *ascender )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetDescender( IDirectFBFont *thiz, int *descender )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!descender)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetHeight( IDirectFBFont *thiz, int *height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!height)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!maxadvance)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetKerning( IDirectFBFont *thiz,
                                     unsigned int prev_index, unsigned int current_index,
                                     int *kern_x, int *kern_y)
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!kern_x && !kern_y)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetStringExtents( IDirectFBFont *thiz,
                                           const char *text, int bytes,
                                           DFBRectangle *logical_rect,
                                           DFBRectangle *ink_rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!text || (!logical_rect && !ink_rect))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetStringWidth( IDirectFBFont *thiz,
                                         const char *text, int bytes,
                                         int *width )
{
     if (!text || !width)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBFont_Dispatcher_GetGlyphExtents( IDirectFBFont *thiz,
                                          unsigned int   index,
                                          DFBRectangle  *rect,
                                          int           *advance )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     if (!rect && !advance)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetAscender( IDirectFBFont *thiz, IDirectFBFont *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          ascender;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     ret = real->GetAscender( real, &ascender );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, ascender,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetDescender( IDirectFBFont *thiz, IDirectFBFont *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          descender;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     ret = real->GetDescender( real, &descender );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, descender,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetHeight( IDirectFBFont *thiz, IDirectFBFont *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          height;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     ret = real->GetHeight( real, &height );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, height,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetMaxAdvance( IDirectFBFont *thiz, IDirectFBFont *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          max_advance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     ret = real->GetMaxAdvance( real, &max_advance );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, max_advance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetKerning( IDirectFBFont *thiz, IDirectFBFont *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     unsigned int         prev;
     unsigned int         next;
     int                  kern_x;
     int                  kern_y;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, prev );
     VOODOO_PARSER_GET_UINT( parser, next );
     VOODOO_PARSER_END( parser );

     ret = real->GetKerning( real, prev, next, &kern_x, &kern_y );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, kern_x,
                                    VMBT_INT, kern_y,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetStringWidth( IDirectFBFont *thiz, IDirectFBFont *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const char          *text;
     int                  bytes;
     int                  width;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, text );
     VOODOO_PARSER_GET_INT( parser, bytes );
     VOODOO_PARSER_END( parser );

     ret = real->GetStringWidth( real, text, bytes, &width );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, width,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetStringExtents( IDirectFBFont *thiz, IDirectFBFont *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const char          *text;
     int                  bytes;
     DFBRectangle         logical;
     DFBRectangle         ink;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, text );
     VOODOO_PARSER_GET_INT( parser, bytes );
     VOODOO_PARSER_END( parser );

     ret = real->GetStringExtents( real, text, bytes, &logical, &ink );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBRectangle), &logical,
                                    VMBT_DATA, sizeof(DFBRectangle), &ink,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetGlyphExtents( IDirectFBFont *thiz, IDirectFBFont *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     unsigned int        index;
     DFBRectangle        extents;
     int                 advance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_END( parser );

     ret = real->GetGlyphExtents( real, index, &extents, &advance );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(extents), &extents,
                                    VMBT_INT, advance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBFont/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBFONT_METHOD_ID_GetAscender:
               return Dispatch_GetAscender( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetDescender:
               return Dispatch_GetDescender( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetHeight:
               return Dispatch_GetHeight( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetMaxAdvance:
               return Dispatch_GetMaxAdvance( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetKerning:
               return Dispatch_GetKerning( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetStringWidth:
               return Dispatch_GetStringWidth( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetStringExtents:
               return Dispatch_GetStringExtents( dispatcher, real, manager, msg );

          case IDIRECTFBFONT_METHOD_ID_GetGlyphExtents:
               return Dispatch_GetGlyphExtents( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
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
           IDirectFBFont    *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFont_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref  = 1;
     data->real = real;

     thiz->AddRef           = IDirectFBFont_Dispatcher_AddRef;
     thiz->Release          = IDirectFBFont_Dispatcher_Release;
     thiz->GetAscender      = IDirectFBFont_Dispatcher_GetAscender;
     thiz->GetDescender     = IDirectFBFont_Dispatcher_GetDescender;
     thiz->GetHeight        = IDirectFBFont_Dispatcher_GetHeight;
     thiz->GetMaxAdvance    = IDirectFBFont_Dispatcher_GetMaxAdvance;
     thiz->GetKerning       = IDirectFBFont_Dispatcher_GetKerning;
     thiz->GetStringWidth   = IDirectFBFont_Dispatcher_GetStringWidth;
     thiz->GetStringExtents = IDirectFBFont_Dispatcher_GetStringExtents;
     thiz->GetGlyphExtents  = IDirectFBFont_Dispatcher_GetGlyphExtents;

     return DFB_OK;
}

