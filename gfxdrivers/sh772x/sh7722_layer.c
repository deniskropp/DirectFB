#ifdef SH7722_DEBUG_LAYER
#define DIRECT_ENABLE_DEBUG
#endif


#include <config.h>

#include <stdio.h>

#include <sys/mman.h>

#include <asm/types.h>

#include <directfb.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include "sh7722.h"
#include "sh7722_types.h"
#include "sh7722_layer.h"
#include "sh7722_lcd.h"


D_DEBUG_DOMAIN( SH7722_Layer, "SH772x/Layer", "Renesas SH772x Layers" );

/**********************************************************************************************************************/

static int
sh7722LayerDataSize( void )
{
     return sizeof(SH7722LayerData);
}

static int
sh7722RegionDataSize( void )
{
     return sizeof(SH7722RegionData);
}

static DFBResult
sh7722InitLayer( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 DFBDisplayLayerDescription *description,
                 DFBDisplayLayerConfig      *config,
                 DFBColorAdjustment         *adjustment )
{
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = sdrv->dev;
     SH7722LayerData  *data = layer_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     /* initialize layer data */
     data->layer = SH7722_LAYER_INPUT1 + sdrv->num_inputs++;

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY |
                         DLCAPS_SCREEN_POSITION | DLCAPS_SRC_COLORKEY;

     description->type = DLTF_STILL_PICTURE | DLTF_GRAPHICS | DLTF_VIDEO;

     /* set name */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Input %d", sdrv->num_inputs );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = sdev->lcd_width;
     config->height      = sdev->lcd_height;
     config->pixelformat = DSPF_RGB16;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_ALPHACHANNEL;
     
     /* libshbeu */
     sdev->shbeu_src[data->layer].s.w = sdev->lcd_width;
     sdev->shbeu_src[data->layer].s.h = sdev->lcd_height;
     sdev->shbeu_src[data->layer].s.pitch = sdev->lcd_pitch/DFB_BYTES_PER_PIXEL(config->pixelformat);
     sdev->shbeu_src[data->layer].s.format = REN_RGB565;
     sdev->shbeu_src[data->layer].alpha = 0xff;
     sdev->shbeu_src[data->layer].x = 0;
     sdev->shbeu_src[data->layer].y = 0;


     return DFB_OK;
}

static DFBResult
sh7722TestRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags *failed )
{
     SH7722DriverData           *sdrv = driver_data;
     SH7722DeviceData           *sdev = sdrv->dev;
     SH7722LayerData            *slay = layer_data;
     CoreLayerRegionConfigFlags  fail = 0;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     if (config->options & ~SH7722_LAYER_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_LUT8:
               /* Indexed only for third input */
               if (slay->layer != SH7722_LAYER_INPUT3)
                    fail |= CLRCF_FORMAT;
               break;

          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB24:
          case DSPF_RGB16:
          case DSPF_NV12:
          case DSPF_NV16:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width  < 32 || config->width  > sdev->lcd_width)
          fail |= CLRCF_WIDTH;

     if (config->height < 32 || config->height > sdev->lcd_height)
          fail |= CLRCF_HEIGHT;

     if (config->dest.x >= sdev->lcd_width || config->dest.y >= sdev->lcd_height)
          fail |= CLRCF_DEST;

     if (config->dest.x < 0) {
          config->dest.x = 0;
// FIXME
//          fail |= CLRCF_DEST;
     }

     if (config->dest.y < 0) {
          config->dest.y = 0;
// FIXME
//          fail |= CLRCF_DEST;
     }

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
sh7722AddRegion( CoreLayer             *layer,
                 void                  *driver_data,
                 void                  *layer_data,
                 void                  *region_data,
                 CoreLayerRegionConfig *config )
{
     SH7722RegionData *sreg = region_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     sreg->config = *config;

     D_MAGIC_SET( sreg, SH7722RegionData );

     return DFB_OK;
}

static DFBResult
sh7722SetRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 void                       *region_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags  updated,
                 CoreSurface                *surface,
                 CorePalette                *palette,
                 CoreSurfaceBufferLock      *lock )
{
     int               i, n;
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = sdrv->dev;
     SH7722RegionData *sreg = region_data;
     SH7722LayerData  *slay = layer_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( sreg, SH7722RegionData );

     n = slay->layer - SH7722_LAYER_INPUT1;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 2 );

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* Update position? */
     if (updated & CLRCF_DEST) {
          /* libshbeu: Set horizontal and vertical offset. */
          sdev->shbeu_src[n].x = config->dest.x;
          sdev->shbeu_src[n].y = config->dest.y;
     }

     /* Update size? */
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT)) {
          int cw = config->width;
          int ch = config->height;

          if (config->dest.x + cw > sdev->lcd_width)
               cw = sdev->lcd_width - config->dest.x;

          if (config->dest.y + ch > sdev->lcd_height)
               ch = sdev->lcd_height - config->dest.y;

          /* libshbeu: Set width and height. */
          sdev->shbeu_src[n].s.h = ch;
          sdev->shbeu_src[n].s.w  = cw;

     }

     /* Update surface? */
     if (updated & CLRCF_SURFACE) {
          CoreSurfaceBuffer *buffer = lock->buffer;

          /* libshbeu: Set buffer pitch. */
          sdev->shbeu_src[n].s.pitch = lock->pitch / DFB_BYTES_PER_PIXEL(buffer->format);

          /* libshbeu: Set buffer offset (Y plane or RGB packed). */
          sdev->shbeu_src[n].s.py = lock->addr;
          sdev->shbeu_src[n].s.pc = NULL;
          sdev->shbeu_src[n].s.pa = NULL;

          /* libshbeu: Set alpha plane to same physical address as RGB plane if layer uses alpha */
          if (DFB_PIXELFORMAT_HAS_ALPHA(buffer->format) && (config->options & DLOP_ALPHACHANNEL))
               sdev->shbeu_src[n].s.pa = lock->addr;
          
          /* Set buffer offset (UV plane). */
          if (DFB_PLANAR_PIXELFORMAT(buffer->format)) {
               D_ASSUME( buffer->format == DSPF_NV12 || buffer->format == DSPF_NV16 );

               sdev->shbeu_src[n].s.pc = lock->addr + lock->pitch * surface->config.size.h;
          }

          sreg->surface = surface;
     }

     /* Update format? */
     if (updated & CLRCF_FORMAT) {
          /* Set pixel format. */
          switch (config->format) {
               case DSPF_NV12:
                    sdev->shbeu_src[n].s.format = REN_NV12;
                    break;

               case DSPF_NV16:
                    sdev->shbeu_src[n].s.format = REN_NV16;
                    break;

               case DSPF_ARGB:
               case DSPF_RGB32:
                    sdev->shbeu_src[n].s.format = REN_RGB32;
                    break;

               case DSPF_RGB24:
                    sdev->shbeu_src[n].s.format = REN_BGR24;
                    break;

               case DSPF_RGB16:
                    sdev->shbeu_src[n].s.format = REN_RGB565;
                    break;
 
/* Currently not supported
               case DSPF_LUT8:
                    sdev->shbeu_src[n].s.format = REN_PAL8; //FIXME: implement in libshbeu
                    break;
*/
               default:
                    break;
          }

     }

     /* Update options or opacity? */
     if (updated & (CLRCF_OPTIONS | CLRCF_OPACITY | CLRCF_FORMAT)) {
          /* libshbeu: Set opacity value. */
          sdev->shbeu_src[n].alpha = (config->options & DLOP_OPACITY) ? config->opacity : 0xff;

          /* libshbeu: Enable/disable alpha channel. */
          if ((config->options & DLOP_ALPHACHANNEL) && DFB_PIXELFORMAT_HAS_ALPHA(config->format))
               sdev->shbeu_src[n].s.pa = sdev->shbeu_src[n].s.py;
          else
               sdev->shbeu_src[n].s.pa = 0;
     }

//TODO: Implement CLUT in libshbeu
/*
     // Update CLUT?
     if (updated & CLRCF_PALETTE && palette) {
          const DFBColor *entries = palette->entries;

          for (i=0; i<256; i++) {
               SH7722_SETREG32( sdrv, BCLUT(i), PIXEL_ARGB( entries[i].a,
                                                            entries[i].r,
                                                            entries[i].g,
                                                            entries[i].b ) );
          }
     }
*/

     /* Enable or disable input. */
     if ((config->options & DLOP_OPACITY) && !config->opacity)
          sdev->input_mask &= ~(1 << n);
     else
          sdev->input_mask |= (1 << n);

     fusion_skirmish_dismiss( &sdev->beu_lock );

     sreg->config = *config;

     return DFB_OK;
}

static DFBResult
sh7722RemoveRegion( CoreLayer *layer,
                    void      *driver_data,
                    void      *layer_data,
                    void      *region_data )
{
     int               n;
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = sdrv->dev;
     SH7722LayerData  *slay = layer_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( sdev != NULL );
     D_ASSERT( slay != NULL );

     n = slay->layer - SH7722_LAYER_INPUT1;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 2 );

     fusion_skirmish_prevail( &sdev->beu_lock );


     sdev->input_mask &= ~(1 << n);

     /* libshbeu: reorder src surfaces and start blending. */
     if (sdev->input_mask) {
          struct shbeu_surface * src[3] = {NULL, NULL, NULL};
          int nr_surfaces = 0;
          if (sdev->input_mask & 1) {
               src[nr_surfaces] = &sdev->shbeu_src[0];
               nr_surfaces++;
          }
          if (sdev->input_mask & 2) {
               src[nr_surfaces] = &sdev->shbeu_src[1];
               nr_surfaces++;
          }
          if (sdev->input_mask & 4) {
               src[nr_surfaces] = &sdev->shbeu_src[2];
               nr_surfaces++;
          }
          shbeu_blend(sdrv->shbeu, src[0], src[1], src[2], &sdev->shbeu_dest);
     }

     fusion_skirmish_dismiss( &sdev->beu_lock );

     return DFB_OK;
}

static DFBResult
sh7722FlipRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreSurface           *surface,
                  DFBSurfaceFlipFlags    flags,
                  CoreSurfaceBufferLock *lock )
{
     int                n;
     CoreSurfaceBuffer *buffer;
     SH7722DriverData  *sdrv = driver_data;
     SH7722DeviceData  *sdev = sdrv->dev;
     SH7722LayerData   *slay = layer_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( sdrv != NULL );
     D_ASSERT( sdev != NULL );
     D_ASSERT( slay != NULL );

     n = slay->layer - SH7722_LAYER_INPUT1;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 2 );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* set new physical address for layer */
     sdev->shbeu_src[n].s.py = lock->addr;

     /* libshbeu: reorder src surfaces and start blending. */
     if (sdev->input_mask) {
          struct shbeu_surface * src[3] = {NULL, NULL, NULL};
          int nr_surfaces = 0;
          if (sdev->input_mask & 1) {
               src[nr_surfaces] = &sdev->shbeu_src[0];
               nr_surfaces++;
          }
          if (sdev->input_mask & 2) {
               src[nr_surfaces] = &sdev->shbeu_src[1];
               nr_surfaces++;
          }
          if (sdev->input_mask & 4) {
               src[nr_surfaces] = &sdev->shbeu_src[2];
               nr_surfaces++;
          }
          shbeu_blend(sdrv->shbeu, src[0], src[1], src[2], &sdev->shbeu_dest);
     }

     fusion_skirmish_dismiss( &sdev->beu_lock );
     
     dfb_surface_flip( surface, false );

     return DFB_OK;
}

static DFBResult
sh7722UpdateRegion( CoreLayer             *layer,
                    void                  *driver_data,
                    void                  *layer_data,
                    void                  *region_data,
                    CoreSurface           *surface,
                    const DFBRegion       *update,
                    CoreSurfaceBufferLock *lock )
{
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = sdrv->dev;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( sdrv != NULL );
     D_ASSERT( sdev != NULL );

     /* libshbeu: reorder src surfaces and start blending. */
     if (sdev->input_mask) {
          struct shbeu_surface * src[3] = {NULL, NULL, NULL};
          int nr_surfaces = 0;
          if (sdev->input_mask & 1) {
               src[nr_surfaces] = &sdev->shbeu_src[0];
               nr_surfaces++;
          }
          if (sdev->input_mask & 2) {
               src[nr_surfaces] = &sdev->shbeu_src[1];
               nr_surfaces++;
          }
          if (sdev->input_mask & 4) {
               src[nr_surfaces] = &sdev->shbeu_src[2];
               nr_surfaces++;
          }
          shbeu_blend(sdrv->shbeu, src[0], src[1], src[2], &sdev->shbeu_dest);
     }

     return DFB_OK;
}

DisplayLayerFuncs sh7722LayerFuncs = {
     .LayerDataSize  = sh7722LayerDataSize,
     .RegionDataSize = sh7722RegionDataSize,
     .InitLayer      = sh7722InitLayer,

     .TestRegion     = sh7722TestRegion,
     .AddRegion      = sh7722AddRegion,
     .SetRegion      = sh7722SetRegion,
     .RemoveRegion   = sh7722RemoveRegion,
     .FlipRegion     = sh7722FlipRegion,
     .UpdateRegion   = sh7722UpdateRegion,
};

