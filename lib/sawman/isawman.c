/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <direct/interface.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <SaWMan.h>
#include <SaWManManager.h>

#include <sawman.h>

#include "sawman_window.h"

#include "isawman.h"
#include "isawmanmanager.h"


static ReactionResult ISaWMan_Tier_Update( const void *msg_data,
                                           void       *ctx );

/**********************************************************************************************************************/

static void
ISaWMan_Destruct( ISaWMan *thiz )
{
     unsigned int  i;
     ISaWMan_data *data = thiz->priv;

     for (i=0; i<data->num_tiers; i++) {
          dfb_updates_deinit( &data->tiers[i].updates );

          if (data->tiers[i].attached)
               fusion_reactor_detach( data->tiers[i].tier->reactor, &data->tiers[i].reaction );
     }

     dfb_core_destroy( data->core, false );

     pthread_mutex_destroy( &data->lock );
     pthread_cond_destroy( &data->cond );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**********************************************************************************************************************/

static DirectResult
ISaWMan_AddRef( ISaWMan *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     data->ref++;

     return DFB_OK;
}

static DirectResult
ISaWMan_Release( ISaWMan *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     if (--data->ref == 0)
          ISaWMan_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DirectResult
ISaWMan_Start( ISaWMan    *thiz,
               const char *name,
               pid_t      *ret_pid )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     if (!name)
          return DFB_INVARG;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     return SaWMan_Start( sawman, (const u8*) name, strlen(name) + 1, ret_pid );
}

static DirectResult
ISaWMan_Stop( ISaWMan *thiz,
              pid_t    pid )
{
     SaWMan *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     return SaWMan_Stop( sawman, pid );
}

static DirectResult
ISaWMan_ReturnKeyEvent( ISaWMan        *thiz,
                        DFBWindowEvent *event )
{
     DirectResult  ret;
     SaWMan       *sawman;

     DIRECT_INTERFACE_GET_DATA( ISaWManManager )

     if (!event)
          return DFB_INVARG;

     /* Only key events! */
     if (event->type != DWET_KEYDOWN && event->type != DWET_KEYUP)
          return DFB_UNSUPPORTED;

     /* Don't return same event twice! 'flags' is only valid for KEY events. */
     if (event->flags & DWEF_RETURNED)
          return DFB_LIMITEXCEEDED;

     event->flags |= DWEF_RETURNED;

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     if (sawman->unselkeys_window)
          ret = sawman_post_event( sawman, sawman->unselkeys_window, event );

     sawman_unlock( sawman );

     return ret;
}

static DirectResult
ISaWMan_CreateManager( ISaWMan                  *thiz,
                       const SaWManCallbacks    *callbacks,
                       void                     *context,
                       ISaWManManager          **ret_manager )
{
     DirectResult    ret;
     ISaWManManager *manager;
     SaWMan         *sawman;
     SaWManManager  *manager_object;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     if (!callbacks || !ret_manager)
          return DFB_INVARG;

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     ret = sawman_register( sawman, callbacks, context, &manager_object );
     if (ret)
          goto out;

     DIRECT_ALLOCATE_INTERFACE( manager, ISaWManManager );

     ret = ISaWManManager_Construct( manager, data->sawman, data->process, manager_object );
     if (ret) {
          sawman_unregister( sawman );
          goto out;
     }

     *ret_manager = manager;

     SaWManManager_Activate( manager_object );

out:
     return ret;
}

static DirectResult
ISaWMan_GetUpdates( ISaWMan                *thiz,
                    DFBWindowStackingClass  stacking_class,
                    DFBRegion              *ret_updates,
                    unsigned int           *ret_num )
{
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     if (!ret_updates || !ret_num)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     for (i=0; i<data->num_tiers; i++) {
          if (data->tiers[i].tier->classes & (1 << stacking_class)) {
               if (!data->tiers[i].attached) {
                    fusion_reactor_attach_channel( data->tiers[i].tier->reactor,
                                                   SAWMAN_TIER_UPDATE, ISaWMan_Tier_Update, data, &data->tiers[i].reaction );

                    data->tiers[i].attached = true;
               }

               while (!data->tiers[i].updates.num_regions) {
                    pthread_cond_wait( &data->cond, &data->lock );
               }

               direct_memcpy( ret_updates, data->tiers[i].updates.regions, sizeof(DFBRegion) * data->tiers[i].updates.num_regions );

               *ret_num = data->tiers[i].updates.num_regions;

               dfb_updates_reset( &data->tiers[i].updates );

               break;
          }
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DirectResult
ISaWMan_GetPerformance( ISaWMan                *thiz,
                        DFBWindowStackingClass  stacking,
                        DFBBoolean              reset,
                        unsigned int           *ret_updates,
                        unsigned long long     *ret_pixels,
                        long long              *ret_duration )
{
     DFBResult ret;
     u64       pixels;
     s64       duration;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     ret = SaWMan_GetPerformance( data->sawman, stacking, reset, ret_updates, &pixels, &duration );
     if (ret)
          return ret;

     if (ret_pixels)
          *ret_pixels = pixels;

     if (ret_duration)
          *ret_duration = duration;

     return DR_OK;
}

typedef struct {
     DirectLink          link;

     SaWManListeners     listeners;
     void               *context;

     Reaction            reaction;
} Listener;

static ReactionResult
ISaWMan_Listener( const void *msg_data,
                  void       *ctx )
{
     Listener                     *listener = ctx;
     const SaWManListenerCallData *data     = msg_data;

     switch (data->call) {
          case SWMLC_TIER_UPDATE:
               if (listener->listeners.TierUpdate)
                    listener->listeners.TierUpdate( listener->context, data->stereo_eye, data->layer_id, data->updates, data->num_updates );
               break;

          case SWMLC_WINDOW_BLIT:
               if (listener->listeners.WindowBlit)
                    listener->listeners.WindowBlit( listener->context, data->stereo_eye, data->window_id, data->resource_id, &data->src, &data->dst );
               break;

          default:
               D_BUG( "unknown listener call %d", data->call );
               break;
     }

     return RS_OK;
}

static DirectResult
ISaWMan_RegisterListeners( ISaWMan               *thiz,
                           const SaWManListeners *listeners,
                           void                  *context )
{
     Listener *listener;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     if (!listeners)
          return DFB_INVARG;

     listener = D_CALLOC( 1, sizeof(Listener) );
     if (!listener)
          return D_OOM();

     listener->listeners = *listeners;
     listener->context   = context;

     pthread_mutex_lock( &data->lock );

     direct_list_append( &data->listeners, &listener->link );

     pthread_mutex_unlock( &data->lock );

     fusion_reactor_attach( m_sawman->reactor, ISaWMan_Listener, listener, &listener->reaction );

     return DFB_OK;
}

static DirectResult
ISaWMan_UnregisterListeners( ISaWMan *thiz,
                             void    *context )
{
     Listener *listener;

     DIRECT_INTERFACE_GET_DATA( ISaWMan )

     pthread_mutex_lock( &data->lock );

     direct_list_foreach (listener, data->listeners) {
          if (listener->context == context) {
               fusion_reactor_detach( m_sawman->reactor, &listener->reaction );

               direct_list_remove( &data->listeners, &listener->link );
               
               D_FREE( listener );

               pthread_mutex_unlock( &data->lock );

               return DFB_OK;
          }
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
ISaWMan_Construct( ISaWMan       *thiz,
                   SaWMan        *sawman,
                   SaWManProcess *process )
{
     SaWManTier *tier;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, ISaWMan )

     data->ref     = 1;
     data->sawman  = sawman;
     data->process = process;

     dfb_core_create( &data->core );

     pthread_mutex_init( &data->lock, NULL );
     pthread_cond_init( &data->cond, NULL );

     direct_list_foreach (tier, sawman->tiers) {
          data->tiers[data->num_tiers].tier = tier;

          dfb_updates_init( &data->tiers[data->num_tiers].updates, data->tiers[data->num_tiers].updates_regions,
                            D_ARRAY_SIZE(data->tiers[data->num_tiers].updates_regions) );

          data->num_tiers++;
     }

     thiz->AddRef              = ISaWMan_AddRef;
     thiz->Release             = ISaWMan_Release;
     thiz->Start               = ISaWMan_Start;
     thiz->Stop                = ISaWMan_Stop;
     thiz->ReturnKeyEvent      = ISaWMan_ReturnKeyEvent;
     thiz->CreateManager       = ISaWMan_CreateManager;
     thiz->GetUpdates          = ISaWMan_GetUpdates;
     thiz->RegisterListeners   = ISaWMan_RegisterListeners;
     thiz->UnregisterListeners = ISaWMan_UnregisterListeners;
     thiz->GetPerformance      = ISaWMan_GetPerformance;

     return DFB_OK;
}

/**********************************************************************************************************************/

static ReactionResult
ISaWMan_Tier_Update( const void *msg_data,
                     void       *ctx )
{
     int                     i, n;
     const SaWManTierUpdate *update = msg_data;
     ISaWMan_data           *data   = ctx;

     pthread_mutex_lock( &data->lock );

     for (i=0; i<data->num_tiers; i++) {
          if (data->tiers[i].tier->classes == update->classes) {
               for (n=0; n<update->num_regions; n++)
                    dfb_updates_add( &data->tiers[i].updates, &update->regions[n] );

               break;
          }
     }

     pthread_mutex_unlock( &data->lock );

     pthread_cond_broadcast( &data->cond );

     return RS_OK;
}
