/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

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

#ifndef __LAYERS_H__
#define __LAYERS_H__

#include <dfb_types.h>
#include <pthread.h>

#include <core/fusion/lock.h>
#include <core/fusion/reactor.h>

#include <directfb.h>
#include <core/coretypes.h>

#include <core/windows.h>

typedef struct {
     DFBDisplayLayerDescription  desc;  /* description of the layer's caps */
} DisplayLayerInfo;

typedef struct {
     int       (*LayerDataSize)     ();

     DFBResult (*InitLayer)         ( GraphicsDevice             *device,
                                      CoreLayer                  *layer,
                                      DisplayLayerInfo           *layer_info,
                                      DFBDisplayLayerConfig      *default_config,
                                      DFBColorAdjustment         *default_adj,
                                      void                       *driver_data,
                                      void                       *layer_data );

     /*
      * internal layer driver API
      */

     DFBResult (*Enable)            ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data );

     DFBResult (*Disable)           ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data );

     DFBResult (*TestConfiguration) ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config,
                                      DFBDisplayLayerConfigFlags *failed );

     DFBResult (*SetConfiguration)  ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config );

     DFBResult (*SetOpacity)        ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        opacity );

     DFBResult (*GetCurrentOutputField) ( CoreLayer              *layer,
                                          void                   *driver_data,
                                          void                   *layer_data,
                                          int                    *field );

     DFBResult (*SetFieldParity)    ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                         field );

     DFBResult (*WaitVSync)         ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data );

     DFBResult (*SetScreenLocation) ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      float                       x,
                                      float                       y,
                                      float                       width,
                                      float                       height );

     DFBResult (*SetSrcColorKey)    ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        r,
                                      __u8                        g,
                                      __u8                        b );

     DFBResult (*SetDstColorKey)    ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        r,
                                      __u8                        g,
                                      __u8                        b );

     DFBResult (*GetLevel)          ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                        *level );

     DFBResult (*SetLevel)          ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                         level );

     DFBResult (*FlipBuffers)       ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBSurfaceFlipFlags         flags );

     DFBResult (*UpdateRegion)      ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBRegion                  *region,
                                      DFBSurfaceFlipFlags         flags );

     DFBResult (*SetColorAdjustment)( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBColorAdjustment         *adj );

     DFBResult (*SetPalette)        ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      CorePalette                *palette );

     DFBResult (*SetField)          ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                         field );

     DFBResult (*SetScreenPowerMode)( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBScreenPowerMode          mode );

     /*
      * optional to override default surface (re)allocation
      */

     DFBResult (*AllocateSurface) ( CoreLayer                  *layer,
                                    void                       *driver_data,
                                    void                       *layer_data,
                                    DFBDisplayLayerConfig      *config,
                                    CoreSurface               **surface );

     DFBResult (*ReallocateSurface) ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config,
                                      CoreSurface                *surface );

     DFBResult (*DeallocateSurface) ( CoreLayer                  *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      CoreSurface                *surface );


     /*
      * hardware window support
      */
     int       (*WindowDataSize)     ();

     DFBResult (*AddWindow)    ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *window_data,
                                 CoreWindow                 *window );

     DFBResult (*UpdateWindow) ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *window_data,
                                 CoreWindow                 *window,
                                 CoreWindowUpdateFlags       flags );

     DFBResult (*RemoveWindow) ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *window_data,
                                 CoreWindow                 *window );
} DisplayLayerFuncs;


/*
 * Add a layer to a graphics device by pointing to a table
 * containing driver functions. The supplied driver data
 * will be passed to these functions.
 */
void dfb_layers_register( GraphicsDevice    *device,
                          void              *driver_data,
                          DisplayLayerFuncs *funcs );

/*
 * Replace functions of the primary layer implementation by passing
 * an alternative driver function table. All non-NULL functions in the new
 * table replace the functions in the original function table.
 * The original function table is written to 'primary_funcs' before to allow
 * drivers to use existing functionality from the original implementation.
 */
void dfb_layers_hook_primary( GraphicsDevice     *device,
                              void               *driver_data,
                              DisplayLayerFuncs  *funcs,
                              DisplayLayerFuncs  *primary_funcs,
                              void              **primary_driver_data );

/*
 * Replace functions of the primary layer implementation completely by passing
 * an alternative driver function table.
 */
void dfb_layers_replace_primary( GraphicsDevice     *device,
                                 void               *driver_data,
                                 DisplayLayerFuncs  *funcs );

typedef DFBEnumerationResult (*DisplayLayerCallback) (CoreLayer *layer,
                                                      void      *ctx);

void dfb_layers_enumerate( DisplayLayerCallback  callback,
                           void                 *ctx );

CoreLayer *dfb_layer_at( DFBDisplayLayerID id );
CoreLayer *dfb_layer_at_translated( DFBDisplayLayerID id );

/*
 * Lease layer during window stack repaints.
 */
DFBResult dfb_layer_lease( CoreLayer *layer );

/*
 * Purchase layer for exclusive access.
 */
DFBResult dfb_layer_purchase( CoreLayer *layer );

/*
 * Kill the process that purchased the layer.
 */
DFBResult dfb_layer_holdup( CoreLayer *layer );

/*
 * Release layer after lease/purchase.
 * Repaints the window stack if 'repaint' is true.
 */
void dfb_layer_release( CoreLayer *layer, bool repaint );


/*
 * enable/disable layer
 */
DFBResult dfb_layer_enable( CoreLayer *layer );
DFBResult dfb_layer_disable( CoreLayer *layer );

/*
 * configuration testing/setting/getting
 */
DFBResult dfb_layer_test_configuration( CoreLayer                  *layer,
                                        DFBDisplayLayerConfig      *config,
                                        DFBDisplayLayerConfigFlags *failed );

DFBResult dfb_layer_set_configuration( CoreLayer             *layer,
                                       DFBDisplayLayerConfig *config );

DFBResult dfb_layer_get_configuration( CoreLayer             *layer,
                                       DFBDisplayLayerConfig *config );

/*
 * background handling
 */
DFBResult dfb_layer_set_background_mode ( CoreLayer                     *layer,
                                          DFBDisplayLayerBackgroundMode  mode );

DFBResult dfb_layer_set_background_image( CoreLayer                     *layer,
                                          CoreSurface                   *image);

DFBResult dfb_layer_set_background_color( CoreLayer                     *layer,
                                          DFBColor                      *color);

/*
 * various functions
 */
CoreSurface       *dfb_layer_surface      ( const CoreLayer            *layer );

CardState         *dfb_layer_state        ( CoreLayer                  *layer );

void               dfb_layer_description  ( const CoreLayer            *layer,
                                            DFBDisplayLayerDescription *desc );

DFBDisplayLayerID  dfb_layer_id           ( const CoreLayer            *layer );

DFBDisplayLayerID  dfb_layer_id_translated( const CoreLayer            *layer );

DFBResult          dfb_layer_flip_buffers ( CoreLayer                  *layer,
                                            DFBSurfaceFlipFlags         flags );

DFBResult          dfb_layer_update_region( CoreLayer                  *layer,
                                            DFBRegion                  *region,
                                            DFBSurfaceFlipFlags         flags );

DFBResult dfb_layer_set_src_colorkey( CoreLayer *layer,
                                      __u8 r, __u8 g, __u8 b );

DFBResult dfb_layer_set_dst_colorkey( CoreLayer *layer,
                                      __u8 r, __u8 g, __u8 b );

DFBResult dfb_layer_get_level( CoreLayer *layer, int *level );
DFBResult dfb_layer_set_level( CoreLayer *layer, int level );

DFBResult dfb_layer_set_screenlocation( CoreLayer *layer,
                                        float x, float y,
                                        float width, float height );

DFBResult dfb_layer_set_opacity (CoreLayer *layer, __u8 opacity);

DFBResult dfb_layer_get_current_output_field( CoreLayer *layer, int *field );

DFBResult dfb_layer_set_field_parity( CoreLayer *layer, int field );

DFBResult dfb_layer_wait_vsync( CoreLayer *layer );

DFBResult dfb_layer_set_screen_power_mode( CoreLayer          *layer,
                                           DFBScreenPowerMode  mode );

DFBResult dfb_layer_set_coloradjustment (CoreLayer          *layer,
                                         DFBColorAdjustment *adj);

DFBResult dfb_layer_get_coloradjustment (CoreLayer          *layer,
                                         DFBColorAdjustment *adj);

DFBSurfacePixelFormat dfb_primary_layer_pixelformat();
void                  dfb_primary_layer_rectangle( float x, float y,
                                                   float w, float h,
                                                   DFBRectangle *rect );

/*
 * hardware window support
 */
DFBResult dfb_layer_add_window   ( CoreLayer             *layer,
                                   CoreWindow            *window );

DFBResult dfb_layer_update_window( CoreLayer             *layer,
                                   CoreWindow            *window,
                                   CoreWindowUpdateFlags  flags );

DFBResult dfb_layer_remove_window( CoreLayer             *layer,
                                   CoreWindow            *window );

/*
 * window control
 */
DFBResult dfb_layer_create_window( CoreLayer              *layer,
                                   int                     x,
                                   int                     y,
                                   int                     width,
                                   int                     height,
                                   DFBWindowCapabilities   caps,
                                   DFBSurfaceCapabilities  surface_caps,
                                   DFBSurfacePixelFormat   pixelformat,
                                   CoreWindow            **window );

CoreWindow *dfb_layer_find_window( CoreLayer              *layer,
                                   DFBWindowID             id );

CoreWindowStack *dfb_layer_window_stack( const CoreLayer *layer );

/*
 * cursor control
 */
DFBResult dfb_layer_cursor_enable( CoreLayer    *layer,
                                   bool          enable );

DFBResult dfb_layer_cursor_set_shape( CoreLayer   *layer,
                                      CoreSurface *shape,
                                      int          hot_x,
                                      int          hot_y );

DFBResult dfb_layer_cursor_set_opacity( CoreLayer *layer,
                                        __u8       opacity );

DFBResult dfb_layer_cursor_set_acceleration( CoreLayer *layer,
                                             int        numerator,
                                             int        denominator,
                                             int        threshold );

DFBResult dfb_layer_cursor_warp( CoreLayer *layer,
                                 int        x,
                                 int        y );


DFBResult dfb_layer_get_cursor_position (CoreLayer *layer,
                                         int       *x,
                                         int       *y);



/* global reactions */
ReactionResult _dfb_layer_background_image_listener( const void *msg_data,
                                                     void       *ctx );

#endif
