#ifndef __SaWMan_includes_h__
#define __SaWMan_includes_h__

#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include <sawman.h>
#include <sawman_internal.h>


static __inline__ DirectResult
SaWMan_Call( SaWMan              *sawman,
             FusionCallExecFlags  flags,
             int                  call_arg,
             void                *ptr,
             unsigned int         length,
             void                *ret_ptr,
             unsigned int         ret_size,
             unsigned int        *ret_length )
{
     return fusion_call_execute3( &sawman->call, flags, call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}



static __inline__ DirectResult
SaWManProcess_Lookup( CoreDFB        *core,
                      u32             object_id,
                      SaWManProcess **ret_process )
{
     SaWManProcess *process;

     direct_list_foreach (process, m_sawman->processes) {
          if (process->fusion_id == object_id) {
               *ret_process = process;
               return fusion_ref_up( &process->ref, false );
          }
     }

     return DR_IDNOTFOUND;
}

static __inline__ DirectResult
SaWManProcess_Unref( SaWManProcess *process )
{
     return fusion_ref_down( &process->ref, false );
}

static __inline__ DirectResult
SaWManProcess_Catch( CoreDFB        *core,
                     u32             object_id,
                     SaWManProcess **ret_process )
{
     SaWManProcess *process;

     direct_list_foreach (process, m_sawman->processes) {
          if (process->fusion_id == object_id) {
               *ret_process = process;

               fusion_ref_up( &process->ref, false );

               return fusion_ref_catch( &process->ref );
          }
     }

     return DR_IDNOTFOUND;
}

static __inline__ DirectResult
SaWManProcess_Throw( SaWManProcess *process,
                     FusionID       catcher,
                     u32           *ret_object_id )
{
     *ret_object_id = process->fusion_id;

     return fusion_ref_throw( &process->ref, catcher );
}


#ifdef __cplusplus
}
#endif


#endif

