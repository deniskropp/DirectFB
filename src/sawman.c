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

#include <config.h>

#include <unistd.h>

#include <direct/debug.h>
#include <direct/list.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/windows_internal.h>

#include <misc/conf.h>

#include <sawman.h>
#include <sawman_manager.h>

#include "sawman_config.h"

#include "isawman.h"


D_DEBUG_DOMAIN( SaWMan_Update, "SaWMan/Update", "SaWMan window manager updates" );


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

     unregister_process( sawman, process );

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

          case SWMCID_WINDOW_CONFIG:
               if (sawman->manager.callbacks.WindowConfig)
                    *ret_val = sawman->manager.callbacks.WindowConfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_RESTACK:
               if (sawman->manager.callbacks.WindowRestack)
                    *ret_val = sawman->manager.callbacks.WindowRestack( sawman->manager.context, call_ptr );
               break;

          case SWMCID_STACK_RESIZED:
               if (sawman->manager.callbacks.StackResized)
                    *ret_val = sawman->manager.callbacks.StackResized( sawman->manager.context, call_ptr );
               break;

          case SWMCID_SWITCH_FOCUS:
               if (sawman->manager.callbacks.SwitchFocus)
                    *ret_val = sawman->manager.callbacks.SwitchFocus( sawman->manager.context, call_ptr );
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
     ret = fusion_shm_pool_create( world, "SaWMan Pool", 0x1000000, true, &sawman->shmpool );
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

     /* Initialize tiers. */
     for (i=0; i<D_ARRAY_SIZE(dfb_config->layers); i++) {
          if (!dfb_config->layers[i].stacking)
               continue;

          ret = add_tier( sawman, i, dfb_config->layers[i].stacking );
          if (ret) {
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
          D_MAGIC_CLEAR( sawman );
          goto error;
     }

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
     DirectResult   ret;
     DirectLink    *l, *next;
     SaWManProcess *process;
     SaWManWindow  *sawwin;

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

          D_WARN( "window %d,%d-%dx%d still there", DFB_RECTANGLE_VALS( &sawwin->window->config.bounds ) );

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
     direct_list_foreach_safe (l, next, sawman->grabbed_keys)
          SHFREE( sawman->shmpool, l );

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

          case SWMCID_WINDOW_CONFIG:
               if (!sawman->manager.callbacks.WindowConfig)
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
               sawman_update_window( sawman, from, NULL, DSFLIP_NONE, false, false, false );
     }

     if (to) {
          CoreWindow *window = to->window;

          D_ASSERT( window != NULL );

          if (window->surface && window->surface->palette) {
               CoreSurface *surface;

               D_ASSERT( window->primary_region != NULL );

               if (dfb_layer_region_get_surface( window->primary_region, &surface ) == DFB_OK) {
                    if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                         dfb_surface_set_palette( surface, window->surface->palette );

                    dfb_surface_unref( surface );
               }
          }

          evt.type = DWET_GOTFOCUS;

          sawman_post_event( sawman, to, &evt );

          if (sawman_window_border( to ))
               sawman_update_window( sawman, to, NULL, DSFLIP_NONE, false, false, false );
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
                      bool                 force_complete,
                      bool                 force_invisible,
                      bool                 scale_region )
{
     DFBRegion        area;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     DFB_REGION_ASSERT_IF( region );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %p )\n", __FUNCTION__, sawwin, region );

     if (!SAWMAN_VISIBLE_WINDOW(window) && !force_invisible)
          return DFB_OK;

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED))
          return DFB_OK;

     window = sawwin->window;
     D_ASSERT( window != NULL );

     tier = sawman_tier_by_class( sawman, window->config.stacking );

     if (region) {
          if (scale_region && (window->config.options & DWOP_SCALE)) {
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
     else
          area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );

     if (!dfb_unsafe_region_intersect( &area, 0, 0, tier->size.w - 1, tier->size.h - 1 ))
          return DFB_OK;

     if (force_complete/* || sawwin->children.count*/)
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

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_ASSERT( window != NULL );

     if (!sawman_tier_by_stack( sawman, stack, &tier ))
          return DFB_BUG;

     *ret_showing = false;

     if (!SAWMAN_VISIBLE_WINDOW(window))
          return DFB_OK;

     while (sawwin->parent)
          sawwin = sawwin->parent;

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED))
          return DFB_OK;

     area = DFB_REGION_INIT_FROM_RECTANGLE( &window->config.bounds );

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

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     if (sawwin->parent && (window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER))) {
          D_MAGIC_ASSERT( sawwin->parent, SaWManWindow );

          relative = sawwin->parent;
          top      = (window->config.options & DWOP_KEEP_ABOVE) ? true : false;
     }
     else if (relative)
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
          int old = fusion_vector_index_of( &sawman->layout, sawwin );

          old = sawman_window_index( sawman, sawwin );
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

     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_BUG( "window not inserted" );
          return DFB_BUG;
     }

     window = sawwin->window;
     D_ASSERT( window != NULL );

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
          SHFREE( sawman->shmpool, window->config.keys );

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

     window = sawwin->window;
     D_ASSERT( window != NULL );

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
     if (SAWMAN_VISIBLE_WINDOW(window) && window->config.opacity > 0) {
          window->config.opacity = 0;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, true, false );
     }

     return DFB_OK;
}

static void
apply_geometry( const DFBWindowGeometry *geometry,
                const DFBRegion         *clip,
                const DFBRectangle      *parent,
                DFBRectangle            *ret_rect )
{
     int width, height;

     D_ASSERT( geometry != NULL );
     DFB_REGION_ASSERT( clip );
     DFB_RECTANGLE_ASSERT_IF( parent );
     D_ASSERT( ret_rect != NULL );

     width  = clip->x2 - clip->x1 + 1;
     height = clip->y2 - clip->y1 + 1;

     switch (geometry->mode) {
          case DWGM_DEFAULT:
               *ret_rect = DFB_RECTANGLE_INIT_FROM_REGION( clip );
               return;

          case DWGM_FOLLOW:
               D_ASSERT( parent != NULL );
               *ret_rect = *parent;
               break;

          case DWGM_RECTANGLE:
               *ret_rect = geometry->rectangle;
               ret_rect->x += clip->x1;
               ret_rect->y += clip->y1;
               break;

          case DWGM_LOCATION:
               ret_rect->x = (int)(geometry->location.x * width  + 0.5f) + clip->x1;
               ret_rect->y = (int)(geometry->location.y * height + 0.5f) + clip->y1;
               ret_rect->w = (int)(geometry->location.w * width  + 0.5f);
               ret_rect->h = (int)(geometry->location.h * height + 0.5f);
               break;

          default:
               D_BUG( "invalid geometry mode %d", geometry->mode );
               return;
     }

     if (!dfb_rectangle_intersect_by_region( ret_rect, clip )) {
          D_WARN( "invalid geometry" );
          ret_rect->w = 1;
          ret_rect->h = 1;
     }
}
                
DirectResult
sawman_update_geometry( SaWManWindow *sawwin )
{
     int           i;
     CoreWindow   *window;
     SaWMan       *sawman;
     SaWManWindow *parent;
     SaWManWindow *child;
     DFBRegion     clip;
     DFBRectangle  src;
     DFBRectangle  dst;
     bool          src_updated = false;
     bool          dst_updated = false;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_DEBUG_AT( SaWMan_Update, "%s( %p )\n", __FUNCTION__, sawwin );

     sawman = sawwin->sawman;
     D_MAGIC_ASSERT_IF( sawman, SaWMan );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     parent = sawwin->parent;
     D_MAGIC_ASSERT_IF( parent, SaWManWindow );

     /* Update source geometry. */
     clip.x1 = 0;
     clip.y1 = 0;

     if (window->caps & DWCAPS_INPUTONLY) {
          clip.x2 = window->config.bounds.w - 1;
          clip.y2 = window->config.bounds.h - 1;
     }
     else {
          CoreSurface *surface = window->surface;
          D_ASSERT( surface != NULL );

          clip.x2 = surface->width - 1;
          clip.y2 = surface->height - 1;
     }

     apply_geometry( &window->config.src_geometry, &clip,
                     parent ? &parent->src : NULL, &src );

     if (!DFB_RECTANGLE_EQUAL( src, sawwin->src )) {
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, false, false );

          sawwin->src = src;
          src_updated = true;
     }

     /* Update destination geometry. */
     clip = DFB_REGION_INIT_FROM_RECTANGLE( &window->config.bounds );

     apply_geometry( &window->config.dst_geometry, &clip,
                     parent ? &parent->dst : NULL, &dst );

     if (!DFB_RECTANGLE_EQUAL( dst, sawwin->dst )) {
          if (!src_updated)
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, false, false );

          sawwin->dst = dst;
          dst_updated = true;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, false, false );
     }


     fusion_vector_foreach (child, i, sawwin->children) {
          window = child->window;
          D_ASSERT( window != NULL );

          if ((window->config.src_geometry.mode == DWGM_FOLLOW && src_updated) ||
              (window->config.dst_geometry.mode == DWGM_FOLLOW && dst_updated))
               sawman_update_geometry( child );
     }

     return DFB_OK;
}

int
sawman_window_border( const SaWManWindow *sawwin )
{
     const CoreWindow       *window;
     const SaWManTier       *tier;
     const SaWManBorderInit *border;
     int                     thickness = 0;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (sawwin->caps & DWCAPS_NODECORATION)
          return 0;

     window = sawwin->window;
     D_ASSERT( window != NULL );

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
     recursive procedure to call repaint
     skipping opaque windows that are above the window
     that changed
*/
static void
wind_of_change( SaWMan              *sawman,
                SaWManTier          *tier,
                DFBRegion           *update,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed )
{
     CoreWindow   *window;
     SaWManWindow *sawwin;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( update != NULL );

     D_ASSERT( changed >= 0 );
     D_ASSERT( current >= changed );
     D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

     sawwin = fusion_vector_at( &sawman->layout, current );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %d,%d-%dx%d, %d, %d )\n", __FUNCTION__,
                 tier, DFB_RECTANGLE_VALS_FROM_REGION( update ), current, changed );

     /*
          got to the window that changed, redraw.
     */
     if (current == changed) {
          D_DEBUG_AT( SaWMan_Update, "  -> adding update %d,%d-%dx%d\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( update ) );

          dfb_updates_add( &tier->updates, update );
     }
     else {
          DFBRegion         opaque;
          DFBWindowOptions  options = window->config.options;

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    window->config.bounds.x, window->config.bounds.y,
                                                    window->config.bounds.x + window->config.bounds.w - 1,
                                                    window->config.bounds.y + window->config.bounds.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       window->config.bounds.x + window->config.opaque.x1,
                                                       window->config.bounds.y + window->config.opaque.y1,
                                                       window->config.bounds.x + window->config.opaque.x2,
                                                       window->config.bounds.y + window->config.opaque.y2 ))
                 )  )) {
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
          }
          /*
               pass through
          */
          else
               wind_of_change( sawman, tier, update, flags, current-1, changed );
     }
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

     D_ASSERT( changed >= 0 );
     D_ASSERT( current >= changed );
     D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

     if (*ret_showing)
          return;

     /*
          got to the window that changed, redraw.
     */
     if (current == changed)
          *ret_showing = true;
     else {
          DFBRegion         opaque;
          DFBWindowOptions  options;
          CoreWindow       *window;
          SaWManWindow     *sawwin = fusion_vector_at( &sawman->layout, current );

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_ASSERT( window != NULL );
          options = window->config.options;

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    window->config.bounds.x, window->config.bounds.y,
                                                    window->config.bounds.x + window->config.bounds.w - 1,
                                                    window->config.bounds.y + window->config.bounds.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       window->config.bounds.x + window->config.opaque.x1,
                                                       window->config.bounds.y + window->config.opaque.y1,
                                                       window->config.bounds.x + window->config.opaque.x2,
                                                       window->config.bounds.y + window->config.opaque.y2 ))
                 )  )) {
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
          }
          /*
               pass through
          */
          else
               wind_of_showing( sawman, tier, update, current-1, changed, ret_showing );
     }
}

