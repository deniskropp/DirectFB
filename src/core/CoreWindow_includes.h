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
                 size_t               ret_size )
{
     return fusion_call_execute3( &window->call, flags, call_arg, ptr, length, ret_ptr, ret_size );
}

#ifdef __cplusplus
}
#endif

