#include "CoreDFB_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <core/layer_context.h>
#include <core/layers_internal.h>
#include <core/windows.h>
#include <core/windows_internal.h>


static __inline__ DirectResult
CoreLayerContext_Call( CoreLayerContext    *context,
                       FusionCallExecFlags  flags,
                       int                  call_arg,
                       void                *ptr,
                       unsigned int         length,
                       void                *ret_ptr,
                       size_t               ret_size )
{
     return fusion_call_execute3( &context->call, flags, call_arg, ptr, length, ret_ptr, ret_size );
}

#ifdef __cplusplus
}
#endif

