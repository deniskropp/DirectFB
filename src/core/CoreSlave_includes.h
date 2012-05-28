#ifndef __CORESLAVE_INCLUDES_H__
#define __CORESLAVE_INCLUDES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <directfb.h>

#include <fusion/call.h>

#include <core/coretypes.h>

#include <misc/conf.h>


typedef struct {
     FusionCall call;
} CoreSlave;

static __inline__ DirectResult
CoreSlave_Call( CoreSlave           *slave,
                FusionCallExecFlags  flags,
                int                  call_arg,
                void                *ptr,
                unsigned int         length,
                void                *ret_ptr,
                unsigned int         ret_size,
                unsigned int        *ret_length )
{
     return fusion_call_execute3( &slave->call,
                                  (FusionCallExecFlags)(dfb_config->call_nodirect | flags),
                                  call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}

#ifdef __cplusplus
}
#endif

#endif

