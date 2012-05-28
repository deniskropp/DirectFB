#include "CoreDFB_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <core/windows.h>
#include <core/windows_internal.h>


static __inline__ DirectResult
CoreWindow_Call( CoreWindow          *window,
                 FusionCallExecFlags  flags,
                 int                  call_arg,
                 void                *ptr,
                 unsigned int         length,
                 void                *ret_ptr,
                 unsigned int         ret_size,
                 unsigned int        *ret_length )
{
     return fusion_call_execute3( &window->call,
                                  (FusionCallExecFlags)(dfb_config->call_nodirect | flags),
                                  call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}

#ifdef __cplusplus
}
#endif

