/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <unistd.h>

#include <direct/debug.h>
#include <direct/list.h>

#include <fusion/conf.h>
#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/windows_internal.h>

#include <gfx/clip.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

#include <sawman.h>
#include <sawman_internal.h>

#include "sawman_config.h"
#include "sawman_draw.h"

#include "isawman.h"


D_DEBUG_DOMAIN( SaWMan_Auto,     "SaWMan/Auto",     "SaWMan auto configuration" );
D_DEBUG_DOMAIN( SaWMan_Update,   "SaWMan/Update",   "SaWMan window manager updates" );
D_DEBUG_DOMAIN( SaWMan_Geometry, "SaWMan/Geometry", "SaWMan window manager geometry" );
D_DEBUG_DOMAIN( SaWMan_Stacking, "SaWMan/Stacking", "SaWMan window manager stacking" );


/* FIXME: avoid globals */
static SaWMan        *m_sawman;
static SaWManProcess *m_process;
static FusionWorld   *m_world;

/**********************************************************************************************************************/

static FusionCallHandlerResult manager_call_handler( int                    caller,
                                                     int                    call_arg,
                                                     void                  *call_ptr,
                                                     void                  *ctx,
                                                     unsigned int           serial,
                                                     int                   *ret_val );

static DirectResult            register_process    ( SaWMan                *sawman,
                                                     SaWManProcessFlags     flags,
                                                     FusionWorld           *world );

static DirectResult            unregister_process  ( SaWMan                *sawman,
                                                     SaWManProcess         *process );

static FusionCallHandlerResult process_watcher     ( int                    caller,
                                                     int                    call_arg,
                                                     void                  *call_ptr,
                                                     void                  *ctx,
                                                     unsigned int           serial,
                                                     int                   *ret_val );

static DirectResult            add_tier            ( SaWMan                *sawman,
                                                     DFBDisplayLayerID      layer_id,
                                                     SaWManStackingClasses  classes );

/**********************************************************************************************************************/

DirectResult
SaWManInit( int    *argc,
            char ***argv )
{
     return sawman_config_init( argc, argv );
}

DirectResult
SaWManCreate( ISaWMan **ret_sawman )
{
     DirectResult  ret;
     ISaWMan      *sawman;

     if (!ret_sawman)
          return DFB_INVARG;

     if (!m_sawman) {
          D_ERROR( "SaWManCreate: No running SaWMan detected! Did you use the 'wm=sawman' option?\n" );
          return DFB_NOIMPL;
     }

     D_MAGIC_ASSERT( m_sawman, SaWMan );
     D_MAGIC_ASSERT( m_process, SaWManProcess );

     DIRECT_ALLOCATE_INTERFACE( sawman, ISaWMan );

     ret = ISaWMan_Construct( sawman, m_sawman, m_process );
     if (ret)
          return ret;

     *ret_sawman = sawman;

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
sawman_initialize( SaWMan         *sawman,
                   FusionWorld    *world,
                   SaWManProcess **ret_process )
{
     int                i;
     DirectResult       ret;
     GraphicsDeviceInfo info;

     D_ASSERT( sawman != NULL );
     D_ASSERT( world != NULL );

     D_ASSERT( m_sawman == NULL );

     /* Initialize process watcher call. */
     ret = fusion_call_init( &sawman->process_watch, process_watcher, sawman, world );
     if (ret)
          return ret;

     /* Create shared memory pool. */
     ret = fusion_shm_pool_create( world, "SaWMan Pool", 0x100000, fusion_config->debugshm, &sawman->shmpool );
     if (ret)
          goto error;

     /* Initialize lock. */
     fusion_skirmish_init( &sawman->lock, "SaWMan", world );

     /* Initialize window layout vector. */
     fusion_vector_init( &sawman->layout, 8, sawman->shmpool );

     /* Default to HW Scaling if supported. */
     if (dfb_gfxcard_get_device_info( &info ), info.caps.accel & DFXL_STRETCHBLIT)
          sawman->scaling_mode = SWMSM_STANDARD;

     /* Initialize grabbed keys. */
     for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++)
          sawman->keys[i].code = -1;

     dfb_updates_init( &sawman->bg.visible, sawman->bg.visible_regions, D_ARRAY_SIZE(sawman->bg.visible_regions) );

     D_MAGIC_SET( sawman, SaWMan );

     sawman_lock( sawman );

     /* Initialize tiers. */
     for (i=0; i<D_ARRAY_SIZE(dfb_config->layers); i++) {
          if (!dfb_config->layers[i].stacking)
               continue;

          ret = add_tier( sawman, i, dfb_config->layers[i].stacking );
          if (ret) {
               sawman_unlock( sawman );
               D_MAGIC_CLEAR( sawman );
               goto error;
          }
     }

     /* Set global singleton. */
     m_sawman = sawman;
     m_world  = world;

     /* Register ourself as a new process. */
     ret = register_process( sawman, SWMPF_MASTER, world );
     if (ret) {
          sawman_unlock( sawman );
          D_MAGIC_CLEAR( sawman );
          goto error;
     }

     sawman_unlock( sawman );

     if (ret_process)
          *ret_process = m_process;

     return DFB_OK;


error:
     if (sawman->tiers) {
          SaWManTier *tier;
          DirectLink *next;

          direct_list_foreach_safe (tier, next, sawman->tiers) {
               D_MAGIC_CLEAR( tier );
               SHFREE( sawman->shmpool, tier );
          }
     }

     fusion_call_destroy( &sawman->process_watch );

     m_sawman  = NULL;
     m_world   = NULL;
     m_process = NULL;

     return ret;
}

DirectResult
sawman_join( SaWMan         *sawman,
             FusionWorld    *world,
             SaWManProcess **ret_process )
{
     DirectResult ret;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( world != NULL );

     D_ASSERT( m_sawman == NULL );

     /* Set global singleton. */
     m_sawman = sawman;
     m_world  = world;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          goto error;

     /* Register ourself as a new process. */
     ret = register_process( sawman, SWMPF_NONE, world );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     if (ret)
          goto error;

     if (ret_process)
          *ret_process = m_process;

     return DFB_OK;


error:
     m_sawman  = NULL;
     m_world   = NULL;
     m_process = NULL;

     return ret;
}

DirectResult
sawman_shutdown( SaWMan      *sawman,
                 FusionWorld *world )
{
     DirectResult      ret;
     DirectLink       *next;
     SaWManProcess    *process;
     SaWManWindow     *sawwin;
     SaWManGrabbedKey *key;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( world != NULL );

     D_ASSERT( m_sawman == sawman );
     D_ASSERT( m_world == world );

     D_ASSERT( sawman->processes != NULL );
     D_ASSERT( sawman->processes->next == NULL );

     process = (SaWManProcess*) sawman->processes;

     D_ASSERT( process == m_process );
     D_ASSERT( process->fusion_id == fusion_id( world ) );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Shutdown our own process. */
     unregister_process( sawman, process );

     /* Clear global singleton. */
     m_process = NULL;

     /* Destroy process watcher call. */
     fusion_call_destroy( &sawman->process_watch );

     D_ASSERT( sawman->processes == NULL );
     D_ASSERT( !sawman->manager.present );

     D_ASSUME( sawman->windows == NULL );

     direct_list_foreach (sawwin, sawman->windows) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_ASSERT( sawwin->window != NULL );

          D_WARN( "window %d,%d-%dx%d still there", DFB_RECTANGLE_VALS( &sawwin->bounds ) );

          sawwin->window->stack = NULL;
     }

     /* FIXME */
     D_ASSUME( !sawman->windows );
     D_ASSUME( !sawman->layout.count );

     /* Destroy window layout vector. */
     fusion_vector_destroy( &sawman->layout );

     /* Destroy lock. */
     fusion_skirmish_destroy( &sawman->lock );

     /* Free grabbed keys. */
     direct_list_foreach_safe (key, next, sawman->grabbed_keys) {
          SHFREE( key->owner->shmpool, key );
     }

     dfb_updates_deinit( &sawman->bg.visible );

     D_MAGIC_CLEAR( sawman );

     /* Destroy shared memory pool. */
     fusion_shm_pool_destroy( world, sawman->shmpool );

     /* Clear global singleton. */
     m_sawman = NULL;
     m_world  = NULL;

     return DFB_OK;
}

DirectResult
sawman_leave( SaWMan      *sawman,
              FusionWorld *world )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( world != NULL );

     D_ASSERT( m_sawman == sawman );
     D_ASSERT( m_world == world );
     D_MAGIC_ASSERT( m_process, SaWManProcess );

     /* Set 'cleanly exiting' flag. */
     m_process->flags |= SWMPF_EXITING;

     /* Clear global singletons. */
     m_sawman  = NULL;
     m_world   = NULL;
     m_process = NULL;

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
sawman_register( SaWMan                *sawman,
                 const SaWManCallbacks *callbacks,
                 void                  *context )
{
     DirectResult ret;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( callbacks != NULL );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     D_ASSERT( m_sawman == sawman );
     D_ASSERT( m_world != NULL );
     D_MAGIC_ASSERT( m_process, SaWManProcess );

     /* Check if another manager already exists. */
     if (sawman->manager.present)
          return DFB_BUSY;

     /* Initialize the call to the manager executable (ourself). */
     ret = fusion_call_init( &sawman->manager.call, manager_call_handler, sawman, m_world );
     if (ret)
          return ret;

     /* Initialize manager data. */
     sawman->manager.callbacks = *callbacks;
     sawman->manager.context   = context;

     /* Set manager flag for our process. */
     m_process->flags |= SWMPF_MANAGER;

     /* Activate it at last. */
     sawman->manager.present = true;

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
sawman_call( SaWMan       *sawman,
             SaWManCallID  call,
             void         *ptr )
{
     int ret = DFB_FUSION;

     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     D_ASSERT( m_sawman == sawman );

     /* Check for presence of manager. */
     if (!sawman->manager.present)
          return DFB_NOIMPL;

     /* Avoid useless context switches etc. */
     switch (call) {
          case SWMCID_START:
               if (!sawman->manager.callbacks.Start)
                    return DFB_NOIMPL;
               break;

          case SWMCID_STOP:
               if (!sawman->manager.callbacks.Stop)
                    return DFB_NOIMPL;
               break;

          case SWMCID_PROCESS_ADDED:
               if (!sawman->manager.callbacks.ProcessAdded)
                    return DFB_NOIMPL;
               break;

          case SWMCID_PROCESS_REMOVED:
               if (!sawman->manager.callbacks.ProcessRemoved)
                    return DFB_NOIMPL;
               break;

          case SWMCID_INPUT_FILTER:
               if (!sawman->manager.callbacks.InputFilter)
                    return DFB_NOIMPL;
               break;

          case SWMCID_WINDOW_PRECONFIG:
               if (!sawman->manager.callbacks.WindowPreConfig)
                    return DFB_NOIMPL;
               break;

          case SWMCID_WINDOW_ADDED:
               if (!sawman->manager.callbacks.WindowAdded)
                    return DFB_NOIMPL;
               break;

          case SWMCID_WINDOW_REMOVED:
               if (!sawman->manager.callbacks.WindowRemoved)
                    return DFB_NOIMPL;
               break;

          case SWMCID_WINDOW_RECONFIG:
               if (!sawman->manager.callbacks.WindowReconfig)
                    return DFB_NOIMPL;
               break;

          case SWMCID_WINDOW_RESTACK:
               if (!sawman->manager.callbacks.WindowRestack)
                    return DFB_NOIMPL;
               break;

          case SWMCID_STACK_RESIZED:
               if (!sawman->manager.callbacks.StackResized)
                    return DFB_NOIMPL;
               break;

          case SWMCID_SWITCH_FOCUS:
               if (!sawman->manager.callbacks.SwitchFocus)
                    return DFB_NOIMPL;
               break;

          case SWMCID_LAYER_RECONFIG:
               if (!sawman->manager.callbacks.LayerReconfig)
                    return DFB_NOIMPL;
               break;

          case SWMCID_APPLICATION_ID_CHANGED:
               if (!sawman->manager.callbacks.ApplicationIDChanged)
                    return DFB_NOIMPL;
               break;

     }

     /* Execute the call in the manager executable. */
     if (fusion_call_execute( &sawman->manager.call, FCEF_NONE, call, ptr, &ret ))
          return DFB_NOIMPL;

     return ret;
}

static FusionCallHandlerResult
manager_call_handler( int           caller,
                      int           call_arg,
                      void         *call_ptr,
                      void         *ctx,
                      unsigned int  serial,
                      int          *ret_val )
{
     DirectResult  ret;
     SaWMan       *sawman = ctx;
     SaWManCallID  call   = call_arg;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Last mile of dispatch. */
     switch (call) {
          case SWMCID_START:
               if (sawman->manager.callbacks.Start) {
                    pid_t pid;

                    ret = sawman->manager.callbacks.Start( sawman->manager.context, call_ptr, &pid );
                    if (ret)
                         *ret_val = ret;
                    else
                         *ret_val = -pid;
               }
               break;

          case SWMCID_STOP:
               if (sawman->manager.callbacks.Stop)
                    *ret_val = sawman->manager.callbacks.Stop( sawman->manager.context, (long) call_ptr, caller );
               break;

          case SWMCID_PROCESS_ADDED:
               if (sawman->manager.callbacks.ProcessAdded)
                    *ret_val = sawman->manager.callbacks.ProcessAdded( sawman->manager.context, call_ptr );
               break;

          case SWMCID_PROCESS_REMOVED:
               if (sawman->manager.callbacks.ProcessRemoved)
                    *ret_val = sawman->manager.callbacks.ProcessRemoved( sawman->manager.context, call_ptr );
               break;

          case SWMCID_INPUT_FILTER:
               if (sawman->manager.callbacks.InputFilter)
                    *ret_val = sawman->manager.callbacks.InputFilter( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_PRECONFIG:
               if (sawman->manager.callbacks.WindowPreConfig)
                    *ret_val = sawman->manager.callbacks.WindowPreConfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_ADDED:
               if (sawman->manager.callbacks.WindowAdded)
                    *ret_val = sawman->manager.callbacks.WindowAdded( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_REMOVED:
               if (sawman->manager.callbacks.WindowRemoved)
                    *ret_val = sawman->manager.callbacks.WindowRemoved( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_RECONFIG:
               if (sawman->manager.callbacks.WindowReconfig)
                    *ret_val = sawman->manager.callbacks.WindowReconfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_RESTACK:
               if (sawman->manager.callbacks.WindowRestack)
                    *ret_val = sawman->manager.callbacks.WindowRestack( sawman->manager.context,
                                                                        sawman->callback.handle,
                                                                        sawman->callback.relative,
                                                                        (SaWManWindowRelation)call_ptr );
               break;

          case SWMCID_STACK_RESIZED:
               if (sawman->manager.callbacks.StackResized)
                    *ret_val = sawman->manager.callbacks.StackResized( sawman->manager.context, call_ptr );
               break;

          case SWMCID_SWITCH_FOCUS:
               if (sawman->manager.callbacks.SwitchFocus)
                    *ret_val = sawman->manager.callbacks.SwitchFocus( sawman->manager.context,
                                                                      (SaWManWindowHandle)call_ptr );
               break;

          case SWMCID_LAYER_RECONFIG:
               if (sawman->manager.callbacks.LayerReconfig)
                    *ret_val = sawman->manager.callbacks.LayerReconfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_APPLICATION_ID_CHANGED:
               if (sawman->manager.callbacks.ApplicationIDChanged)
                    *ret_val = sawman->manager.callbacks.ApplicationIDChanged( sawman->manager.context, call_ptr );
               break;

          default:
               *ret_val = DFB_NOIMPL;
     }

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

static DirectResult
register_process( SaWMan             *sawman,
                  SaWManProcessFlags  flags,
                  FusionWorld        *world )
{
     DirectResult   ret;
     SaWManProcess *process;

     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     /* Allocate process data. */
     process = SHCALLOC( sawman->shmpool, 1, sizeof(SaWManProcess) );
     if (!process)
          return D_OOSHM();

     /* Initialize process data. */
     process->pid       = getpid();
     process->fusion_id = fusion_id( world );
     process->flags     = flags;

     /* Initialize reference counter. */
     ret = fusion_ref_init( &process->ref, "SaWMan Process", world );
     if (ret) {
          D_DERROR( ret, "SaWMan/Register: fusion_ref_init() failed!\n" );
          goto error_ref;
     }

     /* Add a local reference. */
     ret = fusion_ref_up( &process->ref, false );
     if (ret) {
          D_DERROR( ret, "SaWMan/Register: fusion_ref_up() failed!\n" );
          goto error;
     }

     /* Set the process watcher on this. */
     ret = fusion_ref_watch( &process->ref, &sawman->process_watch, process->pid );
     if (ret) {
          D_DERROR( ret, "SaWMan/Register: fusion_ref_watch() failed!\n" );
          goto error;
     }

     D_MAGIC_SET( process, SaWManProcess );

     /* Add process to list. */
     direct_list_append( &sawman->processes, &process->link );

     /* Call application manager executable. */
     sawman_call( sawman, SWMCID_PROCESS_ADDED, process );

     /* Set global singleton. */
     m_process = process;

     return DFB_OK;


error:
     fusion_ref_destroy( &process->ref );

error_ref:
     SHFREE( sawman->shmpool, process );

     return ret;
}

static DirectResult
unregister_process( SaWMan        *sawman,
                    SaWManProcess *process )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( process, SaWManProcess );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     /* Destroy reference counter. */
     fusion_ref_destroy( &process->ref );

     /* Remove process from list. */
     direct_list_remove( &sawman->processes, &process->link );

     /* Unregister manager process? */
     if (process->flags & SWMPF_MANAGER) {
          D_ASSERT( sawman->manager.present );

          /* Destroy manager call, unless it was another process. */
          if (m_process == process)
               fusion_call_destroy( &sawman->manager.call );
          else
               sawman->manager.call.handler = NULL;    /* FIXME: avoid failing assertion in fusion_call_init() */

          /* Ready for new manager. */
          sawman->manager.present = false;
     }
     else {
          /* Call application manager executable. */
          sawman_call( sawman, SWMCID_PROCESS_REMOVED, process );
     }

     D_MAGIC_CLEAR( process );

     /* Deallocate process data. */
     SHFREE( sawman->shmpool, process );

     return DFB_OK;
}

static FusionCallHandlerResult
process_watcher( int           caller,
                 int           call_arg,
                 void         *call_ptr,
                 void         *ctx,
                 unsigned int  serial,
                 int          *ret_val )
{
     DFBResult      ret;
     SaWMan        *sawman  = ctx;
     SaWManProcess *process;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lookup process by pid. */
     direct_list_foreach (process, sawman->processes) {
          D_MAGIC_ASSERT( process, SaWManProcess );

          if (process->pid == call_arg)
               break;
     }

     if (!process) {
          D_BUG( "process with pid %d not found", call_arg );
          *ret_val = DFB_BUG;
          return FCHR_RETURN;
     }

     D_INFO( "SaWMan/Watcher: Process %d [%lu] has exited%s\n", process->pid,
             process->fusion_id, (process->flags & SWMPF_EXITING) ? "." : " ABNORMALLY!" );

     ret = sawman_lock( sawman );
     if (ret)
          D_DERROR( ret, "SaWMan/%s(): sawman_lock() failed!\n", __FUNCTION__ );
     else {
          unregister_process( sawman, process );

          sawman_unlock( sawman );
     }

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

static DirectResult
add_tier( SaWMan                *sawman,
          DFBDisplayLayerID      layer_id,
          SaWManStackingClasses  classes )
{
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( layer_id >= 0 );
     D_ASSERT( layer_id < MAX_LAYERS );
     D_ASSERT( (classes &  7) != 0 );
     D_ASSERT( (classes & ~7) == 0 );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->classes & classes) {
               D_ERROR( "SaWMan/Tiers: Cannot add tier for layer %d's classes 0x%x which collides with "
                        "layer %d's classes 0x%x!\n", layer_id, classes, tier->layer_id, tier->classes );
               return DFB_BUSY;
          }

          if (tier->layer_id == layer_id) {
               D_ERROR( "SaWMan/Tiers: Cannot add tier with layer %d which is already added!\n", layer_id );
               return DFB_BUSY;
          }
     }

     tier = SHCALLOC( sawman->shmpool, 1, sizeof(SaWManTier) );
     if (!tier)
          return D_OOSHM();

     tier->layer_id = layer_id;
     tier->classes  = classes;

     dfb_updates_init( &tier->updates, tier->update_regions, SAWMAN_MAX_UPDATE_REGIONS );

     D_MAGIC_SET( tier, SaWManTier );

     direct_list_append( &sawman->tiers, &tier->link );

     return DFB_OK;
}

