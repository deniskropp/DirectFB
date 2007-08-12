/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <fusion/shmalloc.h>
#include <fusion/arena.h>
#include <fusion/property.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>

#include <core/input.h>
#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/screen.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/system.h>
#include <core/windows.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/layers_internal.h>


D_DEBUG_DOMAIN( Core_Layers, "Core/Layers", "DirectFB Display Layer Core" );

/** public **/

DFBResult
dfb_layer_suspend( CoreLayer *layer )
{
     CoreLayerShared   *shared;
     CoreLayerContexts *contexts;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared   = layer->shared;
     contexts = &shared->contexts;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     D_ASSUME( !shared->suspended );

     if (shared->suspended) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_OK;
     }

     /* Deactivate current context. */
     if (contexts->active >= 0) {
          DFBResult         ret;
          CoreLayerContext *current = fusion_vector_at( &contexts->stack,
                                                         contexts->active );

          ret = dfb_layer_context_deactivate( current );
          if (ret) {
               D_ERROR("DirectFB/Core/layer: "
                        "Could not deactivate current context of '%s'! (%s)\n",
                        shared->description.name, DirectFBErrorString( ret ));
          }
     }

     shared->suspended = true;

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_resume( CoreLayer *layer )
{
     CoreLayerShared   *shared;
     CoreLayerContexts *contexts;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared   = layer->shared;
     contexts = &shared->contexts;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     D_ASSUME( shared->suspended );

     if (!shared->suspended) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_OK;
     }

     /* (Re)Activate current context. */
     if (contexts->active >= 0) {
          DFBResult         ret;
          CoreLayerContext *current = fusion_vector_at( &contexts->stack,
                                                         contexts->active );

          ret = dfb_layer_context_activate( current );
          if (ret) {
               D_ERROR("DirectFB/Core/layer: "
                        "Could not activate current context of '%s'! (%s)\n",
                        shared->description.name, DirectFBErrorString( ret ));
          }
     }

     shared->suspended = false;

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_create_context( CoreLayer         *layer,
                          CoreLayerContext **ret_context )
{
     DFBResult          ret;
     CoreLayerShared   *shared;
     CoreLayerContexts *contexts;
     CoreLayerContext  *context = NULL;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( ret_context != NULL );

     shared   = layer->shared;
     contexts = &shared->contexts;

     D_DEBUG_AT( Core_Layers, "%s (%s)\n", __FUNCTION__, shared->description.name );

     /* Create the object. */
     context = dfb_core_create_layer_context( layer->core );
     if (!context)
          return DFB_FUSION;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock )) {
          fusion_object_destroy( &context->object );
          return DFB_FUSION;
     }

     /* Initialize the new context. */
     ret = dfb_layer_context_init( context, layer );
     if (ret) {
          fusion_skirmish_dismiss( &shared->lock );
          return ret;
     }

     /* Add it to the context stack. */
     if (fusion_vector_add( &contexts->stack, context )) {
          dfb_layer_context_unref( context );
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_FUSION;
     }

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     /* Return the context. */
     *ret_context = context;

     return DFB_OK;
}

DFBResult
dfb_layer_get_active_context( CoreLayer         *layer,
                              CoreLayerContext **ret_context )
{
     CoreLayerShared   *shared;
     CoreLayerContexts *contexts;
     CoreLayerContext  *context;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( ret_context != NULL );

     shared   = layer->shared;
     contexts = &shared->contexts;

     D_DEBUG_AT( Core_Layers, "%s (%s)\n", __FUNCTION__, shared->description.name );

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     /* Check for active context. */
     if (contexts->active < 0) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_NOCONTEXT;
     }

     /* Fetch active context. */
     context = fusion_vector_at( &contexts->stack, contexts->active );

     /* Increase the context's reference counter. */
     if (dfb_layer_context_ref( context )) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_FUSION;
     }

     /* Return the context. */
     *ret_context = context;

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_get_primary_context( CoreLayer         *layer,
                               bool               activate,
                               CoreLayerContext **ret_context )
{
     DFBResult          ret;
     CoreLayerShared   *shared;
     CoreLayerContexts *contexts;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( ret_context != NULL );

     shared   = layer->shared;
     contexts = &shared->contexts;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     D_DEBUG_AT( Core_Layers, "%s (%s, %sactivate) <- active: %d\n",
                 __FUNCTION__, shared->description.name,
                 activate ? "" : "don't ", contexts->active );

     /* Check for primary context. */
     if (contexts->primary) {
          /* Increase the context's reference counter. */
          if (dfb_layer_context_ref( contexts->primary )) {
               fusion_skirmish_dismiss( &shared->lock );
               return DFB_FUSION;
          }
     }
     else {
          CoreLayerContext *primary;

          /* Unlock the layer. */
          fusion_skirmish_dismiss( &shared->lock );

          /* Create the primary (shared) context. */
          ret = dfb_layer_create_context( layer, &primary );
          if (ret)
               return ret;

          /* Lock the layer again. */
          if (fusion_skirmish_prevail( &shared->lock )) {
               dfb_layer_context_unref( primary );
               return DFB_FUSION;
          }

          /* Check if there was a race. */
          if (contexts->primary) {
               /* Throw away ours, the other was faster. */
               dfb_layer_context_unref( primary );

               /* Increase the context's reference counter. */
               if (dfb_layer_context_ref( contexts->primary )) {
                    fusion_skirmish_dismiss( &shared->lock );
                    return DFB_FUSION;
               }
          }
          else
               contexts->primary = primary;
     }

     /* Activate if no context is active? */
     if (contexts->active < 0 && activate) {
          ret = dfb_layer_activate_context( layer, contexts->primary );
          if (ret) {
               dfb_layer_context_unref( contexts->primary );
               fusion_skirmish_dismiss( &shared->lock );
               return ret;
          }
     }

     /* Return the context. */
     *ret_context = contexts->primary;

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_activate_context( CoreLayer        *layer,
                            CoreLayerContext *context )
{
     DFBResult          ret = DFB_OK;
     int                index;
     CoreLayerShared   *shared;
     CoreLayerContexts *ctxs;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( context != NULL );

     shared = layer->shared;
     ctxs   = &shared->contexts;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     D_DEBUG_AT( Core_Layers, "%s (%s, %p)\n", __FUNCTION__, shared->description.name, context );

     D_ASSERT( fusion_vector_contains( &ctxs->stack, context ) );

     /* Lookup the context in the context stack. */
     index = fusion_vector_index_of( &ctxs->stack, context );

     /* Lock the context. */
     if (dfb_layer_context_lock( context )) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_FUSION;
     }

     /* Need to activate? */
     if (ctxs->active != index) {
          DFBResult ret;

          /* Another context currently active? */
          if (ctxs->active >= 0) {
               CoreLayerContext *current = fusion_vector_at( &ctxs->stack,
                                                              ctxs->active );

               /* Deactivate current context. */
               if (!shared->suspended) {
                    ret = dfb_layer_context_deactivate( current );
                    if (ret)
                         goto error;
               }

               /* No active context. */
               ctxs->active = -1;
          }

          /* Activate context now. */
          if (!shared->suspended) {
               ret = dfb_layer_context_activate( context );
               if (ret)
                    goto error;
          }

          ctxs->active = index;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;

error:
     dfb_layer_context_unlock( context );

     fusion_skirmish_dismiss( &shared->lock );

     return ret;
}

DFBResult
dfb_layer_remove_context( CoreLayer        *layer,
                          CoreLayerContext *context )
{
     int                index;
     CoreLayerShared   *shared;
     CoreLayerContexts *ctxs;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( context != NULL );

     shared = layer->shared;
     ctxs   = &shared->contexts;

     /* Lock the layer. */
     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     D_DEBUG_AT( Core_Layers, "%s (%s, %p)\n", __FUNCTION__, shared->description.name, context );

     D_ASSUME( fusion_vector_contains( &ctxs->stack, context ) );

     /* Lookup the context in the context stack. */
     index = fusion_vector_index_of( &ctxs->stack, context );
     if (index < 0) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_OK;
     }

     /* Lock the context. */
     if (dfb_layer_context_lock( context )) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_FUSION;
     }

     /* Remove context from context stack. */
     fusion_vector_remove( &ctxs->stack, index );

     /* Check if the primary context is removed. */
     if (context == ctxs->primary)
          ctxs->primary = NULL;

     /* Need to deactivate? */
     if (ctxs->active == index) {
          /* Deactivate the context. */
          if (!shared->suspended)
               dfb_layer_context_deactivate( context );

          /* There's no active context anymore. */
          ctxs->active = -1;

          if (ctxs->primary)
               D_ASSERT( fusion_vector_contains( &ctxs->stack, ctxs->primary ) );

          if (fusion_vector_has_elements( &ctxs->stack )) {
               CoreLayerContext *ctx;

               /* Activate most recent context. */
               index = fusion_vector_size( &ctxs->stack ) - 1;
               ctx   = fusion_vector_at( &ctxs->stack, index );

               if (shared->suspended || dfb_layer_context_activate( ctx ) == DFB_OK)
                    ctxs->active = index;
          }
     }
     else if (ctxs->active > index) {
          /* Adjust index of active context due to the removed context. */
          ctxs->active--;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     /* Unlock the layer. */
     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_get_current_output_field( CoreLayer *layer, int *field )
{
     DFBResult ret;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( field != NULL );

     if (!layer->funcs->GetCurrentOutputField)
          return DFB_UNSUPPORTED;

     ret = layer->funcs->GetCurrentOutputField( layer, layer->driver_data,
                                                layer->layer_data, field );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
dfb_layer_get_level( CoreLayer *layer, int *ret_level )
{
     DisplayLayerFuncs *funcs;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( ret_level != NULL );

     funcs = layer->funcs;

     if (!funcs->GetLevel)
          return DFB_UNSUPPORTED;

     return funcs->GetLevel( layer, layer->driver_data,
                             layer->layer_data, ret_level );
}

DFBResult
dfb_layer_set_level( CoreLayer *layer, int level )
{
     DisplayLayerFuncs *funcs;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     if (!funcs->SetLevel)
          return DFB_UNSUPPORTED;

     return funcs->SetLevel( layer, layer->driver_data,
                             layer->layer_data, level );
}

DFBResult
dfb_layer_wait_vsync( CoreLayer *layer )
{
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->screen != NULL );

     return dfb_screen_wait_vsync( layer->screen );
}

DFBResult
dfb_layer_get_source_info( CoreLayer                        *layer,
                           int                               source,
                           DFBDisplayLayerSourceDescription *ret_desc )
{
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( source >= 0 );
     D_ASSERT( source < layer->shared->description.sources );
     D_ASSERT( ret_desc != NULL );

     *ret_desc = layer->shared->sources[source].description;

     return DFB_OK;
}

