/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <misc/util.h>


static void      init_region_config  ( DFBDisplayLayerConfig      *init,
                                       CoreLayerRegionConfig      *config );

static void      build_updated_config( CoreLayerRegion            *region,
                                       DFBDisplayLayerConfig      *update,
                                       CoreLayerRegionConfig      *ret_config,
                                       CoreLayerRegionConfigFlags *ret_flags );

static DFBResult allocate_surface    ( CoreLayer                  *layer,
                                       CoreLayerRegion            *region,
                                       CoreLayerRegionConfig      *config );

static DFBResult reallocate_surface  ( CoreLayer                  *layer,
                                       CoreLayerRegion            *region,
                                       CoreLayerRegionConfig      *config,
                                       DFBDisplayLayerConfig      *previous );

static DFBResult deallocate_surface  ( CoreLayer                  *layer,
                                       CoreLayerRegion            *region );

/******************************************************************************/

static void
context_destructor( FusionObject *object, bool zombie )
{
     CoreLayerContext *context = (CoreLayerContext*) object;
     CoreLayer        *layer   = dfb_layer_at( context->layer_id );
     CoreLayerShared  *shared  = layer->shared;

     (void) shared;

     DEBUGMSG("DirectFB/core/layers: destroying context %p (%s, %sactive%s)\n",
              context, shared->description.name, context->active ? "" : "in",
              zombie ? " - ZOMBIE" : "");

     /* Remove the context from the layer's context stack. */
     dfb_layer_remove_context( layer, context );

     /* Destroy the window stack. */
     if (context->stack)
          dfb_windowstack_destroy( context->stack );

     /* Destroy the region vector. */
     fusion_vector_destroy( &context->regions );

     /* Deinitialize the lock. */
     fusion_skirmish_destroy( &context->lock );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

/******************************************************************************/

FusionObjectPool *
dfb_layer_context_pool_create()
{
     return fusion_object_pool_create( "Layer Context Pool",
                                       sizeof(CoreLayerContext),
                                       sizeof(CoreLayerContextNotification),
                                       context_destructor );
}

/******************************************************************************/

DFBResult
dfb_layer_context_create( CoreLayer         *layer,
                          CoreLayerContext **ret_context )
{
     CoreLayerShared       *shared;
     CoreLayerContext      *context;
     CoreLayerRegionConfig  config;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( ret_context != NULL );

     shared = layer->shared;

     /* Create the object. */
     context = dfb_core_create_layer_context( layer->core );
     if (!context)
          return DFB_FUSION;

     /* Initialize the lock. */
     if (fusion_skirmish_init( &context->lock )) {
          fusion_object_destroy( &context->object );
          return DFB_FUSION;
     }

     /* Initialize the region vector. */
     fusion_vector_init( &context->regions, 4 );

     /* Store layer ID and default configuration. */
     context->layer_id = shared->layer_id;
     context->config   = shared->default_config;

     /* Activate the object. */
     fusion_object_activate( &context->object );

     /* Create the primary region. */
     dfb_layer_region_create( layer, context, &context->primary );

     /* Set the primary region's default configuration. */
     init_region_config( &context->config, &config );

     dfb_layer_region_set_configuration( context->primary, &config, CLRCF_ALL );

     /* Create the window stack. */
     context->stack = dfb_windowstack_create( context );

     /* Return the new context. */
     *ret_context = context;

     return DFB_OK;
}

DFBResult
dfb_layer_context_activate( CoreLayerContext *context )
{
     int              index;
     CoreLayerRegion *region;

     DFB_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     DFB_ASSUME( !context->active );

     if (context->active) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Iterate through all regions. */
     fusion_vector_foreach (&context->regions, index, region) {
          /* Activate each region. */
          dfb_layer_region_activate( region );
     }

     context->active = true;

     /* Resume window stack. */
     if (context->stack) {
          CoreWindowStack *stack = context->stack;

          /* Enables input and output. */
          stack->active = true;

          /* FIXME: call only if really needed */
          dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_deactivate( CoreLayerContext *context )
{
     int              index;
     CoreLayerRegion *region;

     DFB_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     DFB_ASSUME( context->active );

     if (!context->active) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Iterate through all regions. */
     fusion_vector_foreach (&context->regions, index, region) {
          /* Deactivate each region. */
          dfb_layer_region_deactivate( region );
     }

     context->active = false;

     /* Suspend window stack. */
     if (context->stack)
          context->stack->active = false;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_add_region( CoreLayerContext *context,
                              CoreLayerRegion  *region )
{
     DFB_ASSERT( context != NULL );
     DFB_ASSERT( region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     DFB_ASSUME( ! fusion_vector_contains( &context->regions, region ) );

     if (fusion_vector_contains( &context->regions, region )) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Add region to vector. */
     if (fusion_vector_add( &context->regions, region )) {
          dfb_layer_context_unlock( context );
          return DFB_FUSION;
     }

     /* Inherit state from context. */
     region->state = context->active ? CLRSF_ACTIVE : CLRSF_NONE;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_remove_region( CoreLayerContext *context,
                                 CoreLayerRegion  *region )
{
     int index;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     DFB_ASSUME( fusion_vector_contains( &context->regions, region ) );

     /* Lookup region. */
     index = fusion_vector_index_of( &context->regions, region );
     if (index < 0) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     if (index >= 0) {
          /* Remove region from vector. */
          fusion_vector_remove( &context->regions, index );
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_primary_region( CoreLayerContext  *context,
                                      CoreLayerRegion  **ret_region )
{
     DFB_ASSERT( context != NULL );
     DFB_ASSERT( ret_region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Increase the primary region's reference counter. */
     if (dfb_layer_region_ref( context->primary )) {
          dfb_layer_context_unlock( context );
          return DFB_FUSION;
     }

     /* Return region. */
     *ret_region = context->primary;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

/*
 * configuration management
 */
DFBResult
dfb_layer_context_test_configuration( CoreLayerContext           *context,
                                      DFBDisplayLayerConfig      *config,
                                      DFBDisplayLayerConfigFlags *ret_failed )
{
     DFBResult                   ret;
     CoreLayer                  *layer;
     CoreLayerRegionConfig       region_config;
     CoreLayerRegionConfigFlags  failed;
     DisplayLayerFuncs          *funcs;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );
     DFB_ASSERT( config != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     layer = dfb_layer_at( context->layer_id );

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Build region configuration to be tested. */
     build_updated_config( context->primary, config, &region_config, NULL );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );


     DFB_ASSERT( funcs->TestRegion != NULL );

     /* Let the driver examine the modified configuration. */
     ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data,
                              &region_config, &failed );

     /* Return flags for failing entries. */
     if (ret_failed) {
          DFBDisplayLayerConfigFlags flags = DLCONF_NONE;

          /* Translate flags. */
          if (ret != DFB_OK) {
               if (failed & CLRCF_WIDTH)
                    flags |= DLCONF_WIDTH;

               if (failed & CLRCF_HEIGHT)
                    flags |= DLCONF_HEIGHT;

               if (failed & CLRCF_FORMAT)
                    flags |= DLCONF_PIXELFORMAT;

               if (failed & CLRCF_BUFFERMODE)
                    flags |= DLCONF_BUFFERMODE;

               if (failed & CLRCF_OPTIONS)
                    flags |= DLCONF_OPTIONS;
          }

          *ret_failed = flags;
     }

     return ret;
}

DFBResult
dfb_layer_context_set_configuration( CoreLayerContext      *context,
                                     DFBDisplayLayerConfig *config )
{
     DFBResult                   ret;
     CoreLayer                  *layer;
     CoreLayerRegion            *region;
     CoreLayerRegionConfig       region_config;
     CoreLayerRegionConfigFlags  flags;
     DisplayLayerFuncs          *funcs;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );
     DFB_ASSERT( config != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     layer = dfb_layer_at( context->layer_id );

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->funcs->TestRegion != NULL );

     funcs  = layer->funcs;
     region = context->primary;

     /* Lock the region. */
     if (dfb_layer_region_lock( region )) {
          dfb_layer_context_unlock( context );
          return DFB_FUSION;
     }

     /* Build region configuration to be set. */
     build_updated_config( context->primary, config, &region_config, &flags );

     /* Special hardware window mode? */
     if (region_config.buffermode == DLBM_WINDOWS) {
          /* Disable and deallocate the primary region. */
          if (FLAG_IS_SET( region->state, CLRSF_ENABLED )) {
               dfb_layer_region_disable( region );
               deallocate_surface( layer, region );
          }

          /* Remember configuration. */
          region->config = region_config;
     }
     else {
          /* Test new configuration first. */
          ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data,
                                   &region_config, NULL );
          if (ret) {
               dfb_layer_region_unlock( region );
               dfb_layer_context_unlock( context );
               return ret;
          }

          /* (Re)allocate the region's surface. */
          if (FLAG_IS_SET( region->state, CLRSF_ENABLED ))
               ret = reallocate_surface( layer, region,
                                         &region_config, &context->config );
          else
               ret = allocate_surface( layer, region, &region_config );

          if (ret) {
               dfb_layer_region_unlock( region );
               dfb_layer_context_unlock( context );
               return ret;
          }

          /* Set the new region configuration. */
          dfb_layer_region_set_configuration( region, &region_config, flags );

          /* Enable the primary region. */
          if (! FLAG_IS_SET( region->state, CLRSF_ENABLED ))
               dfb_layer_region_enable( region );
     }

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     /*
      * Write back modified entries.
      */
     if (config->flags & DLCONF_WIDTH)
          context->config.width = config->width;

     if (config->flags & DLCONF_HEIGHT)
          context->config.height = config->height;

     if (config->flags & DLCONF_PIXELFORMAT)
          context->config.pixelformat = config->pixelformat;

     if (config->flags & DLCONF_BUFFERMODE)
          context->config.buffermode = config->buffermode;

     if (config->flags & DLCONF_OPTIONS)
          context->config.options = config->options;

     /* Update the window stack. */
     if (context->stack) {
          CoreWindowStack *stack = context->stack;

          /* Update hardware flag. */
          stack->hw_mode = (context->config.buffermode == DLBM_WINDOWS);

          /* Tell the windowing core about the new size. */
          dfb_windowstack_resize( stack,
                                  context->config.width,
                                  context->config.height );

          /* FIXME: call only if really needed */
          dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_configuration( CoreLayerContext      *context,
                                     DFBDisplayLayerConfig *config )
{
     DFB_ASSERT( context != NULL );
     DFB_ASSERT( config != NULL );

     *config = context->config;

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_src_colorkey( CoreLayerContext *context,
                                    __u8 r, __u8 g, __u8 b )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     config.src_key.r = r;
     config.src_key.g = g;
     config.src_key.b = b;

     ret = dfb_layer_region_set_configuration( context->primary,
                                               &config, CLRCF_SRCKEY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_dst_colorkey( CoreLayerContext *context,
                                    __u8 r, __u8 g, __u8 b )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     config.dst_key.r = r;
     config.dst_key.g = g;
     config.dst_key.b = b;

     ret = dfb_layer_region_set_configuration( context->primary,
                                               &config, CLRCF_DSTKEY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_screenlocation( CoreLayerContext *context,
                                      float x, float y,
                                      float width, float height )
{
     DFBResult              ret;
     VideoMode             *mode;
     CoreLayerRegionConfig  config;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     if (context->screen.x == x     && context->screen.y == y &&
         context->screen.w == width && context->screen.h == height)
     {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     mode = dfb_system_current_mode();
     if (!mode) {
          dfb_layer_context_unlock( context );
          return DFB_UNIMPLEMENTED;
     }

     config.dest.x = x * mode->xres;
     config.dest.y = y * mode->yres;
     config.dest.w = width * mode->xres;
     config.dest.h = height * mode->yres;

     ret = dfb_layer_region_set_configuration( context->primary,
                                               &config, CLRCF_DEST );
     if (ret == DFB_OK) {
          context->screen.x = x;
          context->screen.y = y;
          context->screen.w = width;
          context->screen.h = height;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_opacity (CoreLayerContext *context, __u8 opacity)
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     if (context->opacity == opacity) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     config.opacity = opacity;

     ret = dfb_layer_region_set_configuration( context->primary,
                                               &config, CLRCF_OPACITY );
     if (ret == DFB_OK)
          context->opacity = opacity;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_coloradjustment (CoreLayerContext   *context,
                                       DFBColorAdjustment *adj)
{
     DFBResult                ret;
     DFBColorAdjustmentFlags  unchanged;
     CoreLayer               *layer;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( adj != NULL );

     layer = dfb_layer_at( context->layer_id );

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );


     unchanged = ~adj->flags & context->adjustment.flags;

     if (!layer->funcs->SetColorAdjustment)
          return DFB_UNSUPPORTED;

     /* if flags are set that are not in the default adjustment */
     if (adj->flags & ~context->adjustment.flags)
          return DFB_UNSUPPORTED;

     /* fill unchanged values */
     if (unchanged & DCAF_BRIGHTNESS)
          adj->brightness = context->adjustment.brightness;

     if (unchanged & DCAF_CONTRAST)
          adj->contrast = context->adjustment.contrast;

     if (unchanged & DCAF_HUE)
          adj->hue = context->adjustment.hue;

     if (unchanged & DCAF_SATURATION)
          adj->saturation = context->adjustment.saturation;

     /* set new adjustment */
     ret = layer->funcs->SetColorAdjustment( layer, layer->driver_data,
                                             layer->layer_data, adj );
     if (ret)
          return ret;

     /* write back any changed values */
     if (adj->flags & DCAF_BRIGHTNESS)
          context->adjustment.brightness = adj->brightness;

     if (adj->flags & DCAF_CONTRAST)
          context->adjustment.contrast = adj->contrast;

     if (adj->flags & DCAF_HUE)
          context->adjustment.hue = adj->hue;

     if (adj->flags & DCAF_SATURATION)
          context->adjustment.saturation = adj->saturation;

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_coloradjustment (CoreLayerContext   *context,
                                       DFBColorAdjustment *adj)
{
     DFB_ASSERT( context != NULL );
     DFB_ASSERT( adj != NULL );

     *adj = context->adjustment;

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_field_parity( CoreLayerContext *context, int field )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->primary != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     config.parity = field;

     ret = dfb_layer_region_set_configuration( context->primary,
                                               &config, CLRCF_PARITY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_create_window( CoreLayerContext       *context,
                                 int                     x,
                                 int                     y,
                                 int                     width,
                                 int                     height,
                                 DFBWindowCapabilities   caps,
                                 DFBSurfaceCapabilities  surface_caps,
                                 DFBSurfacePixelFormat   pixelformat,
                                 CoreWindow            **ret_window )
{
     DFBResult        ret;
     CoreWindow      *window;
     CoreWindowStack *stack;
     CoreLayer       *layer;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->stack != NULL );
     DFB_ASSERT( ret_window != NULL );

     layer = dfb_layer_at( context->layer_id );

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );

     if (dfb_layer_context_lock( context ))
         return DFB_FUSION;

     stack = context->stack;

     if (!stack->cursor.set)
          dfb_windowstack_cursor_enable( stack, true );

     ret = dfb_window_create( stack, x, y, width, height,
                              caps, surface_caps, pixelformat, &window );
     if (ret) {
          dfb_layer_context_unlock( context );
          return ret;
     }

     *ret_window = window;

     dfb_layer_context_unlock( context );

     return DFB_OK;
}

CoreWindow *
dfb_layer_context_find_window( CoreLayerContext *context, DFBWindowID id )
{
     int               i;
     CoreWindowStack  *stack;
     int               num;
     CoreWindow       *window = NULL;
     CoreWindow      **windows;

     DFB_ASSERT( context != NULL );
     DFB_ASSERT( context->stack );

     stack = context->stack;

     if (dfb_layer_context_lock( context ))
         return NULL;

     num     = stack->num_windows;
     windows = stack->windows;

     for (i=0; i<num; i++) {
          if (windows[i]->id == id) {
               window = windows[i];
               break;
          }
     }

     if (window && dfb_window_ref( window ))
          window = NULL;

     dfb_layer_context_unlock( context );

     return window;
}

CoreWindowStack *
dfb_layer_context_windowstack( CoreLayerContext *context )
{
     DFB_ASSERT( context != NULL );

     return context->stack;
}

inline FusionResult
dfb_layer_context_lock( CoreLayerContext *context )
{
     DFB_ASSERT( context != NULL );

     return fusion_skirmish_prevail( &context->lock );
}

inline FusionResult
dfb_layer_context_unlock( CoreLayerContext *context )
{
     DFB_ASSERT( context != NULL );

     return fusion_skirmish_dismiss( &context->lock );
}

/******************************************************************************/

/*
 * region config construction
 */
static void
init_region_config( DFBDisplayLayerConfig *init,
                    CoreLayerRegionConfig *config )
{
     DFB_ASSERT( init   != NULL );
     DFB_ASSERT( config != NULL );

     /* Initialize values from layer config. */
     config->width      = init->width;
     config->height     = init->height;
     config->format     = init->pixelformat;
     config->buffermode = init->buffermode;
     config->options    = init->options;

     /* Initialize source rectangle. */
     config->source.x = 0;
     config->source.y = 0;
     config->source.w = config->width;
     config->source.h = config->height;

     /* FIXME */
     config->dest.x = 0;
     config->dest.y = 0;
     config->dest.w = config->width;
     config->dest.h = config->height;

     /* Set default opacity. */
     config->opacity = 0xff;
}

static void
build_updated_config( CoreLayerRegion            *region,
                      DFBDisplayLayerConfig      *update,
                      CoreLayerRegionConfig      *ret_config,
                      CoreLayerRegionConfigFlags *ret_flags )
{
     CoreLayerRegionConfigFlags flags = CLRCF_NONE;

     DFB_ASSERT( region != NULL );
     DFB_ASSERT( update != NULL );
     DFB_ASSERT( ret_config != NULL );

     /* Get the current region configuration. */
     dfb_layer_region_get_configuration( region, ret_config );

     /* Change width. */
     if (update->flags & DLCONF_WIDTH) {
          flags |= CLRCF_WIDTH;
          ret_config->width = update->width;
     }

     /* Change height. */
     if (update->flags & DLCONF_HEIGHT) {
          flags |= CLRCF_HEIGHT;
          ret_config->height = update->height;
     }

     /* Change pixel format. */
     if (update->flags & DLCONF_PIXELFORMAT) {
          flags |= CLRCF_FORMAT;
          ret_config->format = update->pixelformat;
     }

     /* Change buffer mode. */
     if (update->flags & DLCONF_BUFFERMODE) {
          flags |= CLRCF_BUFFERMODE;
          ret_config->buffermode = update->buffermode;
     }

     /* Change options. */
     if (update->flags & DLCONF_OPTIONS) {
          flags |= CLRCF_OPTIONS;
          ret_config->options = update->options;
     }

     /* Return translated flags. */
     if (ret_flags)
          *ret_flags = flags;
}

/*
 * region surface (re/de)allocation
 */
static DFBResult
allocate_surface( CoreLayer             *layer,
                  CoreLayerRegion       *region,
                  CoreLayerRegionConfig *config )
{
     DFBResult               ret;
     DisplayLayerFuncs      *funcs;
     CoreSurface            *surface = NULL;
     DFBSurfaceCapabilities  caps    = DSCAPS_VIDEOONLY;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( region != NULL );
     DFB_ASSERT( region->surface == NULL );
     DFB_ASSERT( config != NULL );
     DFB_ASSERT( config->buffermode != DLBM_WINDOWS );

     funcs = layer->funcs;

     /*
      * Create a new surface for the region.
      * Drivers may provide their own surface creation (unusual).
      */
     if (funcs->AllocateSurface) {
          /* Let the driver create the surface. */
          ret = funcs->AllocateSurface( layer, layer->driver_data,
                                        layer->layer_data, region->region_data,
                                        config, &surface );
          if (ret) {
               ERRORMSG( "DirectFB/core/layers: AllocateSurface() failed!\n" );
               return ret;
          }
     }
     else {
          /* Choose surface capabilities depending on the buffer mode. */
          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    break;

               case DLBM_TRIPLE:
                    caps |= DSCAPS_TRIPLE;
                    break;

               case DLBM_BACKVIDEO:
                    caps |= DSCAPS_FLIPPING;
                    break;

               case DLBM_BACKSYSTEM:
                    ONCE("DLBM_BACKSYSTEM as default is unimplemented");
                    break;

               default:
                    BUG("unknown buffermode");
                    break;
          }

          if (config->options & DLOP_DEINTERLACING)
               caps |= DSCAPS_INTERLACED;

          /* Use the default surface creation. */
          ret = dfb_surface_create( layer->core,
                                    config->width, config->height,
                                    config->format, CSP_VIDEOONLY,
                                    caps, NULL, &surface );
          if (ret) {
               ERRORMSG( "DirectFB/core/layers: Surface creation failed!\n" );
               return ret;
          }
     }

     /* Tell the region about it's new surface (adds a global reference). */
     ret = dfb_layer_region_set_surface( region, surface );

     /* Remove local reference of dfb_surface_create(). */
     dfb_surface_unref( surface );

     return ret;
}

static DFBResult
reallocate_surface( CoreLayer             *layer,
                    CoreLayerRegion       *region,
                    CoreLayerRegionConfig *config,
                    DFBDisplayLayerConfig *previous )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;
     CoreSurface       *surface;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( region != NULL );
     DFB_ASSERT( region->surface != NULL );
     DFB_ASSERT( config != NULL );
     DFB_ASSERT( config->buffermode != DLBM_WINDOWS );
     DFB_ASSERT( previous != NULL );
     DFB_ASSERT( previous->buffermode != DLBM_WINDOWS );

     funcs   = layer->funcs;
     surface = region->surface;

     if (funcs->ReallocateSurface)
          return funcs->ReallocateSurface( layer, layer->driver_data,
                                           layer->layer_data,
                                           region->region_data,
                                           config, surface );

     /* FIXME: write surface management functions
               for easier configuration changes */

     if (previous->buffermode != config->buffermode) {
          DFBSurfaceCapabilities old_caps = surface->caps;

          switch (config->buffermode) {
               case DLBM_TRIPLE:
                    surface->caps |=  DSCAPS_TRIPLE;
                    surface->caps &= ~DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |=  DSCAPS_FLIPPING;
                    surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |=  DSCAPS_FLIPPING;
                    surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_SYSTEMONLY );
                    break;
               case DLBM_FRONTONLY:
                    surface->caps &= ~(DSCAPS_FLIPPING | DSCAPS_TRIPLE);
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;

               default:
                    BUG("unknown buffermode");
                    return DFB_BUG;
          }

          if (ret) {
               surface->caps = old_caps;
               return ret;
          }
     }

     if (config->width  != previous->width  ||
         config->height != previous->height ||
         config->format != previous->pixelformat)
     {
          ret = dfb_surface_reformat( layer->core, surface, config->width,
                                      config->height, config->format );
          if (ret)
               return ret;
     }

     if (config->options & DLOP_DEINTERLACING)
          surface->caps |= DSCAPS_INTERLACED;
     else
          surface->caps &= ~DSCAPS_INTERLACED;

     return DFB_OK;
}

static DFBResult
deallocate_surface( CoreLayer *layer, CoreLayerRegion *region )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;
     CoreSurface       *surface;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( region != NULL );

     DFB_ASSUME( region->surface != NULL );

     funcs   = layer->funcs;
     surface = region->surface;

     if (surface) {
          /* Special deallocation by the driver. */
          if (funcs->DeallocateSurface) {
               ret = funcs->DeallocateSurface( layer, layer->driver_data,
                                               layer->layer_data,
                                               region->region_data, surface );
               if (ret)
                    return ret;
          }

          /* Detach the global listener. */
          dfb_surface_detach_global( surface, &region->surface_reaction );

          /* Unlink from structure. */
          dfb_surface_unlink( &region->surface );
     }

     return DFB_OK;
}

