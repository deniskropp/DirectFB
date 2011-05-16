#include "CoreDFB_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <core/surface.h>
#include <core/surface_buffer.h>


static __inline__ DirectResult
CoreSurface_Call( CoreSurface         *surface,
                  FusionCallExecFlags  flags,
                  int                  call_arg,
                  void                *ptr,
                  unsigned int         length,
                  void                *ret_ptr,
                  size_t               ret_size )
{
     return fusion_call_execute3( &surface->call, flags, call_arg, ptr, length, ret_ptr, ret_size );
}

#ifdef __cplusplus
}
#endif

