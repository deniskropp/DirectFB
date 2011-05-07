/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/surface.h>

#include <core/CoreDFB.h>
#include <core/CoreDFB_internal.h>


D_DEBUG_DOMAIN( DirectFB_Core, "DirectFB/Core", "DirectFB Core" );

/******************************************************************************/

static DFBSurfaceID
CoreDFB_Dispatch_CreateSurface( CoreDFB              *core,
                                CoreDFBCreateSurface *create_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( DirectFB_Core, "%s( %p )\n", __FUNCTION__, core );

     D_MAGIC_ASSERT( core, CoreDFB );

     ret = dfb_surface_create( core, &create_surface->config, create_surface->type, create_surface->resource_id, NULL /*FIXME*/, &surface );
     if (ret)
          return 0;

     return surface->object.id;
}

FusionCallHandlerResult
CoreDFB_Dispatch( int           caller,   /* fusion id of the caller */
                  int           call_arg, /* optional call parameter */
                  void         *call_ptr, /* optional call parameter */
                  void         *ctx,      /* optional handler context */
                  unsigned int  serial,
                  int          *ret_val )
{
     CoreDFB *core = ctx;

     D_MAGIC_ASSERT( core, CoreDFB );

     switch (call_arg) {
          case CORE_DFB_CREATE_SURFACE:
               D_DEBUG_AT( DirectFB_Core, "=-> CORE_DFB_CREATE_SURFACE\n" );

               *ret_val = CoreDFB_Dispatch_CreateSurface( ctx, call_ptr );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

/******************************************************************************/

DFBResult
CoreDFB_CreateSurface( CoreDFB                  *core,
                       const CoreSurfaceConfig  *config,
                       CoreSurfaceTypeFlags      type,
                       unsigned long             resource_id,
                       CorePalette              *palette,
                       CoreSurface             **ret_surface )
{
     DFBResult    ret;
     int          val;
     CoreSurface *surface;

     D_MAGIC_ASSERT( core, CoreDFB );

     if (palette)
          D_UNIMPLEMENTED();

     CoreDFBCreateSurface create_surface;

     create_surface.config      = *config;
     create_surface.type        = type;
     create_surface.resource_id = resource_id;

     ret = fusion_call_execute2( &core->shared->call, FCEF_NONE, CORE_DFB_CREATE_SURFACE, &create_surface, sizeof(create_surface), &val );
     if (ret) {
          D_DERROR( ret, "Core/DFB: fusion_call_execute2( CORE_DFB_CREATE_SURFACE ) failed!\n" );
          return ret;
     }

     if (!val) {
          D_ERROR( "Core/DFB: CORE_DFB_CREATE_SURFACE failed!\n" );
          return DFB_FAILURE;
     }

     ret = dfb_core_get_surface( core, val, &surface );
     if (ret) {
          D_DERROR( ret, "Core/DFB: Looking up surface by ID %u failed!\n", (DFBSurfaceID) val );
          return ret;
     }

     *ret_surface = surface;

     return DFB_OK;
}

