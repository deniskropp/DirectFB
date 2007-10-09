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
#include <sawman_manager.h>

#include "isawmanmanager.h"



static void
ISaWManManager_Destruct( ISaWManManager *thiz )
{
     ISaWManManager_data *data = thiz->priv;

     (void) data;

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
     SaWMan     *sawman;
     SaWManTier *tier;
     DFBRegion   update;

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

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     tier = sawman_tier_by_class( sawman, stacking );

     update = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

     if (region && !dfb_region_region_intersect( &update, region ))
          return DFB_OK;

     dfb_updates_add( &tier->updates, &update );

     return DFB_OK;
}

static DirectResult
ISaWManManager_ProcessUpdates( ISaWManManager      *thiz,
                               DFBSurfaceFlipFlags  flags )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     return sawman_process_updates( sawman, flags );
}

static DirectResult
ISaWManManager_CloseWindow( ISaWManManager *thiz,
                            SaWManWindow   *window )
{
     SaWMan         *sawman;
     DFBWindowEvent  event;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     event.type = DWET_CLOSE;

     dfb_window_post_event( window->window, &event );

     return DFB_OK;
}

static DirectResult
ISaWManManager_SetVisible( ISaWManManager *thiz,
                           SaWManWindow   *window,
                           DFBBoolean      visible )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;// sawman_set_visible( sawman, window );
}

static DirectResult
ISaWManManager_SwitchFocus( ISaWManManager *thiz,
                            SaWManWindow   *window )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     return sawman_switch_focus( sawman, window );
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

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     tier = sawman_tier_by_class( sawman, stacking );

     *ret_size = tier->size;

     return DFB_OK;
}

static DirectResult
ISaWManManager_InsertWindow( ISaWManManager *thiz,
                             SaWManWindow   *window,
                             SaWManWindow   *relative,
                             DFBBoolean      top )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     return sawman_insert_window( sawman, window, relative, top );
}

static DirectResult
ISaWManManager_RemoveWindow( ISaWManManager *thiz,
                             SaWManWindow   *window )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!window)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( window, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     return sawman_remove_window( sawman, window );
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

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     if (sawman->scaling_mode != mode) {
          SaWManTier *tier;

          sawman->scaling_mode = mode;

          direct_list_foreach (tier, sawman->tiers) {
               DFBRegion update = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

               dfb_updates_add( &tier->updates, &update );
          }
     }

     return DFB_OK;
}

static DirectResult
ISaWManManager_SendWindowEvent( ISaWManManager       *thiz,
                                const DFBWindowEvent *event,
                                DFBWindowID           window_id )
{
     SaWMan         *sawman;
     SaWManWindow   *window;
     DFBWindowEvent  evt;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!event || !window_id)
          return DFB_INVARG;

     /* Only key events! */
     if (event->type != DWET_KEYDOWN && event->type != DWET_KEYUP)
          return DFB_UNSUPPORTED;

     /* Don't return same event twice! */
     if (event->flags & DWEF_RETURNED)
          return DFB_LIMITEXCEEDED;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     direct_list_foreach (window, sawman->windows) {
          D_MAGIC_ASSERT( window, SaWManWindow );

          if (window->id == window_id)
               break;
     }

     if (!window)
          return DFB_IDNOTFOUND;

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

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     return sawman_unlock( sawman );
}

DirectResult
ISaWManManager_Construct( ISaWManManager *thiz,
                          SaWMan         *sawman,
                          SaWManProcess  *process )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, ISaWManManager )

     data->ref     = 1;
     data->sawman  = sawman;
     data->process = process;

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
     thiz->SendWindowEvent = ISaWManManager_SendWindowEvent;
     thiz->Lock            = ISaWManManager_Lock;
     thiz->Unlock          = ISaWManManager_Unlock;

     return DFB_OK;
}

