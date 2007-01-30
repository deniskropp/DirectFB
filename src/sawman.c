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

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/layer_region.h>
#include <core/windows_internal.h>

#include <sawman.h>
#include <sawman_manager.h>

#include "isawman.h"

/* FIXME: avoid globals */
static SaWMan        *m_sawman;
static SaWManProcess *m_process;
static FusionWorld   *m_world;

/**********************************************************************************************************************/

static void wind_of_change( SaWMan              *sawman,
                            CoreWindowStack     *stack,
                            CoreLayerRegion     *region,
                            DFBRegion           *update,
                            DFBSurfaceFlipFlags  flags,
                            int                  current,
                            int                  changed );

static void repaint_stack_for_window( SaWMan              *sawman,
                                      CoreWindowStack     *stack,
                                      CoreLayerRegion     *region,
                                      DFBRegion           *update,
                                      DFBSurfaceFlipFlags  flags,
                                      int                  window );

/**********************************************************************************************************************/

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

static int
process_watcher( int   caller,
                 int   call_arg,
                 void *call_ptr,
                 void *ctx )
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
          return DFB_BUG;
     }

     D_INFO( "SaWMan/Watcher: Process %d [%lu] has exited%s\n", process->pid,
             process->fusion_id, (process->flags & SWMPF_EXITING) ? "." : " ABNORMALLY!" );

     unregister_process( sawman, process );

     return DFB_OK;
}

static int
manager_call_handler( int   caller,
                      int   call_arg,
                      void *call_ptr,
                      void *ctx )
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
                         return ret;

                    return -pid;
               }
               break;

          case SWMCID_STOP:
               if (sawman->manager.callbacks.Stop)
                    return sawman->manager.callbacks.Stop( sawman->manager.context, (pid_t) call_ptr, caller );
               break;

          case SWMCID_PROCESS_ADDED:
               if (sawman->manager.callbacks.ProcessAdded)
                    return sawman->manager.callbacks.ProcessAdded( sawman->manager.context, call_ptr );
               break;

          case SWMCID_PROCESS_REMOVED:
               if (sawman->manager.callbacks.ProcessRemoved)
                    return sawman->manager.callbacks.ProcessRemoved( sawman->manager.context, call_ptr );
               break;

          case SWMCID_INPUT_FILTER:
               if (sawman->manager.callbacks.InputFilter)
                    return sawman->manager.callbacks.InputFilter( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_PRECONFIG:
               if (sawman->manager.callbacks.WindowPreConfig)
                    return sawman->manager.callbacks.WindowPreConfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_ADDED:
               if (sawman->manager.callbacks.WindowAdded)
                    return sawman->manager.callbacks.WindowAdded( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_REMOVED:
               if (sawman->manager.callbacks.WindowRemoved)
                    return sawman->manager.callbacks.WindowRemoved( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_CONFIG:
               if (sawman->manager.callbacks.WindowConfig)
                    return sawman->manager.callbacks.WindowConfig( sawman->manager.context, call_ptr );
               break;

          case SWMCID_WINDOW_RESTACK:
               if (sawman->manager.callbacks.WindowRestack)
                    return sawman->manager.callbacks.WindowRestack( sawman->manager.context, call_ptr );
               break;

          case SWMCID_STACK_RESIZED:
               if (sawman->manager.callbacks.StackResized)
                    return sawman->manager.callbacks.StackResized( sawman->manager.context, call_ptr );
               break;
     }

     return DFB_NOIMPL;
}

/**********************************************************************************************************************/

DirectResult
sawman_initialize( SaWMan         *sawman,
                   FusionWorld    *world,
                   SaWManProcess **ret_process )
{
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

     /* Initialize update manager. */
     dfb_updates_init( &sawman->updates, sawman->update_regions, SAWMAN_MAX_UPDATE_REGIONS );

     /* Default to HW Scaling if supported. */
     if (dfb_gfxcard_get_device_info( &info ), info.caps.accel & DFXL_STRETCHBLIT)
          sawman->scaling_mode = SWMSM_STANDARD;

     D_MAGIC_SET( sawman, SaWMan );

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
     SaWManProcess *process;

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


     /* FIXME */
     D_ASSUME( !sawman->windows );
     D_ASSUME( !sawman->layout.count );

     /* Destroy window layout vector. */
     fusion_vector_destroy( &sawman->layout );

     /* Destroy lock. */
     fusion_skirmish_destroy( &sawman->lock );


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
     DFBWindowEvent  evt;
     SaWManWindow   *from;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT_IF( to, SaWManWindow );

     from = sawman->focused_window;
     D_MAGIC_ASSERT_IF( from, SaWManWindow );

     if (from == to)
          return DFB_OK;

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

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     DFB_REGION_ASSERT_IF( region );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_ASSERT( window != NULL );

     if (!VISIBLE_WINDOW(window) && !force_invisible)
          return DFB_OK;

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED))
          return DFB_OK;

     if (region) {
          if (scale_region && (window->config.options & DWOP_SCALE)) {
               int sw = window->surface->width;
               int sh = window->surface->height;

               /* horizontal */
               if (window->config.bounds.w > sw) {
                    /* upscaling */
                    area.x1 = (region->x1 - 1) * window->config.bounds.w / sw;
                    area.x2 = (region->x2 + 1) * window->config.bounds.w / sw;
               }
               else {
                    /* downscaling */
                    area.x1 = region->x1 * window->config.bounds.w / sw - 1;
                    area.x2 = region->x2 * window->config.bounds.w / sw + 1;
               }

               /* vertical */
               if (window->config.bounds.h > sh) {
                    /* upscaling */
                    area.y1 = (region->y1 - 1) * window->config.bounds.h / sh;
                    area.y2 = (region->y2 + 1) * window->config.bounds.h / sh;
               }
               else {
                    /* downscaling */
                    area.y1 = region->y1 * window->config.bounds.h / sh - 1;
                    area.y2 = region->y2 * window->config.bounds.h / sh + 1;
               }

               /* limit to window area */
               dfb_region_clip( &area, 0, 0, window->config.bounds.w - 1, window->config.bounds.h - 1 );

               /* screen offset */
               dfb_region_translate( &area, window->config.bounds.x, window->config.bounds.y );
          }
          else
               area = DFB_REGION_INIT_TRANSLATED( region, window->config.bounds.x, window->config.bounds.y );
     }
     else
          area = DFB_REGION_INIT_FROM_RECTANGLE( &window->config.bounds );

     if (!dfb_unsafe_region_intersect( &area, 0, 0, stack->width - 1, stack->height - 1 ))
          return DFB_OK;

     if (force_complete)
          dfb_updates_add( &sawman->updates, &area );
     else
          repaint_stack_for_window( sawman, stack, window->primary_region,
                                    &area, flags, sawman_window_index( sawman, sawwin ) );

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

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );

     if (relative) {
          D_ASSUME( relative->priority == sawwin->priority );

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
     int         index;
     CoreWindow *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_BUG( "window not inserted" );
          return DFB_BUG;
     }

     window = sawwin->window;
     D_ASSERT( window != NULL );

     sawman_withdraw_window( sawman, sawwin );

     /* Hide window. */
     if (VISIBLE_WINDOW(window) && window->config.opacity > 0) {
          window->config.opacity = 0;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, true, false );
     }

     index = sawman_window_index( sawman, sawwin );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < sawman->layout.count );

     fusion_vector_remove( &sawman->layout, index );

     sawwin->flags &= ~SWMWF_INSERTED;

     return DFB_OK;
}

DirectResult
sawman_withdraw_window( SaWMan       *sawman,
                        SaWManWindow *sawwin )
{
     int               i, index;
     DirectLink       *next;
     CoreWindow       *window;
     SaWManGrabbedKey *key;

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

     /* Release all explicit key grabs. */
     direct_list_foreach_safe (key, next, sawman->grabbed_keys) {
          if (key->owner == sawwin) {
               direct_list_remove( &sawman->grabbed_keys, &key->link );
               SHFREE( sawwin->shmpool, key );
          }
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

/*
     recursive procedure to call repaint
     skipping opaque windows that are above the window
     that changed
*/
static void
wind_of_change( SaWMan              *sawman,
                CoreWindowStack     *stack,
                CoreLayerRegion     *region,
                DFBRegion           *update,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( update != NULL );

     D_ASSERT( changed >= 0 );
     D_ASSERT( current >= changed );
     D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

     /*
          got to the window that changed, redraw.
     */
     if (current == changed)
          dfb_updates_add( &sawman->updates, update );
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
          if ((
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
                 )) {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_change( sawman, stack, region, &left, flags, current-1, changed );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_change( sawman, stack, region, &upper, flags, current-1, changed );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_change( sawman, stack, region, &right, flags, current-1, changed );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_change( sawman, stack, region, &lower, flags, current-1, changed );
               }
          }
          /*
               pass through
          */
          else
               wind_of_change( sawman, stack, region, update, flags, current-1, changed );
     }
}

static void
repaint_stack_for_window( SaWMan              *sawman,
                          CoreWindowStack     *stack,
                          CoreLayerRegion     *region,
                          DFBRegion           *update,
                          DFBSurfaceFlipFlags  flags,
                          int                  window )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( update != NULL );
     D_ASSERT( window >= 0 );
     D_ASSERT( window < fusion_vector_size( &sawman->layout ) );

     if (fusion_vector_has_elements( &sawman->layout ) && window >= 0) {
          int num = fusion_vector_size( &sawman->layout );

          D_ASSERT( window < num );

          wind_of_change( sawman, stack, region, update, flags, num - 1, window );
     }
     else
          dfb_updates_add( &sawman->updates, update );
}

