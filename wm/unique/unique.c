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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>
#include <direct/util.h>

#include <fusion/reactor.h>
#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers_internal.h>
#include <core/surface.h>
#include <core/palette.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/wm_module.h>

#include <unique/context.h>
#include <unique/stret.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( WM_Unique, "WM/UniQuE", "UniQuE - Universal Quark Emitter" );


DFB_WINDOW_MANAGER( unique );

/**************************************************************************************************/

typedef struct {
     int                           magic;

     CoreWindowStack              *stack;

     UniqueContext                *context;

     GlobalReaction                context_reaction;
} StackData;

typedef struct {
     int                           magic;

     UniqueContext                *context;

     UniqueWindow                 *window;

     GlobalReaction                window_reaction;
} WindowData;

/**************************************************************************************************/

static ReactionResult
context_notify( WMData                          *data,
                const UniqueContextNotification *notification,
                void                            *ctx )
{
     StackData *stack_data = ctx;

     D_ASSERT( data != NULL );

     D_ASSERT( notification != NULL );
     D_ASSERT( notification->context != NULL );

     D_MAGIC_ASSERT( stack_data, StackData );

     D_ASSERT( notification->context == stack_data->context );

     D_MAGIC_ASSERT( stack_data->context, UniqueContext );

     D_ASSERT( ! D_FLAGS_IS_SET( notification->flags, ~UCNF_ALL ) );

     D_DEBUG_AT( WM_Unique, "context_notify( wm_data %p, stack_data %p )\n", data, stack_data );

     if (notification->flags & UCNF_DESTROYED) {
          D_DEBUG_AT( WM_Unique, "  -> context destroyed.\n" );

          stack_data->context = NULL;

          return RS_REMOVE;
     }

     return RS_OK;
}

static ReactionResult
window_notify( WMData                         *data,
               const UniqueWindowNotification *notification,
               void                           *ctx )
{
     WindowData *window_data = ctx;

     D_ASSERT( data != NULL );

     D_ASSERT( notification != NULL );
     D_ASSERT( notification->window != NULL );

     D_MAGIC_ASSERT( window_data, WindowData );

     D_ASSERT( notification->window == window_data->window );

     D_MAGIC_ASSERT( window_data->window, UniqueWindow );

     D_ASSERT( ! D_FLAGS_IS_SET( notification->flags, ~UWNF_ALL ) );

     D_DEBUG_AT( WM_Unique, "window_notify( wm_data %p, window_data %p )\n", data, window_data );

     if (notification->flags & UWNF_DESTROYED) {
          D_DEBUG_AT( WM_Unique, "  -> window destroyed.\n" );

          window_data->window = NULL;

          return RS_REMOVE;
     }

     return RS_OK;
}

/**************************************************************************************************/

static void
initialize_data( CoreDFB *core, WMData *data, WMShared *shared )
{
     D_ASSERT( data != NULL );

     /* Initialize local data. */
     data->core       = core;
     data->world      = dfb_core_world( core );
     data->shared     = shared;
     data->module_abi = UNIQUE_WM_ABI_VERSION;

     /* Set module callbacks. */
     data->context_notify = context_notify;
     data->window_notify  = window_notify;
}

/**************************************************************************************************/

static void
wm_get_info( CoreWMInfo *info )
{
     info->version.major  = 0;
     info->version.minor  = 4;
     info->version.binary = UNIQUE_WM_ABI_VERSION;

     snprintf( info->name, DFB_CORE_WM_INFO_NAME_LENGTH, "UniQuE" );
     snprintf( info->vendor, DFB_CORE_WM_INFO_VENDOR_LENGTH, "Denis Oliver Kropp" );

     info->wm_data_size     = sizeof(WMData);
     info->wm_shared_size   = sizeof(WMShared);
     info->stack_data_size  = sizeof(StackData);
     info->window_data_size = sizeof(WindowData);
}

static DFBResult
wm_initialize( CoreDFB *core, void *wm_data, void *shared_data )
{
     WMData    *data   = wm_data;
     WMShared  *shared = shared_data;

     D_DEBUG_AT( WM_Unique, "wm_initialize()\n" );

     initialize_data( core, data, shared );

     D_MAGIC_SET( shared, WMShared );

     return unique_wm_module_init( core, data, shared, true );
}

static DFBResult
wm_join( CoreDFB *core, void *wm_data, void *shared_data )
{
     WMData    *data   = wm_data;
     WMShared  *shared = shared_data;

     D_DEBUG_AT( WM_Unique, "wm_join()\n" );

     initialize_data( core, data, shared );

     return unique_wm_module_init( core, data, shared, false );
}

static DFBResult
wm_shutdown( bool emergency, void *wm_data, void *shared_data )
{
     WMShared *shared = shared_data;

     (void) shared;

     D_DEBUG_AT( WM_Unique, "wm_shutdown()\n" );

     unique_wm_module_deinit( wm_data, shared_data, true, emergency );

     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
wm_leave( bool emergency, void *wm_data, void *shared_data )
{
     D_DEBUG_AT( WM_Unique, "wm_leave()\n" );

     unique_wm_module_deinit( wm_data, shared_data, false, emergency );

     return DFB_OK;
}

static DFBResult
wm_suspend( void *wm_data, void *shared_data )
{
     D_DEBUG_AT( WM_Unique, "wm_suspend()\n" );

     return DFB_OK;
}

static DFBResult
wm_resume( void *wm_data, void *shared_data )
{
     D_DEBUG_AT( WM_Unique, "wm_resume()\n" );

     return DFB_OK;
}

static DFBResult
wm_post_init( void *wm_data, void *shared_data )
{
     D_DEBUG_AT( WM_Unique, "wm_post_init()\n" );

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
wm_init_stack( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     DFBResult         ret;
     StackData        *data   = stack_data;
     WMData           *wmdata = wm_data;
     CoreLayerContext *context;
     CoreLayerRegion  *region;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     context = stack->context;

     D_ASSERT( context != NULL );

     ret = dfb_layer_context_get_primary_region( context, true, &region );
     if (ret) {
          D_DERROR( ret, "WM/UniQuE: Could not get the primary region!\n" );
          return ret;
     }

     /* Create the unique context. */
     ret = unique_context_create( wmdata->core, stack, region, context->layer_id,
                                  wmdata->shared, &data->context );
     dfb_layer_region_unref( region );
     if (ret) {
          D_DERROR( ret, "WM/UniQuE: Could not create the context!\n" );
          return ret;
     }

     /* Attach the global context listener. */
     ret = unique_context_attach_global( data->context,
                                         UNIQUE_WM_MODULE_CONTEXT_LISTENER,
                                         data, &data->context_reaction );
     if (ret) {
          unique_context_unref( data->context );
          D_DERROR( ret, "WM/UniQuE: Could not attach global context listener!\n" );
          return ret;
     }

     /* Inherit all local references from the layer context. */
     ret = unique_context_inherit( data->context, context );
     unique_context_unref( data->context );
     if (ret) {
          unique_context_detach_global( data->context, &data->context_reaction );
          D_DERROR( ret, "WM/UniQuE: Could not inherit from layer context!\n" );
          return ret;
     }



     data->stack = stack;

     D_MAGIC_SET( data, StackData );

     return DFB_OK;
}

static DFBResult
wm_close_stack( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     D_ASSUME( data->context == NULL );

     if (data->context)
          unique_context_detach_global( data->context, &data->context_reaction );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
wm_set_active( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data,
               bool             active )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context) {
          D_ASSERT( !active );
          return DFB_OK;
     }

     return unique_context_set_active( data->context, active );
}

static DFBResult
wm_resize_stack( CoreWindowStack *stack,
                 void            *wm_data,
                 void            *stack_data,
                 int              width,
                 int              height )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     return unique_context_resize( data->context, width, height );
}

static DFBResult
wm_process_input( CoreWindowStack     *stack,
                  void                *wm_data,
                  void                *stack_data,
                  const DFBInputEvent *event )
{
     StackData *data = stack_data;

     (void) data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( data, StackData );

     return DFB_OK;
}

static DFBResult
wm_flush_keys( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     return unique_context_flush_keys( data->context );
}

static DFBResult
wm_window_at( CoreWindowStack  *stack,
              void             *wm_data,
              void             *stack_data,
              int               x,
              int               y,
              CoreWindow      **ret_window )
{
     DFBResult     ret;
     UniqueWindow *window;
     StackData    *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     ret = unique_context_window_at( data->context, x, y, &window );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( window, UniqueWindow );

     *ret_window = window->window;

     return DFB_OK;
}

static DFBResult
wm_window_lookup( CoreWindowStack  *stack,
                  void             *wm_data,
                  void             *stack_data,
                  DFBWindowID       window_id,
                  CoreWindow      **ret_window )
{
     DFBResult     ret;
     UniqueWindow *window;
     StackData    *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     ret = unique_context_lookup_window( data->context, window_id, &window );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( window, UniqueWindow );

     *ret_window = window->window;

     return DFB_OK;
}

static DFBResult
wm_enum_windows( CoreWindowStack      *stack,
                 void                 *wm_data,
                 void                 *stack_data,
                 CoreWMWindowCallback  callback,
                 void                 *callback_ctx )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     return unique_context_enum_windows( data->context, callback, callback_ctx );
}

/**************************************************************************************************/

static DFBResult
wm_get_insets( CoreWindowStack *stack,
               CoreWindow      *window,
               DFBInsets       *insets )
{
     if( insets ) {
         insets->l=0;
         insets->t=0;
         insets->r=0;
         insets->b=0;
     }
     return DFB_OK;
}

static DFBResult
wm_preconfigure_window( CoreWindowStack *stack,
                        void            *wm_data,
                        void            *stack_data,
                        CoreWindow      *window,
                        void            *window_data )
{
     return DFB_OK;
}

static DFBResult
wm_set_window_property( CoreWindowStack  *stack,
                        void             *wm_data,
                        void             *stack_data,
                        CoreWindow       *window,
                        void             *window_data,
                        const char       *key,
                        void             *value,
                        void            **ret_old_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );

     fusion_object_set_property((FusionObject*)window,
                     key,value,ret_old_value);
     return DFB_OK;
}

static DFBResult
wm_get_window_property( CoreWindowStack  *stack,
                        void             *wm_data,
                        void             *stack_data,
                        CoreWindow       *window,
                        void             *window_data,
                        const char       *key,
                        void            **ret_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );
     D_ASSERT( ret_value != NULL );

     *ret_value=fusion_object_get_property((FusionObject*)window,key);
     return DFB_OK;
}


static DFBResult
wm_remove_window_property( CoreWindowStack *stack,
                           void            *wm_data,
                           void            *stack_data,
                           CoreWindow      *window,
                           void            *window_data,
                           const char      *key,
                           void           **ret_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );

     fusion_object_remove_property((FusionObject*)window,key,ret_value);
     return DFB_OK;
}

static DFBResult
wm_add_window( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data,
               CoreWindow      *window,
               void            *window_data )
{
     DFBResult   ret;
     StackData  *sdata  = stack_data;
     WindowData *data   = window_data;
     WMData     *wmdata = wm_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sdata, StackData );
     D_MAGIC_ASSERT( sdata->context, UniqueContext );

     data->context = sdata->context;

     /* Create the unique window. */
     ret = unique_window_create( wmdata->core, window, data->context,
                                 window->caps, &window->config, &data->window );
     if (ret) {
          D_DERROR( ret, "WM/UniQuE: Could not create window!\n" );
          return ret;
     }

     /* Attach the global window listener. */
     ret = unique_window_attach_global( data->window,
                                        UNIQUE_WM_MODULE_WINDOW_LISTENER,
                                        data, &data->window_reaction );
     if (ret) {
          unique_window_unref( data->window );
          D_DERROR( ret, "WM/UniQuE: Could not attach global window listener!\n" );
          return ret;
     }

     /* Inherit all local references from the layer window. */
     ret = unique_window_inherit( data->window, window );
     unique_window_unref( data->window );
     if (ret) {
          unique_window_detach_global( data->window, &data->window_reaction );
          D_DERROR( ret, "WM/UniQuE: Could not inherit from core window!\n" );
          return ret;
     }

     unique_window_get_config( data->window, &window->config );


     D_MAGIC_SET( data, WindowData );

     return DFB_OK;
}

static DFBResult
wm_remove_window( CoreWindowStack *stack,
                  void            *wm_data,
                  void            *stack_data,
                  CoreWindow      *window,
                  void            *window_data )
{
     WindowData *data = window_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( data, WindowData );

//     D_ASSUME( data->window == NULL );

     if (data->window)
          unique_window_detach_global( data->window, &data->window_reaction );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
wm_set_window_config( CoreWindow             *window,
                      void                   *wm_data,
                      void                   *window_data,
                      const CoreWindowConfig *config,
                      CoreWindowConfigFlags   flags )
{
     DFBResult   ret;
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( config != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     if (!data->window)
          return DFB_DESTROYED;

     ret = unique_window_set_config( data->window, config, flags );

     unique_window_get_config( data->window, &window->config );

     return ret;
}

static DFBResult
wm_restack_window( CoreWindow             *window,
                   void                   *wm_data,
                   void                   *window_data,
                   CoreWindow             *relative,
                   void                   *relative_data,
                   int                     relation )
{
     WindowData *data     = window_data;
     WindowData *rel_data = relative_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     D_ASSERT( relative == NULL || relative_data != NULL );

     D_ASSERT( relative == NULL || relative == window || relation != 0);

     if (!data->window)
          return DFB_DESTROYED;

     return unique_window_restack( data->window, rel_data ? rel_data->window : NULL, relation );
}

static DFBResult
wm_grab( CoreWindow *window,
         void       *wm_data,
         void       *window_data,
         CoreWMGrab *grab )
{
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     if (!data->window)
          return DFB_DESTROYED;

     return unique_window_grab( data->window, grab );
}

static DFBResult
wm_ungrab( CoreWindow *window,
           void       *wm_data,
           void       *window_data,
           CoreWMGrab *grab )
{
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     if (!data->window)
          return DFB_DESTROYED;

     return unique_window_ungrab( data->window, grab );
}

static DFBResult
wm_request_focus( CoreWindow *window,
                  void       *wm_data,
                  void       *window_data )
{
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     if (!data->window)
          return DFB_DESTROYED;

     return unique_window_request_focus( data->window );
}

/**************************************************************************************************/

static DFBResult
wm_update_stack( CoreWindowStack     *stack,
                 void                *wm_data,
                 void                *stack_data,
                 const DFBRegion     *region,
                 DFBSurfaceFlipFlags  flags )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     DFB_REGION_ASSERT( region );

     D_MAGIC_ASSERT( data, StackData );

     if (!data->context)
          return DFB_DESTROYED;

     return unique_context_update( data->context, region, 1, flags );
}

static DFBResult
wm_update_window( CoreWindow          *window,
                  void                *wm_data,
                  void                *window_data,
                  const DFBRegion     *region,
                  DFBSurfaceFlipFlags  flags )
{
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     DFB_REGION_ASSERT_IF( region );

     D_MAGIC_ASSERT( data, WindowData );

     if (!data->window)
          return DFB_DESTROYED;

     return unique_window_update( data->window, region, flags );
}

/**************************************************************************************************/

/* HACK: implementation dumped in here for now, will move into context */
static DFBResult
wm_update_cursor( CoreWindowStack       *stack,
                  void                  *wm_data,
                  void                  *stack_data,
                  CoreCursorUpdateFlags  flags )
{
     DFBResult         ret;
     DFBRegion         old_region;
     WMData           *wmdata   = wm_data;
     StackData        *data     = stack_data;
     bool              restored = false;
     CoreLayer        *layer;
     CoreLayerRegion  *region;
     CardState        *state;
     CoreSurface      *surface;
     UniqueContext    *context;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     context = data->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     /* Optimize case of invisible cursor moving. */
     if (!(flags & ~(CCUF_POSITION | CCUF_SHAPE)) && (!stack->cursor.opacity || !stack->cursor.enabled)) {
          context->cursor_bs_valid = false;
          return DFB_OK;
     }

     layer   = dfb_layer_at( context->layer_id );
     state   = &layer->state;
     region  = context->region;
     surface = context->surface;

     D_ASSERT( region != NULL );
     D_ASSERT( surface != NULL );

     if (flags & CCUF_ENABLE) {
          CoreSurface *cursor_bs;

          D_ASSERT( context->cursor_bs == NULL );

          /* Create the cursor backing store surface. */
          ret = dfb_surface_create( wmdata->core, stack->cursor.size.w, stack->cursor.size.h,
                                    DSPF_RGB16, stack->cursor.policy, DSCAPS_NONE, NULL, &cursor_bs );
          if (ret) {
               D_ERROR( "WM/Default: Failed creating backing store for cursor!\n" );
               return ret;
          }

          ret = dfb_surface_globalize( cursor_bs );
          D_ASSERT( ret == DFB_OK );

          /* Ensure valid back buffer for now.
           * FIXME: Keep a flag to know when back/front have been swapped and need a sync.
           */
          switch (region->config.buffermode) {
               case DLBM_BACKVIDEO:
               case DLBM_TRIPLE:
                    dfb_gfx_copy( surface, surface, NULL );
                    break;

               default:
                    break;
          }

          context->cursor_bs = cursor_bs;
     }
     else {
          D_ASSERT( context->cursor_bs != NULL );

          /* restore region under cursor */
          if (context->cursor_drawn) {
               DFBRectangle rect = { 0, 0,
                                     context->cursor_region.x2 - context->cursor_region.x1 + 1,
                                     context->cursor_region.y2 - context->cursor_region.y1 + 1 };

               D_ASSERT( stack->cursor.opacity || (flags & CCUF_OPACITY) );
               D_ASSERT( context->cursor_bs_valid );

               dfb_gfx_copy_to( context->cursor_bs, surface, &rect,
                                context->cursor_region.x1, context->cursor_region.y1, false );

               context->cursor_drawn = false;

               old_region = context->cursor_region;
               restored   = true;
          }

          if (flags & CCUF_SIZE) {
               ret = dfb_surface_reformat( wmdata->core, context->cursor_bs,
                                           stack->cursor.size.w, stack->cursor.size.h,
                                           context->cursor_bs->format );
               if (ret) {
                    D_ERROR( "WM/Default: Failed resizing backing store for cursor!\n" );
                    return ret;
               }
          }
     }

     if (flags & (CCUF_ENABLE | CCUF_POSITION | CCUF_SIZE | CCUF_OPACITY)) {
          context->cursor_bs_valid  = false;

          context->cursor_region.x1 = stack->cursor.x - stack->cursor.hot.x;
          context->cursor_region.y1 = stack->cursor.y - stack->cursor.hot.y;
          context->cursor_region.x2 = context->cursor_region.x1 + stack->cursor.size.w - 1;
          context->cursor_region.y2 = context->cursor_region.y1 + stack->cursor.size.h - 1;

          if (!dfb_region_intersect( &context->cursor_region, 0, 0, stack->width - 1, stack->height - 1 )) {
               D_BUG( "invalid cursor region" );
               return DFB_BUG;
          }
     }

     D_ASSERT( context->cursor_bs != NULL );

     if (flags & CCUF_DISABLE) {
          dfb_surface_unlink( &context->cursor_bs );
     }
     else if (stack->cursor.opacity) {
          /* backup region under cursor */
          if (!context->cursor_bs_valid) {
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &context->cursor_region );

               D_ASSERT( !context->cursor_drawn );

               /* FIXME: this requires using blitted flipping all the time,
                  but fixing it seems impossible, for now DSFLIP_BLIT is forced
                  in repaint_stack() when the cursor is enabled. */
               dfb_gfx_copy_to( surface, context->cursor_bs, &rect, 0, 0, true );

               context->cursor_bs_valid = true;
          }

          /* Set destination. */
          state->destination  = surface;
          state->modified    |= SMF_DESTINATION;

          /* Set clipping region. */
          dfb_state_set_clip( state, &context->cursor_region );

          /* draw cursor */
          unique_draw_cursor( stack, context, state, &context->cursor_region );

          /* Reset destination. */
          state->destination  = NULL;
          state->modified    |= SMF_DESTINATION;

          context->cursor_drawn = true;

          if (restored) {
               if (dfb_region_region_intersects( &old_region, &context->cursor_region ))
                    dfb_region_region_union( &old_region, &context->cursor_region );
               else
                    dfb_layer_region_flip_update( region, &context->cursor_region, DSFLIP_BLIT );

               dfb_layer_region_flip_update( region, &old_region, DSFLIP_BLIT );
          }
          else
               dfb_layer_region_flip_update( region, &context->cursor_region, DSFLIP_BLIT );
     }
     else if (restored)
          dfb_layer_region_flip_update( region, &old_region, DSFLIP_BLIT );

     return DFB_OK;
}

