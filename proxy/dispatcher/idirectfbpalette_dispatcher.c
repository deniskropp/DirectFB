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

#include "idirectfbpalette_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBPalette *thiz,
                            IDirectFBPalette *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBPalette, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBPalette_Dispatcher
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBPalette    *real;

     VoodooInstanceID     super;
} IDirectFBPalette_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBPalette_Dispatcher_Destruct( IDirectFBPalette *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBPalette_Dispatcher_AddRef( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Dispatcher_Release( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     if (--data->ref == 0)
          IDirectFBPalette_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Dispatcher_GetCapabilities( IDirectFBPalette       *thiz,
                                             DFBPaletteCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBPalette_Dispatcher_GetSize( IDirectFBPalette *thiz,
                                     unsigned int     *size )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBPalette_Dispatcher_SetEntries( IDirectFBPalette *thiz,
                                        const DFBColor   *entries,
                                        unsigned int      num_entries,
                                        unsigned int      offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBPalette_Dispatcher_GetEntries( IDirectFBPalette *thiz,
                                        DFBColor         *entries,
                                        unsigned int      num_entries,
                                        unsigned int      offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBPalette_Dispatcher_FindBestMatch( IDirectFBPalette *thiz,
                                           __u8              r,
                                           __u8              g,
                                           __u8              b,
                                           __u8              a,
                                           unsigned int     *index )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBPalette_Dispatcher_CreateCopy( IDirectFBPalette  *thiz,
                                        IDirectFBPalette **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetCapabilities( IDirectFBPalette *thiz, IDirectFBPalette *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult           ret;
     DFBPaletteCapabilities caps;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     ret = real->GetCapabilities( real, &caps );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, caps,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetSize( IDirectFBPalette *thiz, IDirectFBPalette *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     unsigned int size;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     ret = real->GetSize( real, &size );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, size,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetEntries( IDirectFBPalette *thiz, IDirectFBPalette *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const DFBColor      *entries;
     unsigned int         num_entries;
     unsigned int         offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, entries );
     VOODOO_PARSER_GET_UINT( parser, num_entries );
     VOODOO_PARSER_GET_UINT( parser, offset );
     VOODOO_PARSER_END( parser );

     ret = real->SetEntries( real, entries, num_entries, offset );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetEntries( IDirectFBPalette *thiz, IDirectFBPalette *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     DFBColor            *entries;
     unsigned int         num_entries;
     unsigned int         offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, num_entries );
     VOODOO_PARSER_GET_UINT( parser, offset );
     VOODOO_PARSER_END( parser );

     entries = alloca( num_entries * sizeof(DFBColor) );
     if (!entries) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     ret = real->GetEntries( real, entries, num_entries, offset );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, num_entries * sizeof(DFBColor), entries,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_FindBestMatch( IDirectFBPalette *thiz, IDirectFBPalette *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const DFBColor      *color;
     unsigned int         index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, color );
     VOODOO_PARSER_END( parser );

     ret = real->FindBestMatch( real, color->r, color->g, color->b, color->a, &index );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, index,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateCopy( IDirectFBPalette *thiz, IDirectFBPalette *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBPalette/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBPALETTE_METHOD_ID_GetCapabilities:
               return Dispatch_GetCapabilities( dispatcher, real, manager, msg );

          case IDIRECTFBPALETTE_METHOD_ID_GetSize:
               return Dispatch_GetSize( dispatcher, real, manager, msg );

          case IDIRECTFBPALETTE_METHOD_ID_SetEntries:
               return Dispatch_SetEntries( dispatcher, real, manager, msg );

          case IDIRECTFBPALETTE_METHOD_ID_GetEntries:
               return Dispatch_GetEntries( dispatcher, real, manager, msg );

          case IDIRECTFBPALETTE_METHOD_ID_FindBestMatch:
               return Dispatch_FindBestMatch( dispatcher, real, manager, msg );

          case IDIRECTFBPALETTE_METHOD_ID_CreateCopy:
               return Dispatch_CreateCopy( dispatcher, real, manager, msg );
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
Construct( IDirectFBPalette *thiz,
           IDirectFBPalette *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBPalette_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref   = 1;
     data->real  = real;
     data->super = super;

     thiz->AddRef          = IDirectFBPalette_Dispatcher_AddRef;
     thiz->Release         = IDirectFBPalette_Dispatcher_Release;
     thiz->GetCapabilities = IDirectFBPalette_Dispatcher_GetCapabilities;
     thiz->GetSize         = IDirectFBPalette_Dispatcher_GetSize;
     thiz->SetEntries      = IDirectFBPalette_Dispatcher_SetEntries;
     thiz->GetEntries      = IDirectFBPalette_Dispatcher_GetEntries;
     thiz->FindBestMatch   = IDirectFBPalette_Dispatcher_FindBestMatch;
     thiz->CreateCopy      = IDirectFBPalette_Dispatcher_CreateCopy;

     return DFB_OK;
}

