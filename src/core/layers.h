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

#include <asm/types.h>
#include <pthread.h>

#include <core/fusion/lock.h>

#include <directfb.h>
#include <core/coretypes.h>

#define DFB_DISPLAY_LAYER_INFO_NAME_LENGTH   30

typedef struct {
     DFBDisplayLayerDescription  desc;  /* description of the layer's caps */

     char                        name[DFB_DISPLAY_LAYER_INFO_NAME_LENGTH];
                                        /* description set by driver */

} DisplayLayerInfo;

typedef struct {
     int       (*LayerDataSize)     ();
     
     DFBResult (*InitLayer)         ( GraphicsDevice             *device,
                                      DisplayLayer               *layer,
                                      DisplayLayerInfo           *layer_info,
                                      DFBDisplayLayerConfig      *default_config,
                                      DFBColorAdjustment         *default_adj,
                                      void                       *driver_data,
                                      void                       *layer_data );

     /*
      * internal layer driver API
      */

     DFBResult (*Enable)            ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data );

     DFBResult (*Disable)           ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data );

     DFBResult (*TestConfiguration) ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config,
                                      DFBDisplayLayerConfigFlags *failed );

     DFBResult (*SetConfiguration)  ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config );

     DFBResult (*SetOpacity)        ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        opacity );
     
     DFBResult (*SetScreenLocation) ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      float                       x,
                                      float                       y,
                                      float                       width,
                                      float                       height );
     
     DFBResult (*SetSrcColorKey)    ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        r,
                                      __u8                        g,
                                      __u8                        b );
     
     DFBResult (*SetDstColorKey)    ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      __u8                        r,
                                      __u8                        g,
                                      __u8                        b );
     
     DFBResult (*GetLevel)          ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                        *level );
     
     DFBResult (*SetLevel)          ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                         level );
     
     DFBResult (*FlipBuffers)       ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBSurfaceFlipFlags         flags );
     
     DFBResult (*SetColorAdjustment)( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBColorAdjustment         *adj );

     DFBResult (*SetPalette)        ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      CorePalette                *palette );

     DFBResult (*SetField)          ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      int                         field );
     
     /*
      * optional to override default surface (re)allocation
      */

     DFBResult (*AllocateSurface) ( DisplayLayer               *layer,
                                    void                       *driver_data,
                                    void                       *layer_data,
                                    DFBDisplayLayerConfig      *config,
                                    CoreSurface               **surface );

     DFBResult (*ReallocateSurface) ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      DFBDisplayLayerConfig      *config,
                                      CoreSurface                *surface );

     DFBResult (*DeallocateSurface) ( DisplayLayer               *layer,
                                      void                       *driver_data,
                                      void                       *layer_data,
                                      CoreSurface                *surface );
} DisplayLayerFuncs;


DFBResult dfb_layers_initialize();
DFBResult dfb_layers_join();

DFBResult dfb_layers_init_all();
DFBResult dfb_layers_join_all();


DFBResult dfb_layers_shutdown( bool emergency );
DFBResult dfb_layers_leave( bool emergency );

#ifdef FUSION_FAKE
DFBResult dfb_layers_suspend();
DFBResult dfb_layers_resume();
#endif

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

typedef DFBEnumerationResult (*DisplayLayerCallback) (DisplayLayer *layer,
                                                      void         *ctx);

void dfb_layers_enumerate( DisplayLayerCallback  callback,
                           void                 *ctx );

DisplayLayer *dfb_layer_at( DFBDisplayLayerID id );

/*
 * Lease layer during window stack repaints.
 */
DFBResult dfb_layer_lease( DisplayLayer *layer );

/*
 * Purchase layer for exclusive access.
 */
DFBResult dfb_layer_purchase( DisplayLayer *layer );

/*
 * Release layer after lease/purchase.
 * Repaints the window stack if 'repaint' is true.
 */
void dfb_layer_release( DisplayLayer *layer, bool repaint );


/*
 * enable/disable layer
 */
DFBResult dfb_layer_enable( DisplayLayer *layer );
DFBResult dfb_layer_disable( DisplayLayer *layer );

/*
 * configuration testing/setting/getting
 */
DFBResult dfb_layer_test_configuration( DisplayLayer               *layer,
                                        DFBDisplayLayerConfig      *config,
                                        DFBDisplayLayerConfigFlags *failed );

DFBResult dfb_layer_set_configuration( DisplayLayer          *layer,
                                       DFBDisplayLayerConfig *config );

DFBResult dfb_layer_get_configuration( DisplayLayer          *layer,
                                       DFBDisplayLayerConfig *config );

/*
 * background handling
 */
DFBResult dfb_layer_set_background_mode ( DisplayLayer                  *layer,
                                          DFBDisplayLayerBackgroundMode  mode );

DFBResult dfb_layer_set_background_image( DisplayLayer                  *layer,
                                          CoreSurface                   *image);

DFBResult dfb_layer_set_background_color( DisplayLayer                  *layer,
                                          DFBColor                      *color);

/*
 * various functions
 */
CoreSurface       *dfb_layer_surface( const DisplayLayer *layer );
CardState         *dfb_layer_state( DisplayLayer *layer );
void               dfb_layer_description( const DisplayLayer         *layer,
                                          DFBDisplayLayerDescription *desc );
DFBDisplayLayerID  dfb_layer_id( const DisplayLayer *layer );

DFBResult          dfb_layer_flip_buffers( DisplayLayer *layer,
                                          DFBSurfaceFlipFlags flags );

DFBResult dfb_layer_create_window( DisplayLayer           *layer,
                                   int                     x,
                                   int                     y,
                                   int                     width,
                                   int                     height,
                                   DFBWindowCapabilities   caps,
                                   DFBSurfaceCapabilities  surface_caps,
                                   DFBSurfacePixelFormat   pixelformat,
                                   CoreWindow            **window );

CoreWindow *dfb_layer_find_window( DisplayLayer           *layer,
                                   DFBWindowID             id );

DFBResult dfb_layer_set_src_colorkey( DisplayLayer *layer,
                                      __u8 r, __u8 g, __u8 b );

DFBResult dfb_layer_set_dst_colorkey( DisplayLayer *layer,
                                      __u8 r, __u8 g, __u8 b );

DFBResult dfb_layer_get_level( DisplayLayer *layer, int *level );
DFBResult dfb_layer_set_level( DisplayLayer *layer, int level );

DFBResult dfb_layer_set_screenlocation( DisplayLayer *layer,
                                        float x, float y,
                                        float width, float height );

DFBResult dfb_layer_set_opacity (DisplayLayer *layer, __u8 opacity);

DFBResult dfb_layer_set_coloradjustment (DisplayLayer       *layer,
                                         DFBColorAdjustment *adj);

DFBResult dfb_layer_get_coloradjustment (DisplayLayer       *layer,
                                         DFBColorAdjustment *adj);

DFBResult dfb_layer_get_cursor_position (DisplayLayer       *layer,
                                         int                *x,
                                         int                *y);

DFBSurfacePixelFormat dfb_primary_layer_pixelformat();
void                  dfb_primary_layer_rectangle( float x, float y,
                                                   float w, float h,
                                                   DFBRectangle *rect );

/*
 * cursor control
 */
DFBResult dfb_layer_cursor_enable( DisplayLayer *layer,
                                   int           enable );

DFBResult dfb_layer_cursor_set_shape( DisplayLayer *layer,
                                      CoreSurface  *shape,
                                      int           hot_x,
                                      int           hot_y );

DFBResult dfb_layer_cursor_set_opacity( DisplayLayer *layer,
                                        __u8          opacity );

DFBResult dfb_layer_cursor_set_acceleration( DisplayLayer *layer,
                                             int           numerator,
                                             int           denominator,
                                             int           threshold );

DFBResult dfb_layer_cursor_warp( DisplayLayer *layer,
                                 int           x,
                                 int           y );


#endif
