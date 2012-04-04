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

#ifndef __CORE__LAYERS_H__
#define __CORE__LAYERS_H__

#include <directfb.h>

#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surface_buffer.h>


struct __DFB_CoreLayerRegionConfig {
     int                        width;            /* width of the source in pixels */
     int                        height;           /* height of the source in pixels */
     DFBSurfacePixelFormat      format;           /* pixel format of the source surface */
     DFBSurfaceColorSpace       colorspace;       /* color space of the source surface */
     DFBSurfaceCapabilities     surface_caps;     /* capabilities of the source surface */
     DFBDisplayLayerBufferMode  buffermode;       /* surface buffer configuration */

     DFBDisplayLayerOptions     options;          /* various configuration options */

     DFBDisplayLayerSourceID    source_id;        /* selected source */

     DFBRectangle               source;           /* viewport within source (input) */
     DFBRectangle               dest;             /* viewport on screen (output) */

     u8                         opacity;          /* global region alpha */

     DFBColorKey                src_key;          /* source color key */
     DFBColorKey                dst_key;          /* destination color key */

     int                        parity;           /* field parity (for interlaced) */

     u8                         alpha_ramp[4];    /* alpha values for 1 or 2 bit lookup */

     DFBRegion                 *clips;            /* clip regions */
     int                        num_clips;        /* number of clip regions */
     DFBBoolean                 positive;         /* show or cut out regions */

     bool                       keep_buffers;
};

#if D_DEBUG_ENABLED
#define DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( domain, config )                                                    \
     do {                                                                                                          \
          const CoreLayerRegionConfig *_config = config;                                                           \
                                                                                                                   \
          D_DEBUG_AT( domain, "  -> size       %dx%d\n", _config->width, _config->height );                        \
          D_DEBUG_AT( domain, "  -> format     %s\n", dfb_pixelformat_name( _config->format ) );                   \
          D_DEBUG_AT( domain, "  -> color spc  %d\n", _config->colorspace );                                       \
          D_DEBUG_AT( domain, "  -> surf caps  0x%08x\n", _config->surface_caps );                                 \
          D_DEBUG_AT( domain, "  -> buffermode %d\n", _config->buffermode );                                       \
          D_DEBUG_AT( domain, "  -> options    0x%08x\n", _config->options );                                      \
          D_DEBUG_AT( domain, "  -> source     %d,%d-%dx%d\n", DFB_RECTANGLE_VALS(&_config->source) );             \
          D_DEBUG_AT( domain, "  -> dest       %d,%d-%dx%d\n", DFB_RECTANGLE_VALS(&_config->dest) );               \
          D_DEBUG_AT( domain, "  -> opacity    %d\n", _config->opacity );                                          \
          D_DEBUG_AT( domain, "  -> src_key    %02x%02x%02x (index %d)\n", DFB_COLORKEY_VALS(&_config->src_key) ); \
          D_DEBUG_AT( domain, "  -> dst_key    %02x%02x%02x (index %d)\n", DFB_COLORKEY_VALS(&_config->dst_key) ); \
     } while (0)
#else
#define DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( domain, config )                                                    \
     do {                                                                                                          \
     } while (0)
#endif

typedef enum {
     CLRCF_NONE         = 0x00000000,

     CLRCF_WIDTH        = 0x00000001,
     CLRCF_HEIGHT       = 0x00000002,
     CLRCF_FORMAT       = 0x00000004,
     CLRCF_SURFACE_CAPS = 0x00000008,

     CLRCF_BUFFERMODE   = 0x00000010,
     CLRCF_OPTIONS      = 0x00000020,
     CLRCF_SOURCE_ID    = 0x00000040,
     CLRCF_COLORSPACE   = 0x00000080,

     CLRCF_SOURCE       = 0x00000100,
     CLRCF_DEST         = 0x00000200,
     CLRCF_CLIPS        = 0x00000400,

     CLRCF_OPACITY      = 0x00001000,
     CLRCF_ALPHA_RAMP   = 0x00002000,

     CLRCF_SRCKEY       = 0x00010000,
     CLRCF_DSTKEY       = 0x00020000,

     CLRCF_PARITY       = 0x00100000,

     CLRCF_SURFACE      = 0x10000000,
     CLRCF_PALETTE      = 0x20000000,

     CLRCF_FREEZE       = 0x80000000,

     CLRCF_ALL          = 0xB01337FF
} CoreLayerRegionConfigFlags;

typedef struct {
   /** Driver Control **/

     /*
      * Return size of layer data (shared memory).
      */
     int       (*LayerDataSize) ( void );

     /*
      * Return size of region data (shared memory).
      */
     int       (*RegionDataSize)( void );

     /*
      * Called once by the master to initialize layer data and reset hardware.
      * Return layer description, default configuration and color adjustment.
      */
     DFBResult (*InitLayer)     ( CoreLayer                  *layer,
                                  void                       *driver_data,
                                  void                       *layer_data,
                                  DFBDisplayLayerDescription *description,
                                  DFBDisplayLayerConfig      *config,
                                  DFBColorAdjustment         *adjustment );

     /*
      * Called once by the master to shutdown the layer.
      * Use this function to free any resources that were taken during init.
      */
     DFBResult (*ShutdownLayer) ( CoreLayer                  *layer,
                                  void                       *driver_data,
                                  void                       *layer_data );

     /*
      * Called once by the master for each source.
      * Driver fills description.
      */
     DFBResult (*InitSource)    ( CoreLayer                         *layer,
                                  void                              *driver_data,
                                  void                              *layer_data,
                                  int                                source,
                                  DFBDisplayLayerSourceDescription  *description );


   /** Layer Control **/

     /*
      * Return the currently displayed field (interlaced only).
      */
     DFBResult (*GetCurrentOutputField)( CoreLayer              *layer,
                                         void                   *driver_data,
                                         void                   *layer_data,
                                         int                    *field );

     /*
      * Return the z position of the layer.
      */
     DFBResult (*GetLevel)             ( CoreLayer              *layer,
                                         void                   *driver_data,
                                         void                   *layer_data,
                                         int                    *level );

     /*
      * Move the layer below or on top of others (z position).
      */
     DFBResult (*SetLevel)             ( CoreLayer              *layer,
                                         void                   *driver_data,
                                         void                   *layer_data,
                                         int                     level );


   /** Configuration **/

     /*
      * Adjust brightness, contrast, saturation etc.
      */
     DFBResult (*SetColorAdjustment)   ( CoreLayer              *layer,
                                         void                   *driver_data,
                                         void                   *layer_data,
                                         DFBColorAdjustment     *adjustment );

     /*
      * Set the stereo depth for L/R mono and stereo layers.     // FIXME: Use SetRegion()!
      */
     DFBResult (*SetStereoDepth)       ( CoreLayer              *layer,
                                         void                   *driver_data,
                                         void                   *layer_data,
                                         bool                    follow_video,
                                         int                     z );


   /** Region Control **/

     /*
      * Check all parameters and return if this region is supported.
      */
     DFBResult (*TestRegion)   ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 CoreLayerRegionConfig      *config,
                                 CoreLayerRegionConfigFlags *failed );

     /*
      * Add a new region to the layer, but don't program hardware, yet.
      */
     DFBResult (*AddRegion)    ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data,
                                 CoreLayerRegionConfig      *config );

     /*
      * Setup hardware, called once after AddRegion() or when parameters
      * have changed. Surface and palette are only set if updated or new.
      */
     DFBResult (*SetRegion)    ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data,
                                 CoreLayerRegionConfig      *config,
                                 CoreLayerRegionConfigFlags  updated,
                                 CoreSurface                *surface,
                                 CorePalette                *palette,
                                 CoreSurfaceBufferLock      *left_lock, 
                                 CoreSurfaceBufferLock      *right_lock );

     /*
      * Remove a region from the layer.
      */
     DFBResult (*RemoveRegion) ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data );

     /*
      * Flip the surface of the region.
      */
     DFBResult (*FlipRegion)   ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data,
                                 CoreSurface                *surface,
                                 DFBSurfaceFlipFlags         flags,
                                 CoreSurfaceBufferLock      *left_lock, 
                                 CoreSurfaceBufferLock      *right_lock );

     /*
      * Indicate updates to the front buffer content.
      */
     DFBResult (*UpdateRegion) ( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data,
                                 CoreSurface                *surface,
                                 const DFBRegion            *left_update,
                                 CoreSurfaceBufferLock      *left_lock, 
                                 const DFBRegion            *right_update,
                                 CoreSurfaceBufferLock      *right_lock );

     /*
      * Control hardware deinterlacing.
      */
     DFBResult (*SetInputField)( CoreLayer                  *layer,
                                 void                       *driver_data,
                                 void                       *layer_data,
                                 void                       *region_data,
                                 int                         field );


   /** Override defaults. Subject to change. **/

     /*
      * Allocate the surface of the region.
      */
     DFBResult (*AllocateSurface)  ( CoreLayer              *layer,
                                     void                   *driver_data,
                                     void                   *layer_data,
                                     void                   *region_data,
                                     CoreLayerRegionConfig  *config,
                                     CoreSurface           **ret_surface );

     /*
      * Reallocate the surface of the region.
      */
     DFBResult (*ReallocateSurface)( CoreLayer              *layer,
                                     void                   *driver_data,
                                     void                   *layer_data,
                                     void                   *region_data,
                                     CoreLayerRegionConfig  *config,
                                     CoreSurface            *surface );

     /*
      * Deallocate the surface of the region.
      */
     DFBResult (*DeallocateSurface)( CoreLayer              *layer,
                                     void                   *driver_data,
                                     void                   *layer_data,
                                     void                   *region_data,
                                     CoreSurface            *surface );
} DisplayLayerFuncs;


/*
 * Add a layer to a graphics device by pointing to a table
 * containing driver functions. The supplied driver data
 * will be passed to these functions.
 */
CoreLayer *dfb_layers_register( CoreScreen              *screen,
                                void                    *driver_data,
                                const DisplayLayerFuncs *funcs );

/*
 * Replace functions of the primary layer implementation by passing
 * an alternative driver function table. All non-NULL functions in the new
 * table replace the functions in the original function table.
 * The original function table is written to 'primary_funcs' before to allow
 * drivers to use existing functionality from the original implementation.
 */
CoreLayer *dfb_layers_hook_primary( CoreGraphicsDevice *device,
                                    void               *driver_data,
                                    DisplayLayerFuncs  *funcs,
                                    DisplayLayerFuncs  *primary_funcs,
                                    void              **primary_driver_data );

/*
 * Replace functions of the primary layer implementation completely by passing
 * an alternative driver function table.
 */
CoreLayer *dfb_layers_replace_primary( CoreGraphicsDevice *device,
                                       void               *driver_data,
                                       DisplayLayerFuncs  *funcs );

typedef DFBEnumerationResult (*DisplayLayerCallback) (CoreLayer *layer,
                                                      void      *ctx);

void dfb_layers_enumerate( DisplayLayerCallback  callback,
                           void                 *ctx );


int        dfb_layer_num( void );

CoreLayer *dfb_layer_at( DFBDisplayLayerID id );

CoreLayer *dfb_layer_at_translated( DFBDisplayLayerID id );


void dfb_layer_get_description( const CoreLayer            *layer,
                                DFBDisplayLayerDescription *desc );

CoreScreen *dfb_layer_screen( const CoreLayer *layer );

CardState  *dfb_layer_state( CoreLayer *layer );

DFBDisplayLayerID dfb_layer_id( const CoreLayer *layer );

DFBDisplayLayerID dfb_layer_id_translated( const CoreLayer *layer );

DFBDisplayLayerID dfb_layer_id_translate( DFBDisplayLayerID layer_id );

DFBSurfacePixelFormat dfb_primary_layer_pixelformat( void );

#endif
