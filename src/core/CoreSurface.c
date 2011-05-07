/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/surface_buffer.h>

#include <core/CoreSurface.h>
#include <core/CoreSurface_internal.h>


D_DEBUG_DOMAIN( Core_Surface, "Core/Surface", "DirectFB Core Surface" );

/*********************************************************************************************************************/

DFBResult
CoreSurface_SetConfig( CoreSurface             *surface,
                       const CoreSurfaceConfig *config,
                       CoreSurfaceConfigFlags   flags )
{
     DFBResult            ret;
     int                  val;
     CoreSurfaceSetConfig set_config;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( config != NULL );

     set_config.config = *config;
     set_config.flags  = flags;

     ret = dfb_surface_call( surface, CORE_SURFACE_SET_CONFIG, &set_config, sizeof(set_config), FCEF_NONE, &val );
     if (ret) {
          D_DERROR( ret, "%s: dfb_surface_call( CORE_SURFACE_SET_CONFIG ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return val;
}

DFBResult
CoreSurface_LockBuffer( CoreSurface             *surface,
                        CoreSurfaceBufferRole    role,
                        CoreSurfaceAccessorID    accessor,
                        CoreSurfaceAccessFlags   access,
                        CoreSurfaceBufferLock   *ret_lock )
{
     DFBResult             ret;
     int                   val;
     CoreSurfaceLockBuffer lock_buffer;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( ret_lock != NULL );

     if (accessor != CSAID_CPU) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     lock_buffer.role   = role;
     lock_buffer.access = access;

     ret = dfb_surface_call( surface, CORE_SURFACE_LOCK_BUFFER, &lock_buffer, sizeof(lock_buffer), FCEF_NONE, &val );
     if (ret) {
          D_DERROR( ret, "%s: dfb_surface_call( CORE_SURFACE_LOCK_BUFFER ) failed!\n", __FUNCTION__ );
          return ret;
     }

     if (!val) {
          D_ERROR( "%s: CORE_SURFACE_LOCK_BUFFER failed!\n", __FUNCTION__ );
          return DFB_FAILURE;
     }

     // FIXME: we need to add fusion_call_execute3 with return data

     ret_lock->addr  = (void*)((long) surface + val);
     ret_lock->pitch = DFB_BYTES_PER_LINE( surface->config.format, surface->config.size.w );   // FIXME

     return DFB_OK;
}

DFBResult
CoreSurface_UnlockBuffer( CoreSurface           *surface,
                          CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( lock != NULL );

     //D_UNIMPLEMENTED();

     return DFB_OK;
}

DFBResult
CoreSurface_Flip( CoreSurface *surface,
                  bool         swap )
{
     DFBResult       ret;
     CoreSurfaceFlip flip;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );

     flip.swap = swap;

     ret = dfb_surface_call( surface, CORE_SURFACE_FLIP, &flip, sizeof(flip), FCEF_ONEWAY, NULL );
     if (ret) {
          D_DERROR( ret, "%s: dfb_surface_call( CORE_SURFACE_FLIP ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return DFB_OK;
}

DFBResult
CoreSurface_SetPalette( CoreSurface *surface,
                        CorePalette *palette )
{
     DFBResult             ret;
     CoreSurfaceSetPalette set_palette;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );

     set_palette.object_id = palette->object.id;

     ret = dfb_surface_call( surface, CORE_SURFACE_SET_PALETTE, &set_palette, sizeof(set_palette), FCEF_ONEWAY, NULL );
     if (ret) {
          D_DERROR( ret, "%s: dfb_surface_call( CORE_SURFACE_SET_PALETTE ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return DFB_OK;
}

/*********************************************************************************************************************/

DirectResult
dfb_surface_call( CoreSurface         *surface,
                  CoreSurfaceCall      call,
                  void                *arg,
                  size_t               len,
                  FusionCallExecFlags  flags,
                  int                 *ret_val )
{
     return fusion_call_execute2( &surface->call, flags, call, arg, len, ret_val );
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/

static DFBResult
CoreSurface_Dispatch_SetConfig( CoreSurface          *surface,
                                CoreSurfaceSetConfig *set_config )
{
     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( set_config != NULL );

     CoreSurfaceConfig config = set_config->config;

     config.flags = set_config->flags;

     return dfb_surface_reconfig( surface, &config );
}

static DFBResult
CoreSurface_Dispatch_LockBuffer( CoreSurface           *surface,
                                 CoreSurfaceLockBuffer *lock_buffer )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( lock_buffer != NULL );

     CoreSurfaceBufferLock lock = { 0 };

     ret = dfb_surface_lock_buffer( surface, lock_buffer->role, CSAID_CPU, (lock_buffer->access & (CSAF_READ | CSAF_WRITE)) | CSAF_SHARED, &lock );
     if (ret)
          return 0;

     return (long) lock.addr - (long) surface;
}

static DFBResult
CoreSurface_Dispatch_UnlockBuffer( CoreSurface             *surface,
                                   CoreSurfaceUnlockBuffer *unlock_buffer )
{
     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( unlock_buffer != NULL );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

static DFBResult
CoreSurface_Dispatch_Flip( CoreSurface     *surface,
                           CoreSurfaceFlip *flip )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( flip != NULL );

     ret = dfb_surface_lock( surface );
     if (ret)
          return ret;

     ret = dfb_surface_flip( surface, flip->swap );

     dfb_surface_unlock( surface );

     return ret;
}

static DFBResult
CoreSurface_Dispatch_SetPalette( CoreSurface           *surface,
                                 CoreSurfaceSetPalette *set_palette )
{
     DFBResult    ret;
     CorePalette *palette;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( set_palette != NULL );

     CoreLayer *layer = dfb_layer_at(0); //HACK
     ret = dfb_core_get_palette( layer->core, set_palette->object_id, &palette );
     if (ret)
          return ret;

     ret = dfb_surface_set_palette( surface, palette );

     dfb_palette_unref( palette );

     return ret;
}

FusionCallHandlerResult
CoreSurface_Dispatch( int           caller,   /* fusion id of the caller */
                      int           call_arg, /* optional call parameter */
                      void         *call_ptr, /* optional call parameter */
                      void         *ctx,      /* optional handler context */
                      unsigned int  serial,
                      int          *ret_val )
{
     switch (call_arg) {
          case CORE_SURFACE_SET_CONFIG:
               D_DEBUG_AT( Core_Surface, "=-> CORE_SURFACE_SET_CONFIG\n" );

               *ret_val = CoreSurface_Dispatch_SetConfig( ctx, call_ptr );
               break;

          case CORE_SURFACE_LOCK_BUFFER:
               D_DEBUG_AT( Core_Surface, "=-> CORE_SURFACE_LOCK_BUFFER\n" );

               *ret_val = CoreSurface_Dispatch_LockBuffer( ctx, call_ptr );
               break;

          case CORE_SURFACE_UNLOCK_BUFFER:
               D_DEBUG_AT( Core_Surface, "=-> CORE_SURFACE_UNLOCK_BUFFER\n" );

               *ret_val = CoreSurface_Dispatch_UnlockBuffer( ctx, call_ptr );
               break;

          case CORE_SURFACE_FLIP:
               D_DEBUG_AT( Core_Surface, "=-> CORE_SURFACE_FLIP\n" );

               *ret_val = CoreSurface_Dispatch_Flip( ctx, call_ptr );
               break;

          case CORE_SURFACE_SET_PALETTE:
               D_DEBUG_AT( Core_Surface, "=-> CORE_SURFACE_SET_PALETTE\n" );

               *ret_val = CoreSurface_Dispatch_SetPalette( ctx, call_ptr );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

