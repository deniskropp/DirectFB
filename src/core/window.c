/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/window.h>
#include <core/window_internal.h>

#include <core/windows_internal.h>


D_DEBUG_DOMAIN( Core_Window, "Core/Window", "DirectFB Core Window" );

/*********************************************************************************************************************/

DFBResult
CoreWindow_SetConfig( CoreWindow             *window,
                      const CoreWindowConfig *config,
                      CoreWindowConfigFlags   flags )
{
     DFBResult           ret;
     int                 val;
     CoreWindowSetConfig set_config;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, window );

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( config != NULL );

     set_config.config = *config;
     set_config.flags  = flags;

     ret = dfb_window_call( window, CORE_WINDOW_SET_CONFIG, &set_config, sizeof(set_config), FCEF_NONE, &val );
     if (ret) {
          D_DERROR( ret, "%s: dfb_window_call( CORE_WINDOW_SET_CONFIG ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return val;
}

DFBResult
CoreWindow_Repaint( CoreWindow          *window,
                    const DFBRegion     *left,
                    const DFBRegion     *right,
                    DFBSurfaceFlipFlags  flags )
{
     DFBResult         ret;
     CoreWindowRepaint repaint;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, window );

     D_MAGIC_ASSERT( window, CoreWindow );
     DFB_REGION_ASSERT( left );
     DFB_REGION_ASSERT( right );

     repaint.left  = *left;
     repaint.right = *right;
     repaint.flags = flags;

     ret = dfb_window_call( window, CORE_WINDOW_REPAINT, &repaint, sizeof(repaint), FCEF_NONE, NULL );
     if (ret) {
          D_DERROR( ret, "%s: dfb_window_call( CORE_WINDOW_SET_CONFIG ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return DFB_OK;
}

/*********************************************************************************************************************/

DirectResult
dfb_window_call( CoreWindow          *window,
                 CoreWindowCall       call,
                 void                *arg,
                 size_t               len,
                 FusionCallExecFlags  flags,
                 int                 *ret_val )
{
     return fusion_call_execute2( &window->call, flags, call, arg, len, ret_val );
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/

static DFBResult
CoreWindow_Dispatch_SetConfig( CoreWindow          *window,
                               CoreWindowSetConfig *set_config )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, window );

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( set_config != NULL );

     return dfb_window_set_config( window, &set_config->config, set_config->flags );
}

static DFBResult
CoreWindow_Dispatch_Repaint( CoreWindow        *window,
                             CoreWindowRepaint *repaint )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, window );

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( repaint != NULL );

     return dfb_window_repaint( window, &repaint->left, &repaint->right, repaint->flags );
}

FusionCallHandlerResult
CoreWindow_Dispatch( int           caller,   /* fusion id of the caller */
                     int           call_arg, /* optional call parameter */
                     void         *call_ptr, /* optional call parameter */
                     void         *ctx,      /* optional handler context */
                     unsigned int  serial,
                     int          *ret_val )
{
     switch (call_arg) {
          case CORE_WINDOW_SET_CONFIG:
               D_DEBUG_AT( Core_Window, "=-> CORE_WINDOW_SET_CONFIG\n" );

               *ret_val = CoreWindow_Dispatch_SetConfig( ctx, call_ptr );
               break;

          case CORE_WINDOW_REPAINT:
               D_DEBUG_AT( Core_Window, "=-> CORE_WINDOW_REPAINT\n" );

               *ret_val = CoreWindow_Dispatch_Repaint( ctx, call_ptr );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

