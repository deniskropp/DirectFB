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

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/surfaces.h>

#include <unique/context.h>
#include <unique/uniquewm.h>
#include <unique/internal.h>

#include <unique/data/foo.h>


D_DEBUG_DOMAIN( UniQuE_WM, "UniQuE/WM", "UniQuE - Universal Quark Emitter" );

/**************************************************************************************************/

static CoreDFB  *dfb_core;
static WMData   *wm_data;
static WMShared *wm_shared;

/**************************************************************************************************/

static const StretRegionClass *region_classes[_UCI_NUM] = {
     &unique_root_region_class,
     &unique_frame_region_class,
     &unique_window_region_class,
     &unique_foo_region_class
};

static DFBResult register_classes( WMShared *shared,
                                   bool      master );

static DFBResult unregister_classes( WMShared *shared );

/**************************************************************************************************/

static DFBResult
load_foo( CoreDFB *core, WMShared *shared )
{
     int        i;
     DFBResult  ret;
     void      *data;
     int        pitch;

     ret = dfb_surface_create( core, foo_desc.width, foo_desc.height, foo_desc.pixelformat,
                               CSP_VIDEOHIGH, DSCAPS_NONE, NULL, &shared->foo_surface );
     if (ret) {
          D_DERROR( ret, "UniQuE/WM: Could not create %dx%d surface for border tiles!\n",
                    foo_desc.width, foo_desc.height );
          return ret;
     }

     ret = dfb_surface_soft_lock( shared->foo_surface, DSLF_WRITE, &data, &pitch, false );
     if (ret) {
          D_DERROR( ret, "UniQuE/WM: Could not lock surface for border tiles!\n" );
          dfb_surface_unref( shared->foo_surface );
          return ret;
     }

     for (i=0; i<foo_desc.height; i++) {
          direct_memcpy( dfb_surface_data_offset( shared->foo_surface, data, pitch, 0, i ),
                         foo_data + i * foo_desc.preallocated[0].pitch,
                         DFB_BYTES_PER_LINE( foo_desc.pixelformat, foo_desc.width ) );
     }

     dfb_surface_unlock( shared->foo_surface, false );

     dfb_surface_globalize( shared->foo_surface );

     return DFB_OK;
}

DFBResult
unique_wm_module_init( CoreDFB *core, WMData *data, WMShared *shared, bool master )
{
     DFBResult ret;

     D_DEBUG_AT( UniQuE_WM, "unique_wm_init( core %p, data %p, shared %p, %s )\n",
                 core, data, shared, master ? "master" : "slave" );

     D_ASSERT( core != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     D_ASSERT( dfb_core == NULL );
     D_ASSERT( wm_data == NULL );
     D_ASSERT( wm_shared == NULL );

     if (data->module_abi != UNIQUE_WM_ABI_VERSION) {
          D_ERROR( "UniQuE/WM: Module ABI version (%d) does not match %d!\n",
                   data->module_abi, UNIQUE_WM_ABI_VERSION );
          return DFB_VERSIONMISMATCH;
     }

     ret = register_classes( shared, master );
     if (ret)
          return ret;

     if (master) {
          int i;

          ret = load_foo( core, shared );
          if (ret) {
               unregister_classes( wm_shared );
               return ret;
          }

          shared->context_pool = unique_context_pool_create();

          shared->insets.l = foo[UFI_W].rect.w;
          shared->insets.t = foo[UFI_N].rect.h;
          shared->insets.r = foo[UFI_E].rect.w;
          shared->insets.b = foo[UFI_S].rect.h;

          for (i=0; i<8; i++)
               shared->foo_rects[i] = foo[i].rect;
     }

     dfb_core  = core;
     wm_data   = data;
     wm_shared = shared;

     return DFB_OK;
}

void
unique_wm_module_deinit( bool master, bool emergency )
{
     D_DEBUG_AT( UniQuE_WM, "unique_wm_deinit( %s%s ) <- core %p, data %p, shared %p\n",
                 master ? "master" : "slave", emergency ? ", emergency" : "",
                 dfb_core, wm_data, wm_shared );

     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_shared != NULL );

     if (master)
          fusion_object_pool_destroy( wm_shared->context_pool );

     unregister_classes( wm_shared );

     dfb_core  = NULL;
     wm_data   = NULL;
     wm_shared = NULL;
}

UniqueContext *
unique_wm_create_context()
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_shared != NULL );
     D_ASSERT( wm_shared->context_pool != NULL );

     return (UniqueContext*) fusion_object_create( wm_shared->context_pool );
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

FusionResult
unique_wm_enum_contexts( FusionObjectCallback  callback,
                         void                 *ctx )
{
     D_ASSERT( dfb_core != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( wm_shared != NULL );
     D_ASSERT( wm_shared->context_pool != NULL );

     return fusion_object_pool_enum( wm_shared->context_pool, callback, ctx );
}

/**************************************************************************************************/

static DFBResult
register_classes( WMShared *shared,
                  bool      master )
{
     int                i;
     DFBResult          ret;
     StretRegionClassID class_id;

     D_ASSERT( shared != NULL );

     for (i=0; i<_UCI_NUM; i++) {
          ret = stret_class_register( region_classes[i], &class_id );
          if (ret) {
               D_DERROR( ret, "UniQuE/WM: Failed to register region class %d!\n", i );

               goto error;
          }

          if (master)
               shared->classes[i] = class_id;
          else if (shared->classes[i] != class_id) {
               D_ERROR( "UniQuE/WM: Class IDs mismatch (%d/%d)!\n", class_id, shared->classes[i] );

               stret_class_unregister( class_id );

               ret = DFB_VERSIONMISMATCH;
               goto error;
          }
     }

     return DFB_OK;

error:
     while (--i >= 0)
          stret_class_unregister( shared->classes[i] );

     return ret;
}

static DFBResult
unregister_classes( WMShared *shared )
{
     int i;

     D_ASSERT( shared != NULL );

     for (i=_UCI_NUM-1; i>=0; i--)
          stret_class_unregister( shared->classes[i] );

     return DFB_OK;
}

