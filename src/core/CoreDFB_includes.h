#ifndef __CoreDFB_includes_h__
#define __CoreDFB_includes_h__

#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include <core/core.h>
#include <core/graphics_state.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/screens.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/windows.h>
#include <core/windows_internal.h>


static __inline__ DirectResult
CoreDFB_Call( CoreDFB             *core,
              FusionCallExecFlags  flags,
              int                  call_arg,
              void                *ptr,
              unsigned int         length,
              void                *ret_ptr,
              unsigned int         ret_size,
              unsigned int        *ret_length )
{
     return fusion_call_execute3( &core->shared->call, flags, call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}



static __inline__ u32
CoreGraphicsState_GetID( const CoreGraphicsState *state )
{
     return state->object.id;
}

static __inline__ DirectResult
CoreGraphicsState_Lookup( CoreDFB            *core,
                          u32                 object_id,
                          FusionID            caller,
                          CoreGraphicsState **ret_state )
{
     DFBResult          ret;
     CoreGraphicsState *state;

     ret = dfb_core_get_graphics_state( core, object_id, &state );
     if (ret)
          return (DirectResult) ret;

     if (state->object.owner && state->object.owner != caller) {
          dfb_graphics_state_unref( state );
          return DR_ACCESSDENIED;
     }

     *ret_state = state;

     return DR_OK;
}

static __inline__ DirectResult
CoreGraphicsState_Unref( CoreGraphicsState *state )
{
     return (DirectResult) dfb_graphics_state_unref( state );
}

static __inline__ DirectResult
CoreGraphicsState_Catch( CoreDFB            *core,
                         u32                 object_id,
                         CoreGraphicsState **ret_state )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_graphics_state( core, object_id, ret_state );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_state)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreGraphicsState_Throw( CoreGraphicsState *state,
                         FusionID           catcher,
                         u32               *ret_object_id )
{
     *ret_object_id = state->object.id;

     fusion_reactor_add_permissions( state->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &state->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &state->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!state->object.owner)
          state->object.owner = catcher;

     return fusion_ref_throw( &state->object.ref, catcher );
}



static __inline__ u32
CoreLayer_GetID( const CoreLayer *layer )
{
     return dfb_layer_id( layer );
}

static __inline__ DirectResult
CoreLayer_Lookup( CoreDFB    *core,
                  u32         object_id,
                  FusionID    caller,
                  CoreLayer **ret_layer )
{
     if (object_id >= (u32) dfb_layer_num())
          return DR_IDNOTFOUND;

     *ret_layer = dfb_layer_at( object_id );

     return DR_OK;
}

static __inline__ DirectResult
CoreLayer_Unref( CoreLayer *layer )
{
     return DR_OK;
}

static __inline__ DirectResult
CoreLayer_Catch( CoreDFB    *core,
                 u32         object_id,
                 CoreLayer **ret_layer )
{
     if (object_id >= (u32) dfb_layer_num())
          return DR_IDNOTFOUND;

     *ret_layer = dfb_layer_at( object_id );

     return DR_OK;
}

static __inline__ DirectResult
CoreLayer_Throw( CoreLayer *layer,
                 FusionID   catcher,
                 u32       *ret_object_id )
{
     *ret_object_id = layer->shared->layer_id;

     return DR_OK;
}



static __inline__ u32
CoreLayerContext_GetID( const CoreLayerContext *context )
{
     return context->object.id;
}

static __inline__ DirectResult
CoreLayerContext_Lookup( CoreDFB           *core,
                         u32                object_id,
                         FusionID           caller,
                         CoreLayerContext **ret_context )
{
     DFBResult         ret;
     CoreLayerContext *context;

     ret = dfb_core_get_layer_context( core, object_id, &context );
     if (ret)
          return (DirectResult) ret;

     if (context->object.owner && context->object.owner != caller) {
          dfb_layer_context_unref( context );
          return DR_ACCESSDENIED;
     }

     *ret_context = context;

     return DR_OK;
}

static __inline__ DirectResult
CoreLayerContext_Unref( CoreLayerContext *context )
{
     return (DirectResult) dfb_layer_context_unref( context );
}

static __inline__ DirectResult
CoreLayerContext_Catch( CoreDFB           *core,
                        u32                object_id,
                        CoreLayerContext **ret_context )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_layer_context( core, object_id, ret_context );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_context)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreLayerContext_Throw( CoreLayerContext *context,
                        FusionID          catcher,
                        u32              *ret_object_id )
{
     *ret_object_id = context->object.id;

     fusion_reactor_add_permissions( context->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &context->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &context->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (context->stack)
          fusion_call_add_permissions( &context->stack->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &context->object.ref, catcher );
}


static __inline__ u32
CoreLayerRegion_GetID( const CoreLayerRegion *region )
{
     return region->object.id;
}

static __inline__ DirectResult
CoreLayerRegion_Lookup( CoreDFB          *core,
                        u32               object_id,
                        FusionID          caller,
                        CoreLayerRegion **ret_region )
{
     DFBResult        ret;
     CoreLayerRegion *region;

     ret = dfb_core_get_layer_region( core, object_id, &region );
     if (ret)
          return (DirectResult) ret;

     if (region->object.owner && region->object.owner != caller) {
          dfb_layer_region_unref( region );
          return DR_ACCESSDENIED;
     }

     *ret_region = region;

     return DR_OK;
}

static __inline__ DirectResult
CoreLayerRegion_Unref( CoreLayerRegion *region )
{
     return (DirectResult) dfb_layer_region_unref( region );
}

static __inline__ DirectResult
CoreLayerRegion_Catch( CoreDFB          *core,
                       u32               object_id,
                       CoreLayerRegion **ret_region )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_layer_region( core, object_id, ret_region );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_region)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreLayerRegion_Throw( CoreLayerRegion *region,
                       FusionID         catcher,
                       u32             *ret_object_id )
{
     *ret_object_id = region->object.id;

     fusion_reactor_add_permissions( region->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &region->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &region->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &region->object.ref, catcher );
}



static __inline__ u32
CorePalette_GetID( const CorePalette *palette )
{
     return palette->object.id;
}

static __inline__ DirectResult
CorePalette_Lookup( CoreDFB      *core,
                    u32           object_id,
                    FusionID      caller,
                    CorePalette **ret_palette )
{
     DFBResult    ret;
     CorePalette *palette;

     ret = dfb_core_get_palette( core, object_id, &palette );
     if (ret)
          return (DirectResult) ret;

     if (palette->object.owner && palette->object.owner != caller) {
          dfb_palette_unref( palette );
          return DR_ACCESSDENIED;
     }

     *ret_palette = palette;

     return DR_OK;
}

static __inline__ DirectResult
CorePalette_Unref( CorePalette *palette )
{
     return (DirectResult) dfb_palette_unref( palette );
}

static __inline__ DirectResult
CorePalette_Catch( CoreDFB      *core,
                   u32           object_id,
                   CorePalette **ret_palette )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_palette( core, object_id, ret_palette );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_palette)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CorePalette_Throw( CorePalette *palette,
                   FusionID     catcher,
                   u32         *ret_object_id )
{
     *ret_object_id = palette->object.id;

     fusion_reactor_add_permissions( palette->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &palette->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &palette->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!palette->object.owner)
          palette->object.owner = catcher;

     return fusion_ref_throw( &palette->object.ref, catcher );
}



static __inline__ u32
CoreScreen_GetID( const CoreScreen *screen )
{
     return dfb_screen_id( screen );
}

static __inline__ DirectResult
CoreScreen_Lookup( CoreDFB     *core,
                   u32          object_id,
                   FusionID     caller,
                   CoreScreen **ret_screen )
{
     if (object_id >= (u32) dfb_screens_num())
          return DR_IDNOTFOUND;

     *ret_screen = dfb_screens_at( object_id );

     return DR_OK;
}

static __inline__ DirectResult
CoreScreen_Unref( CoreScreen *screen )
{
     return DR_OK;
}

static __inline__ DirectResult
CoreScreen_Catch( CoreDFB     *core,
                  u32          object_id,
                  CoreScreen **ret_screen )
{
     if (object_id >= (u32) dfb_screens_num())
          return DR_IDNOTFOUND;

     *ret_screen = dfb_screens_at( object_id );

     return DR_OK;
}

static __inline__ DirectResult
CoreScreen_Throw( CoreScreen *screen,
                  FusionID    catcher,
                  u32        *ret_object_id )
{
     *ret_object_id = dfb_screen_id( screen );

     return DR_OK;
}



static __inline__ u32
CoreSurface_GetID( const CoreSurface *surface )
{
     return surface->object.id;
}

static __inline__ DirectResult
CoreSurface_Lookup( CoreDFB      *core,
                    u32           object_id,
                    FusionID      caller,
                    CoreSurface **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     ret = dfb_core_get_surface( core, object_id, &surface );
     if (ret)
          return (DirectResult) ret;

     if (surface->object.owner && surface->object.owner != caller) {
          dfb_surface_unref( surface );
          return DR_ACCESSDENIED;
     }

     *ret_surface = surface;

     return DR_OK;
}

static __inline__ DirectResult
CoreSurface_Unref( CoreSurface *surface )
{
     return (DirectResult) dfb_surface_unref( surface );
}

static __inline__ DirectResult
CoreSurface_Catch( CoreDFB      *core,
                   u32           object_id,
                   CoreSurface **ret_surface )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_surface( core, object_id, ret_surface );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_surface)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreSurface_Throw( CoreSurface *surface,
                   FusionID     catcher,
                   u32         *ret_object_id )
{
     *ret_object_id = surface->object.id;

     fusion_reactor_add_permissions( surface->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &surface->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &surface->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!surface->object.owner)
          surface->object.owner = catcher;

     return fusion_ref_throw( &surface->object.ref, catcher );
}



static __inline__ u32
CoreSurfaceAllocation_GetID( const CoreSurfaceAllocation *allocation )
{
     return allocation->object.id;
}

static __inline__ DirectResult
CoreSurfaceAllocation_Lookup( CoreDFB                *core,
                              u32                     object_id,
                              FusionID                caller,
                              CoreSurfaceAllocation **ret_allocation )
{
     DFBResult          ret;
     CoreSurfaceAllocation *allocation;

     ret = dfb_core_get_surface_allocation( core, object_id, &allocation );
     if (ret)
          return (DirectResult) ret;

     if (allocation->object.owner && allocation->object.owner != caller) {
          dfb_surface_allocation_unref( allocation );
          return DR_ACCESSDENIED;
     }

     *ret_allocation = allocation;

     return DR_OK;
}

static __inline__ DirectResult
CoreSurfaceAllocation_Unref( CoreSurfaceAllocation *allocation )
{
     return (DirectResult) dfb_surface_allocation_unref( allocation );
}

static __inline__ DirectResult
CoreSurfaceAllocation_Catch( CoreDFB                *core,
                             u32                     object_id,
                             CoreSurfaceAllocation **ret_allocation )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_surface_allocation( core, object_id, ret_allocation );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_allocation)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreSurfaceAllocation_Throw( CoreSurfaceAllocation *allocation,
                             FusionID               catcher,
                             u32                   *ret_object_id )
{
     *ret_object_id = allocation->object.id;

     fusion_reactor_add_permissions( allocation->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &allocation->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     //fusion_call_add_permissions( &allocation->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!allocation->object.owner)
          allocation->object.owner = catcher;

     return fusion_ref_throw( &allocation->object.ref, catcher );
}



static __inline__ u32
CoreSurfaceBuffer_GetID( const CoreSurfaceBuffer *buffer )
{
     return buffer->object.id;
}

static __inline__ DirectResult
CoreSurfaceBuffer_Lookup( CoreDFB            *core,
                          u32                 object_id,
                          FusionID            caller,
                          CoreSurfaceBuffer **ret_buffer )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     ret = dfb_core_get_surface_buffer( core, object_id, &buffer );
     if (ret)
          return (DirectResult) ret;

     if (buffer->object.owner && buffer->object.owner != caller) {
          dfb_surface_buffer_unref( buffer );
          return DR_ACCESSDENIED;
     }

     *ret_buffer = buffer;

     return DR_OK;
}

static __inline__ DirectResult
CoreSurfaceBuffer_Unref( CoreSurfaceBuffer *buffer )
{
     return (DirectResult) dfb_surface_buffer_unref( buffer );
}

static __inline__ DirectResult
CoreSurfaceBuffer_Catch( CoreDFB            *core,
                         u32                 object_id,
                         CoreSurfaceBuffer **ret_buffer )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_surface_buffer( core, object_id, ret_buffer );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_buffer)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreSurfaceBuffer_Throw( CoreSurfaceBuffer *buffer,
                         FusionID           catcher,
                         u32               *ret_object_id )
{
     *ret_object_id = buffer->object.id;

     fusion_reactor_add_permissions( buffer->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH) );
     fusion_ref_add_permissions( &buffer->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     //fusion_call_add_permissions( &buffer->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!buffer->object.owner)
          buffer->object.owner = catcher;

     return fusion_ref_throw( &buffer->object.ref, catcher );
}



static __inline__ u32
CoreWindow_GetID( const CoreWindow *window )
{
     return window->object.id;
}

static __inline__ DirectResult
CoreWindow_Lookup( CoreDFB     *core,
                   u32          object_id,
                   FusionID     caller,
                   CoreWindow **ret_window )
{
     DFBResult   ret;
     CoreWindow *window;

     ret = dfb_core_get_window( core, object_id, &window );
     if (ret)
          return (DirectResult) ret;

     if (window->object.owner && window->object.owner != caller) {
          dfb_window_unref( window );
          return DR_ACCESSDENIED;
     }

     *ret_window = window;

     return DR_OK;
}

static __inline__ DirectResult
CoreWindow_Unref( CoreWindow *window )
{
     return (DirectResult) dfb_window_unref( window );
}

static __inline__ DirectResult
CoreWindow_Catch( CoreDFB     *core,
                  u32          object_id,
                  CoreWindow **ret_window )
{
     DirectResult ret;

     ret = (DirectResult) dfb_core_get_window( core, object_id, ret_window );
     if (ret)
          return ret;

     fusion_ref_catch( &(*ret_window)->object.ref );

     return DR_OK;
}

static __inline__ DirectResult
CoreWindow_Throw( CoreWindow *window,
                  FusionID    catcher,
                  u32        *ret_object_id )
{
     *ret_object_id = window->object.id;

     fusion_reactor_add_permissions( window->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH | FUSION_REACTOR_PERMIT_DISPATCH) );
     fusion_ref_add_permissions( &window->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &window->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (!window->object.owner)
          window->object.owner = catcher;

     return fusion_ref_throw( &window->object.ref, catcher );
}


#ifdef __cplusplus
}
#endif


#endif

