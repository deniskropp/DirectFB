/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include "SaWManManager.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>

#include <sawman_updates.h>
#include <sawman_window.h>
}

D_DEBUG_DOMAIN( DirectFB_SaWManManager, "DirectFB/SaWManManager", "DirectFB SaWManManager" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ISaWManManager_Real::Activate(
)
{
     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     if (m_sawman->manager.active) {
          sawman_unlock( m_sawman );
          return DFB_BUSY;
     }

     m_sawman->manager.active = true;

     /*
      * Attach to existing entities.
      */
     if (m_sawman->manager.callbacks.ProcessAdded) {
          SaWManProcess *process;

          direct_list_foreach (process, m_sawman->processes) {
               D_MAGIC_ASSERT( process, SaWManProcess );

               sawman_call( m_sawman, SWMCID_PROCESS_ADDED, process, sizeof(*process), false );
          }
     }

     if (m_sawman->manager.callbacks.WindowAdded) {
          SaWManWindow     *window;
          SaWManWindowInfo  info;

          direct_list_foreach (window, m_sawman->windows) {
               D_MAGIC_ASSERT( window, SaWManWindow );
               info.handle = (SaWManWindowHandle)window;
               info.caps   = window->caps;
               SAWMANWINDOWCONFIG_COPY( &info.config, &window->window->config );
               info.config.key_selection = window->window->config.key_selection;
               info.config.keys          = window->window->config.keys;
               info.config.num_keys      = window->window->config.num_keys;
               info.resource_id          = window->window->resource_id;
               info.application_id       = window->window->config.application_id;
               info.win_id               = window->window->id;
               info.flags = (SaWManWindowFlags)(
                             window->flags
                             | (window->window->flags & CWF_FOCUSED ? SWMWF_FOCUSED : 0)
                             | (window->window->flags & CWF_ENTERED ? SWMWF_ENTERED : 0) );

               sawman_call( m_sawman, SWMCID_WINDOW_ADDED, &info, sizeof(info), false );
          }
     }

     sawman_unlock( m_sawman );

     return DFB_OK;
}


DFBResult
ISaWManManager_Real::QueueUpdate(
                    DFBWindowStackingClass  stacking,
                    const DFBRegion        *update
)
{
     SaWManTier *tier;
     DFBRegion   region;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     tier = sawman_tier_by_class( m_sawman, stacking );

     region = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

     if (!update || dfb_region_region_intersect( &region, update )) {
          dfb_updates_add( &tier->left.updates, &region );
          dfb_updates_add( &tier->right.updates, &region );
     }

     sawman_unlock( m_sawman );

     return DFB_OK;
}


DFBResult
ISaWManManager_Real::ProcessUpdates(
                    DFBSurfaceFlipFlags flags
)
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     ret = (DFBResult) sawman_process_updates( m_sawman, flags );

     sawman_unlock( m_sawman );

     return ret;
}


DFBResult
ISaWManManager_Real::CloseWindow(
                    SaWManWindow *window
)
{
     DFBWindowEvent event;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     event.type = DWET_CLOSE;

     dfb_window_post_event( window->window, &event );

     return DFB_OK;
}


DFBResult
ISaWManManager_Real::InsertWindow(
                    SaWManWindow         *window,
                    SaWManWindow         *relative,
                    SaWManWindowRelation  relation
)
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     ret = (DFBResult) sawman_insert_window( m_sawman, window, relative,
                                             (relation == SWMWR_TOP) ? DFB_TRUE : DFB_FALSE );

     sawman_unlock( m_sawman );

     return ret;
}


DFBResult
ISaWManManager_Real::RemoveWindow(
                    SaWManWindow         *window
)
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     ret = (DFBResult) sawman_remove_window( m_sawman, window );

     sawman_unlock( m_sawman );

     return ret;
}


DFBResult
ISaWManManager_Real::SwitchFocus(
                    SaWManWindow         *window
)
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     ret = (DFBResult) sawman_switch_focus( m_sawman, window );

     sawman_unlock( m_sawman );

     return ret;
}


DFBResult
ISaWManManager_Real::SetScalingMode(
                    SaWManScalingMode     mode
)
{
     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     if (m_sawman->scaling_mode != mode) {
          SaWManTier *tier;

          m_sawman->scaling_mode = mode;

          direct_list_foreach (tier, m_sawman->tiers) {
               DFBRegion update = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

               dfb_updates_add( &tier->left.updates, &update );
               dfb_updates_add( &tier->right.updates, &update );
          }
     }

     sawman_unlock( m_sawman );

     return DFB_OK;
}


static void
reset_geometry_to_nonfollow( SaWManWindow *window )
{
     if (window->window->config.dst_geometry.mode == DWGM_FOLLOW)
          window->window->config.dst_geometry.mode = DWGM_DEFAULT;

     if (window->window->config.src_geometry.mode == DWGM_FOLLOW)
          window->window->config.src_geometry.mode = DWGM_DEFAULT;
}

DFBResult
ISaWManManager_Real::SetWindowConfig(
                    SaWManWindow             *sawwin,
                    const SaWManWindowConfig *config,
                    SaWManWindowConfigFlags   flags )
{
     DFBResult   ret;
     SaWMan     *sawman;
     CoreWindow *window;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman = m_sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     sawman_lock( sawman );

     if (flags & SWMCF_OPTIONS) {
          if ((window->config.options & DWOP_SCALE) && !(config->options & DWOP_SCALE) && window->surface) {
               /* scaling turned off - see if we need to reallocate the surface */
               if (window->config.bounds.w != window->surface->config.size.w ||
                   window->config.bounds.h != window->surface->config.size.h)
               {
                    ret = dfb_surface_reformat( window->surface,
                                                window->config.bounds.w,
                                                window->config.bounds.h,
                                                window->surface->config.format );
                    if (ret) {
                         D_DERROR( ret, "WM/SaWMan: Could not resize surface "
                                        "(%dx%d -> %dx%d) to remove DWOP_SCALE!\n",
                                   window->surface->config.size.w,
                                   window->surface->config.size.h,
                                   window->config.bounds.w,
                                   window->config.bounds.h );
                         return ret;
                    }
               }
          }

          if (config->options & (DWOP_KEEP_ABOVE | DWOP_KEEP_UNDER)) {
               D_ASSERT( sawwin->parent );

               if (config->options & DWOP_KEEP_ABOVE) {
                    D_ASSERT( sawman_window_priority(sawwin->parent) <= sawman_window_priority(sawwin) );

                    sawman_insert_window( sawman, sawwin, sawwin->parent, true );
               }
               else {
                    D_ASSERT( sawman_window_priority(sawwin->parent) >= sawman_window_priority(sawwin) );

                    sawman_insert_window( sawman, sawwin, sawwin->parent, false );
               }

               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE,
                                     (SaWManUpdateFlags)( SWMUF_UPDATE_BORDER | SWMUF_FORCE_COMPLETE ) );
          }

          window->config.options = config->options;
     }

     if (flags & SWMCF_EVENTS)
          window->config.events = config->events;

     if (flags & SWMCF_COLOR) {
          window->config.color = config->color;
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
     }

     if (flags & SWMCF_COLOR_KEY)
          window->config.color_key = config->color_key;

     if (flags & SWMCF_OPAQUE)
          window->config.opaque = config->opaque;

     if (flags & CWCF_STACKING)
          sawman_restack_window( sawman, sawwin, sawwin, 0, config->stacking );

     if (flags & SWMCF_OPACITY)
          sawman_set_opacity( sawman, sawwin, config->opacity );

     if (flags & SWMCF_STEREO_DEPTH)
          sawman_set_stereo_depth( sawman, sawwin, config->z );

     if (flags & (SWMCF_POSITION | SWMCF_SIZE)) {
          if( flags == SWMCF_POSITION ) {
               window->config.bounds.x = config->bounds.x;
               window->config.bounds.y = config->bounds.y;
          }
          else if( flags == SWMCF_SIZE ) {
               window->config.bounds.w = config->bounds.w;
               window->config.bounds.h = config->bounds.h;
          }
          else
               window->config.bounds = config->bounds;          
     }

     if (flags & SWMCF_SRC_GEOMETRY)
          window->config.src_geometry = config->src_geometry;

     if (flags & SWMCF_DST_GEOMETRY)
          window->config.dst_geometry = config->dst_geometry;

     if (flags & CWCF_ASSOCIATION && window->config.association != config->association) {
          SaWManWindow *parent = NULL;

          /* Dissociate first */
          if (sawwin->parent_window) {
               int index;

               dfb_window_unlink( &sawwin->parent_window );

               index = fusion_vector_index_of( &parent->children, sawwin );
               D_ASSERT( index >= 0 );
               D_ASSERT( index < parent->children.count );

               fusion_vector_remove( &parent->children, index );

               sawwin->parent = NULL;

               window->config.association = 0;
          }



          /* Lookup new parent window. */
          if (config->association) {
               D_DEBUG_AT( DirectFB_SaWManManager, "  -> new parent win id %u\n", config->association );

               direct_list_foreach (parent, sawman->windows) {
                    D_MAGIC_ASSERT( parent, SaWManWindow );
                    D_ASSERT( parent->window != NULL );

                    if (parent->id == config->association)
                         break;
               }

               if (!parent) {
                    D_ERROR( "SaWMan/WM: Can't find parent window with ID %d!\n", config->association );
                    reset_geometry_to_nonfollow( sawwin );
                    return DFB_IDNOTFOUND;
               }

               D_MAGIC_ASSERT( parent, SaWManWindow );
               D_ASSERT( parent->window != NULL );

#ifndef OLD_COREWINDOWS_STRUCTURE
               if (parent->window->toplevel != window->toplevel) {
                    D_ERROR( "SaWMan/WM: Can't associate windows with different toplevel!\n" );
                    reset_geometry_to_nonfollow( sawwin );
                    return DFB_INVARG;
               }
#endif

               D_DEBUG_AT( DirectFB_SaWManManager, "  -> parent window %p\n", parent );


               ret = (DFBResult) dfb_window_link( &sawwin->parent_window, parent->window );
               if (ret) {
                    D_DERROR( ret, "SaWMan/WM: Can't link parent window with ID %d!\n", config->association );
                    reset_geometry_to_nonfollow( sawwin );
                    return ret;
               }

               ret = (DFBResult) fusion_vector_add( &parent->children, sawwin );
               if (ret) {
                    dfb_window_unlink( &sawwin->parent_window );
                    reset_geometry_to_nonfollow( sawwin );
                    return ret;
               }


               sawwin->parent = parent;

               /* Write back new association */
               window->config.association = config->association;
          }
          else
               reset_geometry_to_nonfollow( sawwin );
     }

     if (flags & (SWMCF_POSITION | SWMCF_SIZE | SWMCF_SRC_GEOMETRY | SWMCF_DST_GEOMETRY | SWMCF_ASSOCIATION))
          sawman_update_geometry( sawwin );

     if (flags & (CWCF_POSITION | CWCF_SIZE | CWCF_OPACITY | CWCF_OPTIONS | CWCF_STEREO_DEPTH))
          sawman_update_visible( sawman );

     sawman_unlock( sawman );

     return ret;
}


DFBResult
ISaWManManager_Real::IsShowingWindow(
                    SaWManWindow         *window,
                    DFBBoolean           *ret_showing
)
{
     DFBResult ret;
     bool      showing;

     D_DEBUG_AT( DirectFB_SaWManManager, "%s()\n", __FUNCTION__ );

     sawman_lock( m_sawman );

     ret = (DFBResult) sawman_showing_window( m_sawman, window, &showing );

     sawman_unlock( m_sawman );

     if (ret == DFB_OK)
          *ret_showing = showing ? DFB_TRUE : DFB_FALSE;

     return ret;
}


}

