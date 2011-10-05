#ifndef __SaWMan_includes_h__
#define __SaWMan_includes_h__

#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <core/core.h>

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



static __inline__ u32
SaWManProcess_GetID( const SaWManProcess *process )
{
     return process->fusion_id;
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

     fusion_ref_add_permissions( &process->ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &process->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &process->ref, catcher );
}


static __inline__ u32
SaWManManager_GetID( const SaWManManager *manager )
{
     return 0;
}

static __inline__ DirectResult
SaWManManager_Lookup( CoreDFB        *core,
                      u32             object_id,
                      SaWManManager **ret_manager )
{
     D_ASSERT( object_id == 0 );

     *ret_manager = &m_sawman->manager;

     return DR_OK;
}

static __inline__ DirectResult
SaWManManager_Unref( SaWManManager *manager )
{
     return DR_OK;
}

static __inline__ DirectResult
SaWManManager_Catch( CoreDFB        *core,
                     u32             object_id,
                     SaWManManager **ret_manager )
{
     D_ASSERT( object_id == 0 );

     *ret_manager = &m_sawman->manager;

     return DR_OK;
}

static __inline__ DirectResult
SaWManManager_Throw( SaWManManager *manager,
                     FusionID       catcher,
                     u32           *ret_object_id )
{
     *ret_object_id = 0;

     fusion_call_add_permissions( &manager->call_from, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return DR_OK;
}


static __inline__ u32
SaWManWindow_GetID( const SaWManWindow *window )
{
     return window->window->object.id;
}

static __inline__ DirectResult
SaWManWindow_Lookup( CoreDFB       *core,
                     u32            object_id,
                     FusionID       caller,
                     SaWManWindow **ret_window )
{
     DFBResult   ret;
     CoreWindow *window;

     ret = dfb_core_get_window( core, object_id, &window );
     if (ret)
          return (DirectResult) ret;
/*
     Don't check owner here, so far calls come from application manager only, which needs access to all windows.

     if (window->object.owner && window->object.owner != caller) {
          dfb_window_unref( window );
          return DR_ACCESSDENIED;
     }
*/
     *ret_window = (SaWManWindow*) window->window_data;

     return DR_OK;
}

static __inline__ DirectResult
SaWManWindow_Unref( SaWManWindow *window )
{
     return (DirectResult) dfb_window_unref( window->window );
}

static __inline__ DirectResult
SaWManWindow_Catch( CoreDFB       *core,
                    u32            object_id,
                    SaWManWindow **ret_window )
{
     DFBResult   ret;
     CoreWindow *window;

     ret = dfb_core_get_window( core, object_id, &window );
     if (ret)
          return (DirectResult) ret;

     fusion_ref_catch( &window->object.ref );

     *ret_window = (SaWManWindow*) window->window_data;

     return DR_OK;
}

static __inline__ DirectResult
SaWManWindow_Throw( SaWManWindow *window,
                    FusionID      catcher,
                    u32          *ret_object_id )
{
     *ret_object_id = window->window->object.id;

     fusion_reactor_add_permissions( window->window->object.reactor, catcher,
                                     (FusionReactorPermissions)(FUSION_REACTOR_PERMIT_ATTACH_DETACH | FUSION_REACTOR_PERMIT_DISPATCH) );
     fusion_ref_add_permissions( &window->window->object.ref, catcher,
                                 (FusionRefPermissions)(FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH) );
     fusion_call_add_permissions( &window->window->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &window->window->object.ref, catcher );
}


#ifdef __cplusplus
}
#endif


#endif

