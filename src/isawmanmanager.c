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

#include <directfb_util.h>

#include <direct/interface.h>
#include <direct/messages.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/wm.h>

#include <sawman.h>

#include "SaWManManager.h"

#include "sawman_updates.h"
#include "sawman_window.h"

#include "isawmanmanager.h"



D_DEBUG_DOMAIN( SaWMan_Manager, "SaWMan/Manager", "SaWMan Manager Interface" );


static void
ISaWManManager_Destruct( ISaWManManager *thiz )
{
     DFBResult   ret;
     SaWMan     *sawman;

     ISaWManManager_data *data = thiz->priv;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     ret = sawman_lock( sawman );
     if (ret)
          D_DERROR( ret, "SaWMan/%s(): sawman_lock() failed!\n", __FUNCTION__ );
     else {
          sawman_unregister( sawman );
          sawman_unlock( sawman );
     }

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}


static DirectResult
ISaWManManager_AddRef( ISaWManManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     data->ref++;

     return DFB_OK;
}

static DirectResult
ISaWManManager_Release( ISaWManManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (--data->ref == 0)
          ISaWManManager_Destruct( thiz );

     return DFB_OK;
}

static DirectResult
ISaWManManager_QueueUpdate( ISaWManManager         *thiz,
                            DFBWindowStackingClass  stacking,
                            const DFBRegion        *region )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!DFB_REGION_CHECK_IF( region ))
          return DFB_INVAREA;

     switch (stacking) {
          case DWSC_LOWER:
          case DWSC_MIDDLE:
          case DWSC_UPPER:
               break;

          default:
               return DFB_INVARG;
     }

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_QueueUpdate( data->manager, stacking, region );
}

static DirectResult
ISaWManManager_ProcessUpdates( ISaWManManager      *thiz,
                               DFBSurfaceFlipFlags  flags )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_ProcessUpdates( data->manager, flags );
}

static DirectResult
ISaWManManager_CloseWindow( ISaWManManager     *thiz,
                            SaWManWindowHandle  handle )
{
     SaWMan         *sawman;
     SaWManWindow   *window = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     return SaWManManager_CloseWindow( data->manager, window );
}

static DirectResult
ISaWManManager_SetVisible( ISaWManManager     *thiz,
                           SaWManWindowHandle  handle,
                           DFBBoolean          visible )
{
     SaWMan       *sawman;
     SaWManWindow *window = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;// sawman_set_visible( sawman, window );
}

static DirectResult
ISaWManManager_SwitchFocus( ISaWManManager     *thiz,
                            SaWManWindowHandle  handle )
{
     SaWMan       *sawman;
     SaWManWindow *window = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_SwitchFocus( data->manager, window );
}

static DirectResult
ISaWManManager_GetSize( ISaWManManager         *thiz,
                        DFBWindowStackingClass  stacking,
                        DFBDimension           *ret_size )
{
     SaWMan     *sawman;
     SaWManTier *tier;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     switch (stacking) {
          case DWSC_LOWER:
          case DWSC_MIDDLE:
          case DWSC_UPPER:
               break;

          default:
               return DFB_INVARG;
     }

     if (!ret_size)
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     tier = sawman_tier_by_class( sawman, stacking );

     *ret_size = tier->size;

     return DFB_OK;
}

static DirectResult
ISaWManManager_InsertWindow( ISaWManManager       *thiz,
                             SaWManWindowHandle    handle,
                             SaWManWindowHandle    relative,
                             SaWManWindowRelation  relation )
{
     SaWMan       *sawman;
     SaWManWindow *window = (SaWManWindow*)handle;
     SaWManWindow *sawrel = (SaWManWindow*)relative;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );
     D_MAGIC_ASSERT_IF( sawrel, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_InsertWindow( data->manager, window, sawrel, relation );
}

static DirectResult
ISaWManManager_RemoveWindow( ISaWManManager     *thiz,
                             SaWManWindowHandle  handle )
{
     SaWMan       *sawman;
     SaWManWindow *window = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_RemoveWindow( data->manager, window );
}

static DirectResult
ISaWManManager_SetScalingMode( ISaWManManager    *thiz,
                               SaWManScalingMode  mode )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (mode != SWMSM_STANDARD && mode != SWMSM_SMOOTH_SW)
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_SetScalingMode( data->manager, mode );
}

static DirectResult
ISaWManManager_SetWindowConfig ( ISaWManManager           *thiz,
                                 SaWManWindowHandle        handle,
                                 SaWManWindowConfigFlags   flags,
                                 SaWManWindowConfig       *config )
{
     SaWMan       *sawman;
     SaWManWindow *sawwin = (SaWManWindow*)handle;

     /*
       not yet implemented:
       SWMCF_KEY_SELECTION, SWMCF_CURSOR_FLAGS, SWMCF_CURSOR_RESOLUTION
     */

     if (flags & ~(SWMCF_ALL - SWMCF_KEY_SELECTION - SWMCF_CURSOR_FLAGS - SWMCF_CURSOR_RESOLUTION))
          return DFB_INVARG;

     if (config == NULL)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return SaWManManager_SetWindowConfig( data->manager, sawwin, config, flags );
}

static DirectResult
ISaWManManager_SendWindowEvent( ISaWManManager       *thiz,
                                SaWManWindowHandle    handle,
                                const DFBWindowEvent *event )
{
     SaWMan         *sawman;
     SaWManWindow   *window  = (SaWManWindow*)handle;
     DFBWindowEvent  evt;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!event || (handle == SAWMAN_WINDOW_NONE))
          return DFB_INVARG;

     /* Only key events! */
     if (event->type != DWET_KEYDOWN && event->type != DWET_KEYUP)
          return DFB_UNSUPPORTED;

     /* Don't return same event twice! */
     if (event->flags & DWEF_RETURNED)
          return DFB_LIMITEXCEEDED;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     evt = *event;
     sawman_post_event( sawman, window, &evt );

     return DFB_OK;
}

static DirectResult
ISaWManManager_Lock( ISaWManManager *thiz )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     return sawman_lock( sawman );
}

static DirectResult
ISaWManManager_Unlock( ISaWManManager *thiz )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return sawman_unlock( sawman );
}

static DirectResult
ISaWManManager_GetWindowInfo( ISaWManManager     *thiz,
                              SaWManWindowHandle  handle,
                              SaWManWindowInfo   *info )
{
     SaWMan          *sawman;
     SaWManWindow    *sawwin = (SaWManWindow*)handle;
     CoreWindow      *window;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!info || (handle == SAWMAN_WINDOW_NONE))
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     info->handle               = handle;
     info->caps                 = sawwin->caps;
     SAWMANWINDOWCONFIG_COPY( &info->config, &window->config )
     info->config.key_selection = window->config.key_selection;
     info->config.keys          = window->config.keys;
     info->config.num_keys      = window->config.num_keys;
     info->resource_id          = window->resource_id;
     info->application_id       = window->config.application_id;
     info->win_id               = window->id;
     info->flags = sawwin->flags 
                   | (window->flags & CWF_FOCUSED ? SWMWF_FOCUSED : 0)
                   | (window->flags & CWF_ENTERED ? SWMWF_ENTERED : 0);

     return DFB_OK;
}

static DirectResult
ISaWManManager_GetProcessInfo( ISaWManManager     *thiz,
                               SaWManWindowHandle  handle,
                               SaWManProcess      *process )
{
     SaWMan         *sawman;
     SaWManWindow   *sawwin = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!process || (handle == SAWMAN_WINDOW_NONE))
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     *process = *sawwin->process;

     return DFB_OK;
}

static DirectResult
ISaWManManager_IsWindowShowing( ISaWManManager     *thiz,
                                SaWManWindowHandle  handle,
                                DFBBoolean         *ret_showing )
{
     DFBResult       ret;
     bool            showing;
     SaWMan         *sawman;
     SaWManWindow   *sawwin = (SaWManWindow*)handle;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!ret_showing)
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     ret = sawman_showing_window( sawman, sawwin, &showing );
     if (ret)
          return ret;

     *ret_showing = showing ? DFB_TRUE : DFB_FALSE;

     return DFB_OK;
}

DirectResult
ISaWManManager_Construct( ISaWManManager *thiz,
                          SaWMan         *sawman,
                          SaWManProcess  *process,
                          SaWManManager  *manager )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, ISaWManManager )

     data->ref     = 1;
     data->sawman  = sawman;
     data->process = process;
     data->manager = manager;

     thiz->AddRef          = ISaWManManager_AddRef;
     thiz->Release         = ISaWManManager_Release;
     thiz->QueueUpdate     = ISaWManManager_QueueUpdate;
     thiz->ProcessUpdates  = ISaWManManager_ProcessUpdates;
     thiz->CloseWindow     = ISaWManManager_CloseWindow;
     thiz->SetVisible      = ISaWManManager_SetVisible;
     thiz->SwitchFocus     = ISaWManManager_SwitchFocus;
     thiz->GetSize         = ISaWManManager_GetSize;
     thiz->InsertWindow    = ISaWManManager_InsertWindow;
     thiz->RemoveWindow    = ISaWManManager_RemoveWindow;
     thiz->SetScalingMode  = ISaWManManager_SetScalingMode;
     thiz->SetWindowConfig = ISaWManManager_SetWindowConfig;
     thiz->SendWindowEvent = ISaWManManager_SendWindowEvent;
     thiz->Lock            = ISaWManManager_Lock;
     thiz->Unlock          = ISaWManManager_Unlock;
     thiz->GetWindowInfo   = ISaWManManager_GetWindowInfo;
     thiz->GetProcessInfo  = ISaWManManager_GetProcessInfo;
     thiz->IsWindowShowing = ISaWManManager_IsWindowShowing;

     return DFB_OK;
}
