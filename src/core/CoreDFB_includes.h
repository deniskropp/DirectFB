#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include <core/core.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/windows.h>

static __inline__ DirectResult
CoreDFB_Call( CoreDFB             *core,
              FusionCallExecFlags  flags,
              int                  call_arg,
              void                *ptr,
              unsigned int         length,
              void                *ret_ptr,
              size_t               ret_size )
{
     return fusion_call_execute3( &core->shared->call, flags, call_arg, ptr, length, ret_ptr, ret_size );
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

#ifdef __cplusplus
}
#endif

