#include "CoreDFB_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <core/layer_context.h>
#include <core/layers_internal.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/windows_internal.h>


static __inline__ DirectResult
CoreWindowStack_Call( CoreWindowStack     *stack,
                      FusionCallExecFlags  flags,
                      int                  call_arg,
                      void                *ptr,
                      unsigned int         length,
                      void                *ret_ptr,
                      unsigned int         ret_size,
                      unsigned int        *ret_length )
{
     return fusion_call_execute3( &stack->call,
                                  (FusionCallExecFlags)(dfb_config->call_nodirect | flags),
                                  call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}

#ifdef __cplusplus
}
#endif

