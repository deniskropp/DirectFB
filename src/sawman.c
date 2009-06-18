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

static void wind_of_change ( SaWMan              *sawman,
                             SaWManTier          *tier,
                             DFBRegion           *update,
                             DFBSurfaceFlipFlags  flags,
                             int                  current,
                             int                  changed );

static void wind_of_showing( SaWMan              *sawman,
                             SaWManTier          *tier,
                             DFBRegion           *update,
                             int                  current,
                             int                  changed,
                             bool                *ret_showing );

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

/**********************************************************************************************************************/

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
                                                                      (SaWManWindowRelation)call_ptr );
               break;

          default:
               *ret_val = DFB_NOIMPL;
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
     }

     /* Execute the call in the manager executable. */
     if (fusion_call_execute( &sawman->manager.call, FCEF_NONE, call, ptr, &ret ))
          return DFB_NOIMPL;

     return ret;
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

DirectResult
sawman_switch_focus( SaWMan       *sawman,
                     SaWManWindow *to )
{
     DirectResult    ret;
     DFBWindowEvent  evt;
     SaWManWindow   *from;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT_IF( to, SaWManWindow );

     from = sawman->focused_window;
     D_MAGIC_ASSERT_IF( from, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     if (from == to)
          return DFB_OK;

     switch (ret = sawman_call( sawman, SWMCID_SWITCH_FOCUS, to )) {
          case DFB_OK:
          case DFB_NOIMPL:
               break;

          default:
               return ret;
     }

     if (from) {
          evt.type = DWET_LOSTFOCUS;

          sawman_post_event( sawman, from, &evt );

          if (sawman_window_border( from ))
               sawman_update_window( sawman, from, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
     }

     if (to) {

#ifndef OLD_COREWINDOWS_STRUCTURE
          CoreWindow *window = to->window;

          D_MAGIC_ASSERT( window, CoreWindow );

          if (window->toplevel) {
               CoreWindow *toplevel = window->toplevel;

               D_MAGIC_ASSERT( toplevel, CoreWindow );

               toplevel->subfocus = window;
          }
          else if (window->subfocus) {
               window = window->subfocus;
               D_MAGIC_ASSERT( window, CoreWindow );

               to = window->window_data;
               D_MAGIC_ASSERT( to, SaWManWindow );
          }
#endif

          evt.type = DWET_GOTFOCUS;

          sawman_post_event( sawman, to, &evt );

          if (sawman_window_border( to ))
               sawman_update_window( sawman, to, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
     }

     sawman->focused_window = to;

     return DFB_OK;
}

DirectResult
sawman_post_event( SaWMan         *sawman,
                   SaWManWindow   *sawwin,
                   DFBWindowEvent *event )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->window != NULL );
     D_ASSERT( event != NULL );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     event->buttons   = sawman->buttons;
     event->modifiers = sawman->modifiers;
     event->locks     = sawman->locks;

     dfb_window_post_event( sawwin->window, event );

     return DFB_OK;
}

DirectResult
sawman_update_window( SaWMan              *sawman,
                      SaWManWindow        *sawwin,
                      const DFBRegion     *region,
                      DFBSurfaceFlipFlags  flags,
                      SaWManUpdateFlags    update_flags )
{
     DFBRegion        area;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     DFB_REGION_ASSERT_IF( region );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_MAGIC_COREWINDOW_ASSERT( window );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %p )\n", __FUNCTION__, sawwin, region );

     if (!SAWMAN_VISIBLE_WINDOW(window) && !(update_flags & SWMUF_FORCE_INVISIBLE))
          return DFB_OK;

     D_ASSUME( sawwin->flags & SWMWF_INSERTED );

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_DEBUG_AT( SaWMan_Update, "  -> window %d not inserted!\n", window->id );
          return DFB_OK;
     }

     tier = sawman_tier_by_class( sawman, window->config.stacking );

     if (region) {
          if ((update_flags & SWMUF_SCALE_REGION) && (window->config.options & DWOP_SCALE)) {
               int sw = sawwin->src.w;
               int sh = sawwin->src.h;
               int dw = sawwin->dst.w;
               int dh = sawwin->dst.h;

               /* horizontal */
               if (dw > sw) {
                    /* upscaling */
                    area.x1 = (region->x1 - 1) * dw / sw;
                    area.x2 = (region->x2 + 1) * dw / sw;
               }
               else {
                    /* downscaling */
                    area.x1 = region->x1 * dw / sw - 1;
                    area.x2 = region->x2 * dw / sw + 1;
               }

               /* vertical */
               if (dh > sh) {
                    /* upscaling */
                    area.y1 = (region->y1 - 1) * dh / sh;
                    area.y2 = (region->y2 + 1) * dh / sh;
               }
               else {
                    /* downscaling */
                    area.y1 = region->y1 * dh / sh - 1;
                    area.y2 = region->y2 * dh / sh + 1;
               }

               /* limit to window area */
               dfb_region_clip( &area, 0, 0, dw - 1, dh - 1 );

               /* screen offset */
               dfb_region_translate( &area, sawwin->dst.x, sawwin->dst.y );
          }
          else
               area = DFB_REGION_INIT_TRANSLATED( region, sawwin->dst.x, sawwin->dst.y );
     }
     else {
          if ((update_flags & SWMUF_UPDATE_BORDER) && sawman_window_border( sawwin ))
               area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );
          else
               area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );
     }

     if (!dfb_unsafe_region_intersect( &area, 0, 0, tier->size.w - 1, tier->size.h - 1 ))
          return DFB_OK;

     if (update_flags & SWMUF_FORCE_COMPLETE)
          dfb_updates_add( &tier->updates, &area );
     else
          wind_of_change( sawman, tier, &area, flags,
                          fusion_vector_size( &sawman->layout ) - 1,
                          sawman_window_index( sawman, sawwin ) );

     return DFB_OK;
}

DirectResult
sawman_showing_window( SaWMan       *sawman,
                       SaWManWindow *sawwin,
                       bool         *ret_showing )
{
     DFBRegion        area;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_MAGIC_COREWINDOW_ASSERT( window );

     if (!sawman_tier_by_stack( sawman, stack, &tier ))
          return DFB_BUG;

     *ret_showing = false;

     if (!SAWMAN_VISIBLE_WINDOW(window))
          return DFB_OK;

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED))
          return DFB_OK;

     area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );

     if (!dfb_unsafe_region_intersect( &area, 0, 0, stack->width - 1, stack->height - 1 ))
          return DFB_OK;

     if (fusion_vector_has_elements( &sawman->layout ) && window >= 0) {
          int num = fusion_vector_size( &sawman->layout );

          wind_of_showing( sawman, tier, &area, num - 1,
                           sawman_window_index( sawman, sawwin ), ret_showing );
     }
     else
          *ret_showing = true;

     return DFB_OK;
}

DirectResult
sawman_insert_window( SaWMan       *sawman,
                      SaWManWindow *sawwin,
                      SaWManWindow *relative,
                      bool          top )
{
     DirectResult  ret;
     int           index = 0;
     SaWManWindow *other;
     CoreWindow   *window;

     D_DEBUG_AT( SaWMan_Stacking, "%s( %p, %p, %p, %s )\n", __FUNCTION__,
                 sawman, sawwin, relative, top ? "top" : "below" );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

#ifndef OLD_COREWINDOWS_STRUCTURE
     /* In case of a sub window, the order from sub window vector is followed */
     if (window->toplevel) {
          CoreWindow   *toplevel = window->toplevel;
          CoreWindow   *tmp;
          SaWManWindow *parent;

          /* Enforce association rules... */
          parent = sawwin->parent;
          if (parent) {
               D_MAGIC_ASSERT( parent, SaWManWindow );

               tmp = parent->window;
               D_ASSERT( tmp != NULL );
               D_ASSERT( tmp->toplevel == toplevel );

               if (window->config.options & DWOP_KEEP_UNDER) {
                    int under;

                    index = fusion_vector_index_of( &toplevel->subwindows, window );
                    under = fusion_vector_index_of( &toplevel->subwindows, parent );

                    if (index < under - 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving under (%d->%d)\n", index, under - 1 );

                         fusion_vector_move( &toplevel->subwindows, index, under - 1 );
                    }
                    else if (index > under - 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving under (%d<-%d)\n", under, index );

                         fusion_vector_move( &toplevel->subwindows, index, under );
                    }
               }
               else if (window->config.options & DWOP_KEEP_ABOVE) {
                    int above;

                    index = fusion_vector_index_of( &toplevel->subwindows, window );
                    above = fusion_vector_index_of( &toplevel->subwindows, parent );

                    if (index < above + 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving above (%d->%d)\n", index, above );

                         fusion_vector_move( &toplevel->subwindows, index, above );
                    }
                    else if (index > above + 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving above (%d<-%d)\n", above + 1, index );

                         fusion_vector_move( &toplevel->subwindows, index, above + 1 );
                    }
               }
          }

          /* Lookup our index in top level window */
          index = fusion_vector_index_of( &toplevel->subwindows, window );

          D_DEBUG_AT( SaWMan_Stacking, "  -> toplevel %p [%4d,%4d-%4dx%4d] (%d)\n",
                      toplevel, DFB_RECTANGLE_VALS(&toplevel->config.bounds), index );

          /* Get sub window below (or top level) */
          if (index == 0)
               tmp = toplevel;
          else
               tmp = fusion_vector_at( &toplevel->subwindows, index - 1 );

          D_DEBUG_AT( SaWMan_Stacking, "  -> relative %p [%4d,%4d-%4dx%4d] (%d)\n",
                      tmp, DFB_RECTANGLE_VALS(&tmp->config.bounds), index - 1 );

          /* Place on top */
          relative = tmp->window_data;
          top      = true;
     }
     else
#endif
     if (sawwin->parent && (window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER))) {
          D_MAGIC_ASSERT( sawwin->parent, SaWManWindow );

          relative = sawwin->parent;
          top      = (window->config.options & DWOP_KEEP_ABOVE) ? true : false;

          D_MAGIC_ASSERT( relative, SaWManWindow );

#ifndef OLD_COREWINDOWS_STRUCTURE
          if (top && relative->window->subwindows.count) {
               CoreWindow   *tmp;

               tmp      = fusion_vector_at( &relative->window->subwindows, relative->window->subwindows.count - 1 );
               relative = tmp->window_data;

               D_MAGIC_ASSERT( relative, SaWManWindow );
          }
#endif
     }


     if (relative)
          D_ASSUME( relative->priority == sawwin->priority );

     if (relative) {
          index = sawman_window_index( sawman, relative );
          D_ASSERT( index >= 0 );
          D_ASSERT( index < sawman->layout.count );

          if (top)
               index++;
     }
     else if (top) {
          /*
           * Iterate from bottom to top,
           * stopping at the first window with a higher priority.
           */
          fusion_vector_foreach (other, index, sawman->layout) {
               D_MAGIC_ASSERT( other, SaWManWindow );

               if (other->priority > sawwin->priority)
                    break;
          }
     }
     else {
          /*
           * Iterate from bottom to top,
           * stopping at the first window with equal or higher priority.
           */
          fusion_vector_foreach (other, index, sawman->layout) {
               D_MAGIC_ASSERT( other, SaWManWindow );

               if (other->priority >= sawwin->priority)
                    break;
          }
     }

     /* (Re)Insert the window at the acquired position. */
     if (sawwin->flags & SWMWF_INSERTED) {
          int old = sawman_window_index( sawman, sawwin );

          D_ASSERT( old >= 0 );
          D_ASSERT( old < sawman->layout.count );

          if (old < index)
               index--;

          if (old != index)
               fusion_vector_move( &sawman->layout, old, index );
     }
     else {
          ret = fusion_vector_insert( &sawman->layout, sawwin, index );
          if (ret)
               return ret;

          /* Set 'inserted' flag. */
          sawwin->flags |= SWMWF_INSERTED;
     }

     return DFB_OK;
}

DirectResult
sawman_remove_window( SaWMan       *sawman,
                      SaWManWindow *sawwin )
{
     int               index;
     CoreWindow       *window;
     SaWManGrabbedKey *key;
     DirectLink       *next;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_BUG( "window %d not inserted", window->id );
          return DFB_BUG;
     }

     sawman_withdraw_window( sawman, sawwin );

     index = sawman_window_index( sawman, sawwin );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < sawman->layout.count );

     fusion_vector_remove( &sawman->layout, index );

     /* Release all explicit key grabs. */
     direct_list_foreach_safe (key, next, sawman->grabbed_keys) {
          if (key->owner == sawwin) {
               direct_list_remove( &sawman->grabbed_keys, &key->link );
               SHFREE( sawwin->shmpool, key );
          }
     }

     /* Release grab of unselected keys. */
     if (sawman->unselkeys_window == sawwin)
          sawman->unselkeys_window = NULL;

     /* Free key list. */
     if (window->config.keys) {
          SHFREE( sawwin->shmpool, window->config.keys );

          window->config.keys     = NULL;
          window->config.num_keys = 0;
     }

     sawwin->flags &= ~SWMWF_INSERTED;

     return DFB_OK;
}

DirectResult
sawman_withdraw_window( SaWMan       *sawman,
                        SaWManWindow *sawwin )
{
     int         i, index;
     CoreWindow *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_BUG( "window %d not inserted", window->id );
          return DFB_BUG;
     }

     /* No longer be the 'entered window'. */
     if (sawman->entered_window == sawwin)
          sawman->entered_window = NULL;

     /* Remove focus from window. */
     if (sawman->focused_window == sawwin) {
          SaWManWindow *swin;
          CoreWindow   *cwin;

          sawman->focused_window = NULL;

          /* Always try to have a focused window */
          fusion_vector_foreach_reverse (swin, index, sawman->layout) {
               D_MAGIC_ASSERT( swin, SaWManWindow );

               cwin = swin->window;
               D_ASSERT( cwin != NULL );

               if (swin != sawwin && cwin->config.opacity && !(cwin->config.options & DWOP_GHOST)) {
                    sawman_switch_focus( sawman, swin );
                    break;
               }
          }
     }

#ifndef OLD_COREWINDOWS_STRUCTURE
     if (window->toplevel) {
          CoreWindow *toplevel = window->toplevel;

          D_MAGIC_ASSERT( toplevel, CoreWindow );

          if (toplevel->subfocus == window)
               toplevel->subfocus = NULL;
     }
#endif

     /* Release explicit keyboard grab. */
     if (sawman->keyboard_window == sawwin)
          sawman->keyboard_window = NULL;

     /* Release explicit pointer grab. */
     if (sawman->pointer_window == sawwin)
          sawman->pointer_window = NULL;

     /* Release all implicit key grabs. */
     for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
          if (sawman->keys[i].code != -1 && sawman->keys[i].owner == sawwin) {
               if (!DFB_WINDOW_DESTROYED( window )) {
                    DFBWindowEvent we;

                    we.type       = DWET_KEYUP;
                    we.key_code   = sawman->keys[i].code;
                    we.key_id     = sawman->keys[i].id;
                    we.key_symbol = sawman->keys[i].symbol;

                    sawman_post_event( sawman, sawwin, &we );
               }

               sawman->keys[i].code  = -1;
               sawman->keys[i].owner = NULL;
          }
     }

     /* Hide window. */
     if (SAWMAN_VISIBLE_WINDOW(window)) {
          window->config.opacity = 0;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_FORCE_INVISIBLE | SWMUF_UPDATE_BORDER );
     }

     return DFB_OK;
}

static void
apply_geometry( const DFBWindowGeometry *geometry,
                const DFBRegion         *clip,
                const DFBWindowGeometry *parent,
                DFBRectangle            *ret_rect )
{
     int width, height;

     D_ASSERT( geometry != NULL );
     DFB_REGION_ASSERT( clip );
     D_ASSERT( ret_rect != NULL );

     width  = clip->x2 - clip->x1 + 1;
     height = clip->y2 - clip->y1 + 1;

     switch (geometry->mode) {
          case DWGM_DEFAULT:
               D_DEBUG_AT( SaWMan_Geometry, " -- default\n" );
               *ret_rect = DFB_RECTANGLE_INIT_FROM_REGION( clip );
               D_DEBUG_AT( SaWMan_Geometry, " => %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( ret_rect ) );
               return;

          case DWGM_FOLLOW:
               D_ASSERT( parent != NULL );
               D_DEBUG_AT( SaWMan_Geometry, " -- FOLLOW\n" );
               apply_geometry( parent, clip, NULL, ret_rect );
               break;

          case DWGM_RECTANGLE:
               D_DEBUG_AT( SaWMan_Geometry, " -- RECTANGLE [%d,%d-%dx%d]\n",
                           DFB_RECTANGLE_VALS( &geometry->rectangle ) );
               *ret_rect = geometry->rectangle;
               ret_rect->x += clip->x1;
               ret_rect->y += clip->y1;
               break;

          case DWGM_LOCATION:
               D_DEBUG_AT( SaWMan_Geometry, " -- LOCATION [%.3f,%.3f-%.3fx%.3f]\n",
                           geometry->location.x, geometry->location.y,
                           geometry->location.w, geometry->location.h );
               ret_rect->x = (int)(geometry->location.x * width  + 0.5f) + clip->x1;
               ret_rect->y = (int)(geometry->location.y * height + 0.5f) + clip->y1;
               ret_rect->w = (int)(geometry->location.w * width  + 0.5f);
               ret_rect->h = (int)(geometry->location.h * height + 0.5f);
               break;

          default:
               D_BUG( "invalid geometry mode %d", geometry->mode );
               return;
     }

     D_DEBUG_AT( SaWMan_Geometry, " -> %d,%d-%dx%d / clip [%d,%d-%dx%d]\n",
                 DFB_RECTANGLE_VALS( ret_rect ),
                 DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     if (!dfb_rectangle_intersect_by_region( ret_rect, clip )) {
          D_WARN( "invalid geometry" );
          dfb_rectangle_from_region( ret_rect, clip );
     }

     D_DEBUG_AT( SaWMan_Geometry, " => %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( ret_rect ) );
}

DirectResult
sawman_update_geometry( SaWManWindow *sawwin )
{
     int           i;
     CoreWindow   *window;
     CoreWindow   *parent_window = NULL;
     CoreWindow   *toplevel;
     SaWManWindow *topsaw = NULL;
     SaWMan       *sawman;
     SaWManWindow *parent;
     SaWManWindow *child;
     CoreWindow   *childwin;
     DFBRegion     clip;
     DFBRectangle  src;
     DFBRectangle  dst;
     bool          src_updated = false;
     bool          dst_updated = false;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_DEBUG_AT( SaWMan_Geometry, "%s( %p )\n", __FUNCTION__, sawwin );

     sawman = sawwin->sawman;
     D_MAGIC_ASSERT_IF( sawman, SaWMan );

     if (sawman)
          FUSION_SKIRMISH_ASSERT( &sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     parent = sawwin->parent;
     if (parent) {
          D_MAGIC_ASSERT( parent, SaWManWindow );

          parent_window = parent->window;
          D_ASSERT( parent_window != NULL );
     }

     toplevel = WINDOW_TOPLEVEL(window);
     if (toplevel) {
          topsaw = toplevel->window_data;
          D_MAGIC_ASSERT( topsaw, SaWManWindow );
     }

     if (parent && (window->config.options & DWOP_FOLLOW_BOUNDS))
          /* Initialize bounds from parent window (window association) */
          sawwin->bounds = parent->bounds;
     else
          /* Initialize bounds from base window configuration */
          sawwin->bounds = window->config.bounds;

     /*
      * In case of a sub window, the top level surface is the coordinate space instead of the layer surface
      */
     toplevel = WINDOW_TOPLEVEL(window);
     if (toplevel) {
          DFBDimension in, out;

          D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (base)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );

          D_DEBUG_AT( SaWMan_Geometry, "  o- top src    %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS(&topsaw->src) );
          D_DEBUG_AT( SaWMan_Geometry, "  o- top dst    %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS(&topsaw->dst) );

          /*
           * Translate against top level source geometry
           */
          sawwin->bounds.x -= topsaw->src.x;
          sawwin->bounds.y -= topsaw->src.y;

          D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (translated)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );

          /*
           * Take input dimension from top level source geometry
           */
          in.w = topsaw->src.w;
          in.h = topsaw->src.h;

          /*
           * Take output dimension from top level destination geometry
           */
          out.w = topsaw->dst.w;
          out.h = topsaw->dst.h;

          /*
           * Scale the sub window size if top level window is scaled
           */
          if (in.w != out.w || in.h != out.h) {
               D_DEBUG_AT( SaWMan_Geometry, "  o- scale in             %4dx%4d\n", in.w, in.h );
               D_DEBUG_AT( SaWMan_Geometry, "  o- scale out            %4dx%4d\n", out.w, out.h );

               sawwin->bounds.x = sawwin->bounds.x * out.w / in.w;
               sawwin->bounds.y = sawwin->bounds.y * out.h / in.h;
               sawwin->bounds.w = sawwin->bounds.w * out.w / in.w;
               sawwin->bounds.h = sawwin->bounds.h * out.h / in.h;

               D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (scaled)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );
          }

          /*
           * Translate to top level destination geometry
           */
          sawwin->bounds.x += topsaw->dst.x;
          sawwin->bounds.y += topsaw->dst.y;

          D_DEBUG_AT( SaWMan_Geometry, "  => sub bounds %4d,%4d-%4dx%4d (translated)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );
     }

     /* Calculate source geometry. */
     clip.x1 = 0;
     clip.y1 = 0;

     if (window->caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR)) {
          clip.x2 = sawwin->bounds.w - 1;
          clip.y2 = sawwin->bounds.h - 1;
     }
     else {
          CoreSurface *surface = window->surface;
          D_ASSERT( surface != NULL );

          clip.x2 = surface->config.size.w - 1;
          clip.y2 = surface->config.size.h - 1;
     }

     D_DEBUG_AT( SaWMan_Geometry, "  -> Applying source geometry...\n" );

     apply_geometry( &window->config.src_geometry, &clip,
                     parent_window ? &parent_window->config.src_geometry : NULL, &src );

     /* Calculate destination geometry. */
     clip = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );

     D_DEBUG_AT( SaWMan_Geometry, "  -> Applying destination geometry...\n" );

     apply_geometry( &window->config.dst_geometry, &clip,
                     parent_window ? &parent_window->config.dst_geometry : NULL, &dst );

     /* Adjust src/dst if clipped by top level window */
     if (toplevel) {
          DFBRegion topclip = DFB_REGION_INIT_FROM_RECTANGLE( &topsaw->dst );

          /*
           * Clip the sub window bounds against the top level window
           */
          dfb_clip_stretchblit( &topclip, &src, &dst );

          D_DEBUG_AT( SaWMan_Geometry, "  => sub dst    %4d,%4d-%4dx%4d (clipped)\n", DFB_RECTANGLE_VALS(&dst) );
          D_DEBUG_AT( SaWMan_Geometry, "  => sub src    %4d,%4d-%4dx%4d (clipped)\n", DFB_RECTANGLE_VALS(&src) );
     }

     /* Update source geometry. */
     if (!DFB_RECTANGLE_EQUAL( src, sawwin->src )) {
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );

          sawwin->src = src;
          src_updated = true;
     }

     /* Update destination geometry. */
     if (!DFB_RECTANGLE_EQUAL( dst, sawwin->dst )) {
          if (!src_updated)
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );

          sawwin->dst = dst;
          dst_updated = true;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
     }

     D_DEBUG_AT( SaWMan_Geometry, "  -> Updating children (associated windows)...\n" );

     fusion_vector_foreach (child, i, sawwin->children) {
          D_MAGIC_ASSERT( child, SaWManWindow );

          childwin = child->window;
          D_ASSERT( childwin != NULL );

          if ((childwin->config.src_geometry.mode == DWGM_FOLLOW && src_updated) ||
              (childwin->config.dst_geometry.mode == DWGM_FOLLOW && dst_updated) ||
              (childwin->config.options & DWOP_FOLLOW_BOUNDS))
               sawman_update_geometry( child );
     }

#ifndef OLD_COREWINDOWS_STRUCTURE
     D_DEBUG_AT( SaWMan_Geometry, "  -> Updating children (sub windows)...\n" );

     fusion_vector_foreach (childwin, i, window->subwindows) {
          D_ASSERT( childwin != NULL );

          sawman_update_geometry( childwin->window_data );
     }
#endif

     return DFB_OK;
}

int
sawman_window_border( const SaWManWindow *sawwin )
{
     SaWMan                 *sawman;
     const CoreWindow       *window;
     const SaWManTier       *tier;
     const SaWManBorderInit *border;
     int                     thickness = 0;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     sawman = sawwin->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     if (sawwin->caps & DWCAPS_NODECORATION)
          return 0;

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     tier = sawman_tier_by_class( sawwin->sawman, window->config.stacking );
     D_MAGIC_ASSERT( tier, SaWManTier );

     D_ASSERT( sawman_config != NULL );

     border = &sawman_config->borders[sawman_window_priority(sawwin)];

     thickness = border->thickness;
     if (thickness && border->resolution.w && border->resolution.h) {
          if (border->resolution.w != tier->size.w && border->resolution.h != tier->size.h) {
               int tw = thickness * tier->size.w / border->resolution.w;
               int th = thickness * tier->size.h / border->resolution.h;

               thickness = (tw + th + 1) / 2;
          }
     }

     return thickness;
}

/**********************************************************************************************************************/

/*
     skipping opaque windows that are above the window that changed
*/
static void
wind_of_change( SaWMan              *sawman,
                SaWManTier          *tier,
                DFBRegion           *update,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( update != NULL );

     /*
          loop through windows above
     */
     for (; current > changed; current--) {
          CoreWindow       *window;
          SaWManWindow     *sawwin;
          DFBRegion         opaque;
          DFBWindowOptions  options;

          D_ASSERT( changed >= 0 );
          D_ASSERT( current >= changed );
          D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

          sawwin = fusion_vector_at( &sawman->layout, current );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          options = window->config.options;

          D_DEBUG_AT( SaWMan_Update, "--[%p] %4d,%4d-%4dx%4d : %d->%d\n",
                      tier, DFB_RECTANGLE_VALS_FROM_REGION( update ), current, changed );

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    sawwin->dst.x, sawwin->dst.y,
                                                    sawwin->dst.x + sawwin->dst.w - 1,
                                                    sawwin->dst.y + sawwin->dst.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       sawwin->dst.x + window->config.opaque.x1,
                                                       sawwin->dst.y + window->config.opaque.y1,
                                                       sawwin->dst.x + window->config.opaque.x2,
                                                       sawwin->dst.y + window->config.opaque.y2 ))
                 )  ))
          {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_change( sawman, tier, &left, flags, current-1, changed );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_change( sawman, tier, &upper, flags, current-1, changed );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_change( sawman, tier, &right, flags, current-1, changed );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_change( sawman, tier, &lower, flags, current-1, changed );
               }

               return;
          }
     }

     D_DEBUG_AT( SaWMan_Update, "+ UPDATE %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( update ) );

     dfb_updates_add( &tier->updates, update );
}

static void
wind_of_showing( SaWMan     *sawman,
                 SaWManTier *tier,
                 DFBRegion  *update,
                 int         current,
                 int         changed,
                 bool       *ret_showing )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( update != NULL );

     if (*ret_showing)
          return;

     /*
          loop through windows above
     */
     for (; current > changed; current--) {
          CoreWindow       *window;
          SaWManWindow     *sawwin;
          DFBRegion         opaque;
          DFBWindowOptions  options;

          D_ASSERT( changed >= 0 );
          D_ASSERT( current >= changed );
          D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

          sawwin = fusion_vector_at( &sawman->layout, current );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          options = window->config.options;

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    sawwin->dst.x, sawwin->dst.y,
                                                    sawwin->dst.x + sawwin->dst.w - 1,
                                                    sawwin->dst.y + sawwin->dst.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       sawwin->dst.x + window->config.opaque.x1,
                                                       sawwin->dst.y + window->config.opaque.y1,
                                                       sawwin->dst.x + window->config.opaque.x2,
                                                       sawwin->dst.y + window->config.opaque.y2 ))
                 )  ))
          {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_showing( sawman, tier, &left,  current-1, changed, ret_showing );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_showing( sawman, tier, &upper, current-1, changed, ret_showing );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_showing( sawman, tier, &right, current-1, changed, ret_showing );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_showing( sawman, tier, &lower, current-1, changed, ret_showing );
               }

               return;
          }
     }

     *ret_showing = true;
}

static void
update_region( SaWMan          *sawman,
               SaWManTier      *tier,
               CardState       *state,
               int              start,
               int              x1,
               int              y1,
               int              x2,
               int              y2 )
{
     int           i      = start;
     DFBRegion     region = { x1, y1, x2, y2 };
     CoreWindow   *window = NULL;
     SaWManWindow *sawwin = NULL;

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %d, %d,%d - %d,%d )\n", __FUNCTION__, tier, start, x1, y1, x2, y2 );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( start < fusion_vector_size( &sawman->layout ) );
     D_ASSUME( x1 <= x2 );
     D_ASSUME( y1 <= y2 );

     if (x1 > x2 || y1 > y2)
          return;

     /* Find next intersecting window. */
     while (i >= 0) {
          sawwin = fusion_vector_at( &sawman->layout, i );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_VISIBLE_WINDOW( window ) && (tier->classes & (1 << window->config.stacking))) {
               if (dfb_region_intersect( &region,
                                         DFB_REGION_VALS_FROM_RECTANGLE( &sawwin->bounds )))
                    break;
          }

          i--;
     }

     /* Intersecting window found? */
     if (i >= 0) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (D_FLAGS_ARE_SET( window->config.options, DWOP_ALPHACHANNEL | DWOP_OPAQUE_REGION )) {
               DFBRegion opaque = DFB_REGION_INIT_TRANSLATED( &window->config.opaque,
                                                              sawwin->bounds.x,
                                                              sawwin->bounds.y );

               if (!dfb_region_region_intersect( &opaque, &region )) {
                    update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );

                    sawman_draw_window( tier, sawwin, state, &region, true );
               }
               else {
                    if ((window->config.opacity < 0xff) || (window->config.options & DWOP_COLORKEYING)) {
                         /* draw everything below */
                         update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );
                    }
                    else {
                         /* left */
                         if (opaque.x1 != x1)
                              update_region( sawman, tier, state, i-1, x1, opaque.y1, opaque.x1-1, opaque.y2 );

                         /* upper */
                         if (opaque.y1 != y1)
                              update_region( sawman, tier, state, i-1, x1, y1, x2, opaque.y1-1 );

                         /* right */
                         if (opaque.x2 != x2)
                              update_region( sawman, tier, state, i-1, opaque.x2+1, opaque.y1, x2, opaque.y2 );

                         /* lower */
                         if (opaque.y2 != y2)
                              update_region( sawman, tier, state, i-1, x1, opaque.y2+1, x2, y2 );
                    }

                    /* left */
                    if (opaque.x1 != region.x1) {
                         DFBRegion r = { region.x1, opaque.y1, opaque.x1 - 1, opaque.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* upper */
                    if (opaque.y1 != region.y1) {
                         DFBRegion r = { region.x1, region.y1, region.x2, opaque.y1 - 1 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* right */
                    if (opaque.x2 != region.x2) {
                         DFBRegion r = { opaque.x2 + 1, opaque.y1, region.x2, opaque.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* lower */
                    if (opaque.y2 != region.y2) {
                         DFBRegion r = { region.x1, opaque.y2 + 1, region.x2, region.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* inner */
                    sawman_draw_window( tier, sawwin, state, &opaque, false );
               }
          }
          else {
               if (SAWMAN_TRANSLUCENT_WINDOW( window )) {
                    /* draw everything below */
                    update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );
               }
               else {
                    DFBRegion dst = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );

                    dfb_region_region_intersect( &dst, &region );

                    /* left */
                    if (dst.x1 != x1)
                         update_region( sawman, tier, state, i-1, x1, dst.y1, dst.x1-1, dst.y2 );

                    /* upper */
                    if (dst.y1 != y1)
                         update_region( sawman, tier, state, i-1, x1, y1, x2, dst.y1-1 );

                    /* right */
                    if (dst.x2 != x2)
                         update_region( sawman, tier, state, i-1, dst.x2+1, dst.y1, x2, dst.y2 );

                    /* lower */
                    if (dst.y2 != y2)
                         update_region( sawman, tier, state, i-1, x1, dst.y2+1, x2, y2 );
               }

               sawman_draw_window( tier, sawwin, state, &region, true );
          }
     }
     else
          sawman_draw_background( tier, state, &region );
}

static void
repaint_tier( SaWMan              *sawman,
              SaWManTier          *tier,
              const DFBRegion     *updates,
              int                  num_updates,
              DFBSurfaceFlipFlags  flags )
{
     int              i;
     CoreLayer       *layer;
     CoreLayerRegion *region;
     CardState       *state;
     CoreSurface     *surface;
     DFBRegion        cursor_inter;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( updates != NULL );
     D_ASSERT( num_updates > 0 );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     stack = tier->stack;
     D_ASSERT( stack != NULL );

     region = tier->region;
     D_ASSERT( region != NULL );

     layer   = dfb_layer_at( tier->layer_id );
     state   = &layer->state;
     surface = region->surface;

     if (/*!data->active ||*/ !surface)
          return;

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %p )\n", __FUNCTION__, sawman, tier );

     /* Set destination. */
     state->destination  = surface;
     state->modified    |= SMF_DESTINATION;

     for (i=0; i<num_updates; i++) {
          const DFBRegion *update = &updates[i];

          DFB_REGION_ASSERT( update );

          D_DEBUG_AT( SaWMan_Update, "  -> %d, %d - %dx%d  (%d)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( update ), i );

          dfb_state_set_dst_colorkey( state, dfb_color_to_pixel( region->config.format,
                                                                 region->config.src_key.r,
                                                                 region->config.src_key.g,
                                                                 region->config.src_key.b ) );

          /* Set clipping region. */
          dfb_state_set_clip( state, update );

          /* Compose updated region. */
          update_region( sawman, tier, state,
                         fusion_vector_size( &sawman->layout ) - 1,
                         update->x1, update->y1, update->x2, update->y2 );

          /* Update cursor? */
          cursor_inter = tier->cursor_region;
          if (tier->cursor_drawn && dfb_region_region_intersect( &cursor_inter, update )) {
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &cursor_inter );

               D_ASSUME( tier->cursor_bs_valid );

               dfb_gfx_copy_to( surface, tier->cursor_bs, &rect,
                                rect.x - tier->cursor_region.x1,
                                rect.y - tier->cursor_region.y1, true );

               sawman_draw_cursor( stack, state, &cursor_inter );
          }
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;

     /* Software cursor code relies on a valid back buffer. */
     if (stack->cursor.enabled)
          flags |= DSFLIP_BLIT;

     for (i=0; i<num_updates; i++) {
          const DFBRegion *update = &updates[i];

          DFB_REGION_ASSERT( update );

          /* Flip the updated region .*/
          dfb_layer_region_flip_update( region, update, flags );
     }

#ifdef SAWMAN_DUMP_TIER_FRAMES
     {
          DFBResult          ret;
          CoreSurfaceBuffer *buffer;

          D_MAGIC_ASSERT( surface, CoreSurface );

          if (fusion_skirmish_prevail( &surface->lock ))
               return;

          buffer = dfb_surface_get_buffer( surface, CSBR_FRONT );
          D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

          ret = dfb_surface_buffer_dump( buffer, "/", "tier" );

          fusion_skirmish_dismiss( &surface->lock );
     }
#endif
}

static SaWManWindow *
get_single_window( SaWMan     *sawman,
                   SaWManTier *tier,
                   bool       *ret_none )
{
     int           n;
     SaWManWindow *sawwin;
     SaWManWindow *single = NULL;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_VISIBLE_WINDOW(window) && (tier->classes & (1 << window->config.stacking))) {
               if (single || (window->caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR) ))
                    return NULL;

               single = sawwin;

               if (single->dst.x == 0 &&
                   single->dst.y == 0 &&
                   single->dst.w == tier->size.w &&
                   single->dst.h == tier->size.h &&
                   !SAWMAN_TRANSLUCENT_WINDOW(window))
                    break;
          }
     }

     if (ret_none && !single)
          *ret_none = true;

     return single;
}

static bool
get_border_only( SaWMan     *sawman,
                 SaWManTier *tier )
{
     int           n;
     SaWManWindow *sawwin;
     bool          none = true;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          none = false;

          if (SAWMAN_VISIBLE_WINDOW(window) && !(window->caps & DWCAPS_INPUTONLY))
               return false;
     }

     return !none;
}

/* FIXME: Split up in smaller functions and clean up things like forcing reconfiguration. */
DirectResult
sawman_process_updates( SaWMan              *sawman,
                        DFBSurfaceFlipFlags  flags )
{
     DirectResult  ret;
     int           idx = -1;
     SaWManTier   *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, 0x%08x )\n", __FUNCTION__, sawman, flags );

     direct_list_foreach (tier, sawman->tiers) {
          int              n, d;
          int              total;
          int              bounding;
          bool             none = false;
          bool             border_only;
          SaWManWindow    *single;
          CoreLayer       *layer;
          CoreLayerShared *shared;
          int              screen_width;
          int              screen_height;
          DFBColorKey      single_key;

          idx++;

          layer = dfb_layer_at( tier->layer_id );
          D_ASSERT( layer != NULL );

          shared = layer->shared;
          D_ASSERT( shared != NULL );

          D_MAGIC_ASSERT( tier, SaWManTier );

          if (!tier->updates.num_regions)
               continue;

          D_DEBUG_AT( SaWMan_Update, "  -> %d updates (tier %d, layer %d)\n",
                      tier->updates.num_regions, idx, tier->layer_id );

          D_ASSERT( tier->region != NULL );

          D_DEBUG_AT( SaWMan_Update, "  -> [%d] %d updates, bounding %dx%d\n",
                      tier->layer_id, tier->updates.num_regions,
                      tier->updates.bounding.x2 - tier->updates.bounding.x1 + 1,
                      tier->updates.bounding.y2 - tier->updates.bounding.y1 + 1 );

          if (!tier->config.width || !tier->config.height)
               continue;

          dfb_screen_get_screen_size( layer->screen, &screen_width, &screen_height );

          single = get_single_window( sawman, tier, &none );

          if (none && !sawman_config->show_empty) {
               if (tier->active) {
                    D_DEBUG_AT( SaWMan_Auto, "  -> Disabling region...\n" );

                    tier->active        = false;
                    tier->single_window = NULL;  /* enforce configuration to reallocate buffers */

                    dfb_layer_region_disable( tier->region );
               }
               dfb_updates_reset( &tier->updates );
               continue;
          }

          border_only = get_border_only( sawman, tier );

          /* Remember color key before single mode is activated. */
          if (!tier->single_mode)
               tier->key = tier->context->primary.config.src_key;


          /* If the first mode after turning off the layer is not single, then we need
             this to force a reconfiguration to reallocate the buffers. */
          if (!tier->active) {
               tier->single_mode = true;               /* avoid endless loop */
               tier->border_only = !border_only;       /* enforce configuration to reallocate buffers */
          }

          if (single && !border_only) {
               CoreWindow             *window;
               CoreSurface            *surface;
               DFBDisplayLayerOptions  options = DLOP_NONE;
               DFBRectangle            dst  = single->dst;
               DFBRectangle            src  = single->src;
               DFBRegion               clip = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

               if (shared->description.caps & DLCAPS_SCREEN_LOCATION) {
                    dst.x = dst.x * screen_width  / tier->size.w;
                    dst.y = dst.y * screen_height / tier->size.h;
                    dst.w = dst.w * screen_width  / tier->size.w;
                    dst.h = dst.h * screen_height / tier->size.h;
               }
               else {
                    if (dst.w != src.w || dst.h != src.h)
                         goto no_single;

                    if (shared->description.caps & DLCAPS_SCREEN_POSITION) {
                         dfb_rectangle_intersect_by_region( &dst, &clip );

                         src.x += dst.x - single->dst.x;
                         src.y += dst.y - single->dst.y;
                         src.w  = dst.w;
                         src.h  = dst.h;

                         dst.x += (screen_width  - tier->size.w) / 2;
                         dst.y += (screen_height - tier->size.h) / 2;
                    }
               }

#ifdef SAWMAN_NO_LAYER_DOWNSCALE
               if (rect.w < src.w)
                    goto no_single;
#endif

#ifdef SAWMAN_NO_LAYER_DST_WINDOW
               if (dst.x != 0 || dst.y != 0 || dst.w != screen_width || dst.h != screen_height)
                    goto no_single;
#endif


               window = single->window;
               D_MAGIC_COREWINDOW_ASSERT( window );

               surface = window->surface;
               D_ASSERT( surface != NULL );

               if (window->config.options & DWOP_ALPHACHANNEL)
                    options |= DLOP_ALPHACHANNEL;

               if (window->config.options & DWOP_COLORKEYING)
                    options |= DLOP_SRC_COLORKEY;

               single_key = tier->single_key;

               if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format )) {
                    CorePalette *palette = surface->palette;

                    D_ASSERT( palette != NULL );
                    D_ASSERT( palette->num_entries > 0 );

                    dfb_surface_set_palette( tier->region->surface, surface->palette );

                    if (options & DLOP_SRC_COLORKEY) {
                         int index = window->config.color_key % palette->num_entries;

                         single_key.r     = palette->entries[index].r;
                         single_key.g     = palette->entries[index].g;
                         single_key.b     = palette->entries[index].b;
                         single_key.index = index;
                    }
               }
               else {
                    DFBColor color;

                    dfb_pixel_to_color( surface->config.format, window->config.color_key, &color );

                    single_key.r     = color.r;
                    single_key.g     = color.g;
                    single_key.b     = color.b;
                    single_key.index = window->config.color_key;
               }

               /* Complete reconfig? */
               if (tier->single_window  != single ||
                   !DFB_RECTANGLE_EQUAL( tier->single_src, src ) ||
                   tier->single_format  != surface->config.format ||
                   tier->single_options != options)
               {
                    DFBDisplayLayerConfig  config;

                    D_DEBUG_AT( SaWMan_Auto, "  -> Switching to %dx%d [%dx%d] %s single mode for %p on %p...\n",
                                single->src.w, single->src.h, src.w, src.h,
                                dfb_pixelformat_name( surface->config.format ), single, tier );

                    config.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS | DLCONF_BUFFERMODE;
                    config.width       = src.w;
                    config.height      = src.h;
                    config.pixelformat = surface->config.format;
                    config.options     = options;
                    config.buffermode  = DLBM_FRONTONLY;

                    sawman->callback.layer_reconfig.layer_id = tier->layer_id;
                    sawman->callback.layer_reconfig.single   = (SaWManWindowHandle) single;
                    sawman->callback.layer_reconfig.config   = config;

                    switch (sawman_call( sawman, SWMCID_LAYER_RECONFIG, &sawman->callback.layer_reconfig )) {
                         case DFB_OK:
                              config = sawman->callback.layer_reconfig.config;
                         case DFB_UNIMPLEMENTED:
                              break;

                         default:
                              goto no_single;
                    }

                    if (dfb_layer_context_test_configuration( tier->context, &config, NULL ) != DFB_OK)
                         goto no_single;

                    tier->single_mode     = true;
                    tier->single_window   = single;
                    tier->single_width    = src.w;
                    tier->single_height   = src.h;
                    tier->single_src      = src;
                    tier->single_dst      = dst;
                    tier->single_format   = surface->config.format;
                    tier->single_options  = options;
                    tier->single_key      = single_key;

                    tier->active          = false;
                    tier->region->state  |= CLRSF_FROZEN;

                    dfb_updates_reset( &tier->updates );

                    dfb_layer_context_set_configuration( tier->context, &config );

                    if (shared->description.caps & DLCAPS_SCREEN_LOCATION)
                         dfb_layer_context_set_screenrectangle( tier->context, &dst );
                    else if (shared->description.caps & DLCAPS_SCREEN_POSITION)
                         dfb_layer_context_set_screenposition( tier->context, dst.x, dst.y );

                    dfb_layer_context_set_src_colorkey( tier->context,
                                                        tier->single_key.r, tier->single_key.g,
                                                        tier->single_key.b, tier->single_key.index );

                    dfb_gfx_copy_to( surface, tier->region->surface, &src, 0, 0, false );

                    tier->active = true;

                    dfb_layer_region_flip_update( tier->region, NULL, flags );

                    dfb_updates_reset( &tier->updates );
                    continue;
               }

               /* Update destination window */
               if (!DFB_RECTANGLE_EQUAL( tier->single_dst, dst )) {
                    tier->single_dst = dst;

                    D_DEBUG_AT( SaWMan_Auto, "  -> Changing single destination to %d,%d-%dx%d.\n",
                                DFB_RECTANGLE_VALS(&dst) );

                    dfb_layer_context_set_screenrectangle( tier->context, &dst );
               }
               else
                    dfb_gfx_copy_to( surface, tier->region->surface, &src, 0, 0, false );

               /* Update color key */
               if (!DFB_COLORKEY_EQUAL( single_key, tier->single_key )) {
                    D_DEBUG_AT( SaWMan_Auto, "  -> Changing single color key.\n" );

                    tier->single_key = single_key;

                    dfb_layer_context_set_src_colorkey( tier->context,
                                                        tier->single_key.r, tier->single_key.g,
                                                        tier->single_key.b, tier->single_key.index );
               }

               tier->active = true;

               dfb_layer_region_flip_update( tier->region, NULL, flags );

               dfb_updates_reset( &tier->updates );
               continue;
          }

no_single:

          if (tier->single_mode) {
               D_DEBUG_AT( SaWMan_Auto, "  -> Switching back from single mode...\n" );

               tier->border_only = !border_only;       /* enforce switch */
          }

          /* Switch border/default config? */
          if (tier->border_only != border_only) {
               const DFBDisplayLayerConfig *config;

               tier->border_only = border_only;

               if (border_only)
                    config = &tier->border_config;
               else
                    config = &tier->config;

               D_DEBUG_AT( SaWMan_Auto, "  -> Switching to %dx%d %s %s mode.\n", config->width, config->height,
                           dfb_pixelformat_name( config->pixelformat ), border_only ? "border" : "standard" );

               tier->active         = false;
               tier->region->state |= CLRSF_FROZEN;

               dfb_updates_reset( &tier->updates );

               /* Temporarily to avoid configuration errors. */
               dfb_layer_context_set_screenposition( tier->context, 0, 0 );

               ret = dfb_layer_context_set_configuration( tier->context, config );
               if (ret) {
                    D_DERROR( ret, "SaWMan/Auto: Switching to standard mode failed!\n" );
                    /* fixme */
               }

               tier->size.w = config->width;
               tier->size.h = config->height;

               /* Notify application manager about new tier size if previous mode was single. */
               if (tier->single_mode)
                    sawman_call( sawman, SWMCID_STACK_RESIZED, &tier->size );

               if (shared->description.caps & DLCAPS_SCREEN_LOCATION) {
                    DFBRectangle full = { 0, 0, screen_width, screen_height };

                    dfb_layer_context_set_screenrectangle( tier->context, &full );
               }
               else if (shared->description.caps & DLCAPS_SCREEN_POSITION) {
                    dfb_layer_context_set_screenposition( tier->context,
                                                          (screen_width  - config->width)  / 2,
                                                          (screen_height - config->height) / 2 );
               }

               if (config->options & DLOP_SRC_COLORKEY) {
                    if (DFB_PIXELFORMAT_IS_INDEXED( config->pixelformat )) {
                         int          index;
                         CoreSurface *surface;
                         CorePalette *palette;

                         surface = tier->region->surface;
                         D_MAGIC_ASSERT( surface, CoreSurface );

                         palette = surface->palette;
                         D_ASSERT( palette != NULL );
                         D_ASSERT( palette->num_entries > 0 );

                         index = tier->key.index % palette->num_entries;

                         dfb_layer_context_set_src_colorkey( tier->context,
                                                             palette->entries[index].r,
                                                             palette->entries[index].g,
                                                             palette->entries[index].b,
                                                             index );
                    }
                    else
                         dfb_layer_context_set_src_colorkey( tier->context,
                                                             tier->key.r, tier->key.g, tier->key.b, tier->key.index );
               }
          }

          if (!tier->active) {
               D_DEBUG_AT( SaWMan_Auto, "  -> Activating tier...\n" );

               tier->active = true;

               DFBRegion region = { 0, 0, tier->size.w - 1, tier->size.h - 1 };
               dfb_updates_add( &tier->updates, &region );
          }

          tier->single_mode   = false;
          tier->single_window = NULL;

          if (!tier->updates.num_regions)
               continue;


          dfb_updates_stat( &tier->updates, &total, &bounding );

          n = tier->updates.max_regions - tier->updates.num_regions + 1;
          d = n + 1;

          /* Try to optimize updates. In buffer swapping modes we can save the copy by updating everything. */
          if ((total > tier->size.w * tier->size.h) ||
              (total > tier->size.w * tier->size.h * 3 / 5 && (tier->context->config.buffermode == DLBM_BACKVIDEO ||
                                                               tier->context->config.buffermode == DLBM_TRIPLE)))
          {
               DFBRegion region = { 0, 0, tier->size.w - 1, tier->size.h - 1 };

               repaint_tier( sawman, tier, &region, 1, flags );
          }
          else if (tier->updates.num_regions < 2 || total < bounding * n / d)
               repaint_tier( sawman, tier, tier->updates.regions, tier->updates.num_regions, flags );
          else
               repaint_tier( sawman, tier, &tier->updates.bounding, 1, flags );

          dfb_updates_reset( &tier->updates );
     }

     return DFB_OK;
}

