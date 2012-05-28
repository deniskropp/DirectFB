#include "CoreDFB_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <core/screens_internal.h>


static __inline__ DirectResult
CoreScreen_Call( CoreScreen          *screen,
                 FusionCallExecFlags  flags,
                 int                  call_arg,
                 void                *ptr,
                 unsigned int         length,
                 void                *ret_ptr,
                 unsigned int         ret_size,
                 unsigned int        *ret_length )
{
     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     return fusion_call_execute3( &screen->shared->call,
                                  (FusionCallExecFlags)(dfb_config->call_nodirect | flags),
                                  call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}

#ifdef __cplusplus
}
#endif

