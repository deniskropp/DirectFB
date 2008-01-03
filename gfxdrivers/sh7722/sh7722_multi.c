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
#include "sh7722_multi.h"


D_DEBUG_DOMAIN( SH7722_Layer, "SH7722/Layer", "Renesas SH7722 Layers" );

/**********************************************************************************************************************/

static int
sh7722LayerDataSize()
{
     return sizeof(SH7722MultiLayerData);
}

static int
sh7722RegionDataSize()
{
     return sizeof(SH7722MultiRegionData);
}

static DFBResult
sh7722InitLayer( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 DFBDisplayLayerDescription *description,
                 DFBDisplayLayerConfig      *config,
                 DFBColorAdjustment         *adjustment )
{
     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     /* set capabilities and type */
     description->caps    = DLCAPS_SURFACE | DLCAPS_SCREEN_POSITION | DLCAPS_SRC_COLORKEY | DLCAPS_WINDOWS;
     description->type    = DLTF_GRAPHICS;
     description->regions = 4;

     /* set name */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Multi Window" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = SH7722_LCD_WIDTH;
     config->height      = SH7722_LCD_HEIGHT;
     config->pixelformat = DSPF_NV16;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     return DFB_OK;
}

static DFBResult
sh7722TestRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     if (config->options & ~SH7722_MULTI_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_NV12:
          case DSPF_NV16:
          /*case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB24:
          case DSPF_RGB16:*/
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width  < 32 || config->width  > 1280)
          fail |= CLRCF_WIDTH;

     if (config->height < 32 || config->height > 1024)
          fail |= CLRCF_HEIGHT;


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
     int                    n;
     SH7722MultiRegionData *sreg = region_data;
     SH7722MultiLayerData  *slay = layer_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     if (slay->added == 0xF)
          return DFB_LIMITEXCEEDED;

     for (n=0; n<4; n++)
          if (! (slay->added & (1 << n)))
               break;

     D_ASSERT( n < 4 );

     sreg->config = *config;


     slay->added |= 1 << n;

     D_MAGIC_SET( sreg, SH7722MultiRegionData );

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
     int                    n;
     SH7722DriverData      *sdrv = driver_data;
     SH7722DeviceData      *sdev = sdrv->dev;
     SH7722MultiRegionData *sreg = region_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( sreg, SH7722MultiRegionData );

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     n = sreg->index;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 3 );

     /* Update position? */
     if (updated & CLRCF_DEST) {
          /* Set horizontal and vertical offset. */
          SH7722_SETREG32( sdrv, BMLOCR(n), (config->dest.y << 16) | config->dest.x );
     }

     /* Update size? */
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT)) {
          /* Set width and height. */
          SH7722_SETREG32( sdrv, BMSSZR(n), (config->height << 16) | config->width );
     }

     /* Update surface? */
     if (updated & CLRCF_SURFACE) {
          CoreSurfaceBuffer *buffer;

          D_ASSERT( surface != NULL );

          buffer = lock->buffer;

          D_ASSERT( buffer != NULL );

          /* Set buffer pitch. */
          SH7722_SETREG32( sdrv, BMSMWR(n), lock->pitch );

          /* Set buffer offset (Y plane or RGB packed). */
          SH7722_SETREG32( sdrv, BMSAYR(n), lock->phys );

          /* Set buffer offset (UV plane). */
          if (DFB_PLANAR_PIXELFORMAT(buffer->format)) {
               D_ASSUME( buffer->format == DSPF_NV12 || buffer->format == DSPF_NV16 );

               SH7722_SETREG32( sdrv, BMSACR(n), lock->phys + lock->pitch * surface->config.size.h );
          }
     }

     /* Update format? */
     if (updated & CLRCF_FORMAT) {
          unsigned long tBMSIFR = 0;

          /* Set pixel format. */
          switch (config->format) {
               case DSPF_NV12:
                    tBMSIFR |= CHRR_YCBCR_420;
                    break;

               case DSPF_NV16:
                    tBMSIFR |= CHRR_YCBCR_422;
                    break;

               case DSPF_ARGB:
                    tBMSIFR |= RPKF_ARGB;
                    break;

               case DSPF_RGB32:
                    tBMSIFR |= RPKF_RGB32;
                    break;

               case DSPF_RGB24:
                    tBMSIFR |= RPKF_RGB24;
                    break;

               case DSPF_RGB16:
                    tBMSIFR |= RPKF_RGB16;
                    break;

               default:
                    break;
          }

          /* FIXME: all regions need to have the same format! */
          SH7722_SETREG32( sdrv, BMSIFR, tBMSIFR );
     }

     SH7722_SETREG32( sdrv, BMWCR0, SH7722_GETREG32( sdrv, BMWCR0 ) | (1 << n) );

     fusion_skirmish_dismiss( &sdev->beu_lock );

     return DFB_OK;
}

static DFBResult
sh7722RemoveRegion( CoreLayer             *layer,
                    void                  *driver_data,
                    void                  *layer_data,
                    void                  *region_data )
{
     int                    n;
     SH7722DriverData      *sdrv = driver_data;
     SH7722DeviceData      *sdev = sdrv->dev;
     SH7722MultiRegionData *sreg = region_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( sreg, SH7722MultiRegionData );

     n = sreg->index;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 3 );

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Disable multi window. */
     SH7722_SETREG32( sdrv, BMWCR0, SH7722_GETREG32( sdrv, BMWCR0 ) & ~(1 << n) );

     /* Start operation! */
     SH7722_SETREG32( sdrv, BESTR, (sdev->input_mask << 8) | 1 );

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
     int                    n;
     CoreSurfaceBuffer     *buffer;
     SH7722DriverData      *sdrv = driver_data;
     SH7722DeviceData      *sdev = sdrv->dev;
     SH7722MultiRegionData *sreg = region_data;

     D_DEBUG_AT( SH7722_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( sdrv != NULL );
     D_ASSERT( sdev != NULL );
     D_MAGIC_ASSERT( sreg, SH7722MultiRegionData );

     n = sreg->index;

     D_ASSERT( n >= 0 );
     D_ASSERT( n <= 3 );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Set buffer pitch. */
     SH7722_SETREG32( sdrv, BMSMWR(n), lock->pitch );

     /* Set buffer offset (Y plane or RGB packed). */
     SH7722_SETREG32( sdrv, BMSAYR(n), lock->phys );

     /* Set buffer offset (UV plane). */
     if (DFB_PLANAR_PIXELFORMAT(buffer->format)) {
          D_ASSUME( buffer->format == DSPF_NV12 || buffer->format == DSPF_NV16 );

          SH7722_SETREG32( sdrv, BMSACR(n), lock->phys + lock->pitch * surface->config.size.h );
     }

     /* Start operation! */
     SH7722_SETREG32( sdrv, BESTR, (sdev->input_mask << 8) | 1 );

     if (flags & DSFLIP_WAIT) {
          /* Wait for idle BEU. */
          while (SH7722_GETREG32( sdrv, BSTAR ) & 1);
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

     fusion_skirmish_prevail( &sdev->beu_lock );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Start operation! */
     SH7722_SETREG32( sdrv, BESTR, (sdev->input_mask << 8) | 1 );

     fusion_skirmish_dismiss( &sdev->beu_lock );

     return DFB_OK;
}

DisplayLayerFuncs sh7722MultiLayerFuncs = {
     LayerDataSize:      sh7722LayerDataSize,
     RegionDataSize:     sh7722RegionDataSize,
     InitLayer:          sh7722InitLayer,

     TestRegion:         sh7722TestRegion,
     AddRegion:          sh7722AddRegion,
     SetRegion:          sh7722SetRegion,
     RemoveRegion:       sh7722RemoveRegion,
     FlipRegion:         sh7722FlipRegion,
     UpdateRegion:       sh7722UpdateRegion
};

