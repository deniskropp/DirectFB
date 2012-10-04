#ifndef __SaWManProcess_includes_h__
#define __SaWManProcess_includes_h__

#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include <sawman.h>
#include <sawman_internal.h>

#include "SaWMan_includes.h"


static __inline__ DirectResult
SaWManProcess_Call( SaWManProcess       *process,
                    FusionCallExecFlags  flags,
                    int                  call_arg,
                    void                *ptr,
                    unsigned int         length,
                    void                *ret_ptr,
                    unsigned int         ret_size,
                    unsigned int        *ret_length )
{
     return fusion_call_execute3( &process->call, flags, call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}




#ifdef __cplusplus
}
#endif


#endif

