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
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/windows.h>


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

static __inline__ DirectResult
CoreLayer_Lookup( CoreDFB    *core,
                  u32         object_id,
                  CoreLayer **ret_layer )
{
     if (object_id >= (u32) dfb_layer_num())
          return DR_IDNOTFOUND;

     *ret_layer = dfb_layer_at( object_id );

     return DR_OK;
}

static __inline__ DirectResult
CoreLayer_Unref( CoreLayerContext *context )
{
     return DR_OK;
}

static __inline__ DirectResult
CoreLayerContext_Lookup( CoreDFB           *core,
                         u32                object_id,
                         CoreLayerContext **ret_context )
{
     return (DirectResult) dfb_core_get_layer_context( core, object_id, ret_context );
}

static __inline__ DirectResult
CoreLayerContext_Unref( CoreLayerContext *context )
{
     return (DirectResult) dfb_layer_context_unref( context );
}

static __inline__ DirectResult
CoreLayerRegion_Lookup( CoreDFB          *core,
                        u32               object_id,
                        CoreLayerRegion **ret_region )
{
     return (DirectResult) dfb_core_get_layer_region( core, object_id, ret_region );
}

static __inline__ DirectResult
CoreLayerRegion_Unref( CoreLayerRegion *region )
{
     return (DirectResult) dfb_layer_region_unref( region );
}

static __inline__ DirectResult
CorePalette_Lookup( CoreDFB      *core,
                    u32           object_id,
                    CorePalette **ret_palette )
{
     return (DirectResult) dfb_core_get_palette( core, object_id, ret_palette );
}

static __inline__ DirectResult
CorePalette_Unref( CorePalette *palette )
{
     return (DirectResult) dfb_palette_unref( palette );
}

static __inline__ DirectResult
CoreSurface_Lookup( CoreDFB      *core,
                    u32           object_id,
                    CoreSurface **ret_surface )
{
     return (DirectResult) dfb_core_get_surface( core, object_id, ret_surface );
}

static __inline__ DirectResult
CoreSurface_Unref( CoreSurface *surface )
{
     return (DirectResult) dfb_surface_unref( surface );
}

static __inline__ DirectResult
CoreWindow_Lookup( CoreDFB     *core,
                   u32          object_id,
                   CoreWindow **ret_window )
{
     return (DirectResult) dfb_core_get_window( core, object_id, ret_window );
}

static __inline__ DirectResult
CoreWindow_Unref( CoreWindow *window )
{
     return (DirectResult) dfb_window_unref( window );
}







struct __DFB_CoreGraphicsState {
     int            magic;

     struct {
          u32 id;
     } object;

     CoreDFB       *core;

     CardState      state;

     FusionCall     call;
};


static __inline__ DirectResult
CoreGraphicsState_Lookup( CoreDFB            *core,
                          u32                 object_id,
                          CoreGraphicsState **ret_state )
{
     CoreGraphicsState *state;

     state = (CoreGraphicsState*) D_CALLOC( 1, sizeof(CoreGraphicsState) );
     if (!state)
          return D_OOM();

     state->core = core;
     // state->state is not used on client side, FIXME: find better integration for local/proxy objects
     fusion_call_init_from( &state->call, object_id, core->world );

     D_MAGIC_SET( state, CoreGraphicsState );

     *ret_state = state;

     return (DirectResult) DFB_OK;
}

static __inline__ DirectResult
CoreGraphicsState_Unref( CoreWindow *window )
{
     D_UNIMPLEMENTED();

     return (DirectResult) DFB_UNIMPLEMENTED;
}

#ifdef __cplusplus
}
#endif


#endif

