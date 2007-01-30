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


static DFBResult
ISaWManManager_AddRef( ISaWManManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     data->ref++;

     return DFB_OK;
}

static DFBResult
ISaWManManager_Release( ISaWManManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (--data->ref == 0)
          ISaWManManager_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
ISaWManManager_QueueUpdate( ISaWManManager  *thiz,
                            const DFBRegion *region )
{
     SaWMan    *sawman;
     DFBRegion  update;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!DFB_REGION_CHECK_IF( region ))
          return DFB_INVAREA;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* FIXME: locking? */

     update = DFB_REGION_INIT_FROM_DIMENSION( &sawman->size );

     if (region && !dfb_region_region_intersect( &update, region ))
          return DFB_OK;

     dfb_updates_add( &sawman->updates, &update );

     return DFB_OK;
}

static DFBResult
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

     /* FIXME: locking? */

     event.type = DWET_CLOSE;

     dfb_window_post_event( window->window, &event );

     return DFB_OK;
}

static DFBResult
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

     /* FIXME: locking? */

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;// sawman_set_visible( sawman, window );
}

static DFBResult
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

     /* FIXME: locking? */

     return sawman_switch_focus( sawman, window );
}

static DFBResult
ISaWManManager_GetSize( ISaWManManager *thiz,
                        DFBDimension   *ret_size )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!ret_size)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* FIXME: locking? */

     *ret_size = sawman->size;

     return DFB_OK;
}

static DFBResult
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

     /* FIXME: locking? */

     return sawman_insert_window( sawman, window, relative, top );
}

static DFBResult
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

     /* FIXME: locking? */

     return sawman_remove_window( sawman, window );
}

static DFBResult
ISaWManManager_SetScalingMode( ISaWManManager    *thiz,
                               SaWManScalingMode  mode )
{
     SaWMan    *sawman;
     DFBRegion  update;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (mode != SWMSM_STANDARD && mode != SWMSM_SMOOTH_SW)
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* FIXME: locking? */

     if (sawman->scaling_mode != mode) {
          sawman->scaling_mode = mode;

          update = DFB_REGION_INIT_FROM_DIMENSION( &sawman->size );

          dfb_updates_add( &sawman->updates, &update );
     }

     return DFB_OK;
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

     thiz->AddRef         = ISaWManManager_AddRef;
     thiz->Release        = ISaWManManager_Release;
     thiz->QueueUpdate    = ISaWManManager_QueueUpdate;
     thiz->CloseWindow    = ISaWManManager_CloseWindow;
     thiz->SetVisible     = ISaWManManager_SetVisible;
     thiz->SwitchFocus    = ISaWManManager_SwitchFocus;
     thiz->GetSize        = ISaWManManager_GetSize;
     thiz->InsertWindow   = ISaWManManager_InsertWindow;
     thiz->RemoveWindow   = ISaWManManager_RemoveWindow;
     thiz->SetScalingMode = ISaWManManager_SetScalingMode;

     return DFB_OK;
}

