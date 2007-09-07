/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/input.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <unique/context.h>
#include <unique/decoration.h>
#include <unique/internal.h>
#include <unique/uniquewm.h>

#include <unique/data/foo.h>


D_DEBUG_DOMAIN( UniQuE_WM, "UniQuE/WM", "UniQuE - Universal Quark Emitter" );

/**************************************************************************************************/

static CoreDFB  *dfb_core;
static WMData   *wm_data;
static WMShared *wm_shared;

/**************************************************************************************************/

static const StretRegionClass *region_classes[_URCI_NUM] = {
     &unique_root_region_class,
     &unique_frame_region_class,
     &unique_window_region_class,
     &unique_foo_region_class
};

static DFBResult register_region_classes  ( WMShared *shared,
                                            bool      master );

static DFBResult unregister_region_classes( WMShared *shared );

/**************************************************************************************************/

static const UniqueDeviceClass *device_classes[_UDCI_NUM] = {
     &unique_pointer_device_class,
     &unique_wheel_device_class,
     &unique_keyboard_device_class
};

static DFBResult register_device_classes  ( WMShared *shared,
                                            bool      master );

static DFBResult unregister_device_classes( WMShared *shared );

/**************************************************************************************************/

static DFBResult
load_foo( CoreDFB *core, WMShared *shared )
{
     int                   i;
     DFBResult             ret;
     CoreSurfaceBufferLock lock;

     D_ASSERT( core != NULL );

     D_MAGIC_ASSERT( shared, WMShared );

     ret = dfb_surface_create_simple( core, foo_desc.width, foo_desc.height, foo_desc.pixelformat,
                                      DSCAPS_NONE, CSTF_SHARED, 0, NULL, &shared->foo_surface );
     if (ret) {
          D_DERROR( ret, "UniQuE/WM: Could not create %dx%d surface for border tiles!\n",
                    foo_desc.width, foo_desc.height );
          return ret;
     }

     ret = dfb_surface_lock_buffer( shared->foo_surface, CSBR_BACK, CSAF_CPU_WRITE, &lock );
     if (ret) {
          D_DERROR( ret, "UniQuE/WM: Could not lock surface for border tiles!\n" );
          dfb_surface_unref( shared->foo_surface );
          return ret;
     }

     for (i=0; i<foo_desc.height; i++) {
          direct_memcpy( dfb_surface_data_offset( shared->foo_surface, lock.addr, lock.pitch, 0, i ),
                         foo_data + i * foo_desc.preallocated[0].pitch,
                         DFB_BYTES_PER_LINE( foo_desc.pixelformat, foo_desc.width ) );
     }

     dfb_surface_unlock_buffer( shared->foo_surface, &lock );

     dfb_surface_globalize( shared->foo_surface );

     return DFB_OK;
}

static void
unload_foo( WMShared *shared )
{
     D_MAGIC_ASSERT( shared, WMShared );

     dfb_surface_unlink( &shared->foo_surface );
}

/**************************************************************************************************/

DFBResult
unique_wm_module_init( CoreDFB *core, WMData *data, WMShared *shared, bool master )
{
     DFBResult ret;

     D_DEBUG_AT( UniQuE_WM, "unique_wm_init( core %p, data %p, shared %p, %s )\n",
                 core, data, shared, master ? "master" : "slave" );

     D_ASSERT( core != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( data->context_notify != NULL );
     D_ASSERT( data->window_notify != NULL );

     D_MAGIC_ASSERT( shared, WMShared );

     D_ASSERT( dfb_core == NULL );
     D_ASSERT( wm_data == NULL );
     D_ASSERT( wm_shared == NULL );

     if (data->module_abi != UNIQUE_WM_ABI_VERSION) {
          D_ERROR( "UniQuE/WM: Module ABI version (%d) does not match %d!\n",
                   data->module_abi, UNIQUE_WM_ABI_VERSION );
          return DFB_VERSIONMISMATCH;
     }

     ret = register_region_classes( shared, master );
     if (ret)
          return ret;

     ret = register_device_classes( shared, master );
     if (ret)
          goto error_device;

     if (master) {
          int i;

          ret = load_foo( core, shared );
          if (ret)
               goto error_foo;

          ret = dfb_input_add_global( _unique_device_listener, &shared->device_listener );
          if (ret)
               goto error_global;

          shared->context_pool    = unique_context_pool_create( data->world );
          shared->decoration_pool = unique_decoration_pool_create( data->world );
          shared->window_pool     = unique_window_pool_create( data->world );

          shared->insets.l = foo[UFI_W].rect.w;
          shared->insets.t = foo[UFI_N].rect.h;
          shared->insets.r = foo[UFI_E].rect.w;
          shared->insets.b = foo[UFI_S].rect.h;

          for (i=0; i<8; i++)
               shared->foo_rects[i] = foo[i].rect;
     }
     else
          dfb_input_set_global( _unique_device_listener, shared->device_listener );

     dfb_core  = core;
     wm_data   = data;
     wm_shared = shared;

     return DFB_OK;

error_global:
     unload_foo( shared );

error_foo:
     unregister_device_classes( shared );

error_device:
     unregister_region_classes( shared );

     return ret;
}

void
unique_wm_module_deinit( WMData *data, WMShared *shared, bool master, bool emergency )
{
     D_DEBUG_AT( UniQuE_WM, "unique_wm_deinit( %s%s ) <- core %p, data %p, shared %p\n",
                 master ? "master" : "slave", emergency ? ", emergency" : "",
                 dfb_core, wm_data, wm_shared );

     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_data == data );

     D_MAGIC_ASSERT( shared, WMShared );
     D_ASSERT( wm_shared == shared );

     if (master) {
          fusion_object_pool_destroy( shared->window_pool, data->world );
          fusion_object_pool_destroy( shared->decoration_pool, data->world );
          fusion_object_pool_destroy( shared->context_pool, data->world );
     }

     unregister_device_classes( shared );
     unregister_region_classes( shared );

//FIXME     unload_foo( wm_shared );

     dfb_core  = NULL;
     wm_data   = NULL;
     wm_shared = NULL;
}

ReactionResult
_unique_wm_module_context_listener( const void *msg_data,
                                    void       *ctx )
{
     const UniqueContextNotification *notification = msg_data;

     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_data->context_notify != NULL );

     D_ASSERT( notification != NULL );

     D_ASSERT( ! D_FLAGS_IS_SET( notification->flags, ~UCNF_ALL ) );

     D_DEBUG_AT( UniQuE_WM, "%s( context %p, flags 0x%08x )\n",
                 __FUNCTION__, notification->context, notification->flags );

     return wm_data->context_notify( wm_data, notification, ctx );
}

ReactionResult
_unique_wm_module_window_listener ( const void *msg_data,
                                    void       *ctx )
{
     const UniqueWindowNotification *notification = msg_data;

     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_data->window_notify != NULL );

     D_ASSERT( notification != NULL );

     D_ASSERT( ! D_FLAGS_IS_SET( notification->flags, ~UWNF_ALL ) );

     D_DEBUG_AT( UniQuE_WM, "%s( window %p, flags 0x%08x )\n",
                 __FUNCTION__, notification->window, notification->flags );

     return wm_data->window_notify( wm_data, notification, ctx );
}

UniqueContext *
unique_wm_create_context()
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( wm_shared, WMShared );
     D_ASSERT( wm_shared->context_pool != NULL );

     return (UniqueContext*) fusion_object_create( wm_shared->context_pool, wm_data->world );
}

UniqueDecoration *
unique_wm_create_decoration()
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( wm_shared, WMShared );
     D_ASSERT( wm_shared->decoration_pool != NULL );

     return (UniqueDecoration*) fusion_object_create( wm_shared->decoration_pool, wm_data->world );
}

UniqueWindow *
unique_wm_create_window()
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( wm_shared, WMShared );
     D_ASSERT( wm_shared->window_pool != NULL );

     return (UniqueWindow*) fusion_object_create( wm_shared->window_pool, wm_data->world );
}

/**************************************************************************************************/

bool
unique_wm_running()
{
     if (dfb_core) {
          if (!wm_data || !wm_shared) {
               D_BUG( "partly initialized module (%p,%p,%p)", dfb_core, wm_data, wm_shared );
               return false;
          }

          return true;
     }

     return false;
}

DirectResult
unique_wm_enum_contexts( FusionObjectCallback  callback,
                         void                 *ctx )
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( wm_shared, WMShared );
     D_ASSERT( wm_shared->context_pool != NULL );

     return fusion_object_pool_enum( wm_shared->context_pool, callback, ctx );
}

DirectResult
unique_wm_enum_windows( FusionObjectCallback  callback,
                        void                 *ctx )
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( wm_shared, WMShared );
     D_ASSERT( wm_shared->context_pool != NULL );

     return fusion_object_pool_enum( wm_shared->window_pool, callback, ctx );
}

/**************************************************************************************************/

static DFBResult
register_region_classes( WMShared *shared,
                         bool      master )
{
     int                i;
     DFBResult          ret;
     StretRegionClassID class_id;

     D_MAGIC_ASSERT( shared, WMShared );

     for (i=0; i<_URCI_NUM; i++) {
          ret = stret_class_register( region_classes[i], &class_id );
          if (ret) {
               D_DERROR( ret, "UniQuE/WM: Failed to register region class %d!\n", i );

               goto error;
          }

          if (master)
               shared->region_classes[i] = class_id;
          else if (shared->region_classes[i] != class_id) {
               D_ERROR( "UniQuE/WM: Class IDs mismatch (%d/%d)!\n",
                        class_id, shared->region_classes[i] );

               stret_class_unregister( class_id );

               ret = DFB_VERSIONMISMATCH;
               goto error;
          }
     }

     return DFB_OK;

error:
     while (--i >= 0)
          stret_class_unregister( shared->region_classes[i] );

     return ret;
}

static DFBResult
unregister_region_classes( WMShared *shared )
{
     int i;

     D_MAGIC_ASSERT( shared, WMShared );

     for (i=_URCI_NUM-1; i>=0; i--)
          stret_class_unregister( shared->region_classes[i] );

     return DFB_OK;
}

static DFBResult
register_device_classes( WMShared *shared,
                         bool      master )
{
     int                 i;
     DFBResult           ret;
     UniqueDeviceClassID class_id;

     D_MAGIC_ASSERT( shared, WMShared );

     for (i=0; i<_UDCI_NUM; i++) {
          ret = unique_device_class_register( device_classes[i], &class_id );
          if (ret) {
               D_DERROR( ret, "UniQuE/WM: Failed to register device class %d!\n", i );

               goto error;
          }

          if (master)
               shared->device_classes[i] = class_id;
          else if (shared->device_classes[i] != class_id) {
               D_ERROR( "UniQuE/WM: Class IDs mismatch (%d/%d)!\n",
                        class_id, shared->device_classes[i] );

               unique_device_class_unregister( class_id );

               ret = DFB_VERSIONMISMATCH;
               goto error;
          }
     }

     return DFB_OK;

error:
     while (--i >= 0)
          unique_device_class_unregister( shared->device_classes[i] );

     return ret;
}

static DFBResult
unregister_device_classes( WMShared *shared )
{
     int i;

     D_MAGIC_ASSERT( shared, WMShared );

     for (i=_UDCI_NUM-1; i>=0; i--)
          unique_device_class_unregister( shared->device_classes[i] );

     return DFB_OK;
}

