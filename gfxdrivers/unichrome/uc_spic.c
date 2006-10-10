/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include <config.h>

#include "unichrome.h"
#include "vidregs.h"
#include "mmio.h"

#include <core/system.h>
#include <core/palette.h>

#define UC_SPIC_OPTIONS DLOP_OPACITY

typedef struct _UcSubpictureData {
} UcSubpictureData;

static int uc_spic_datasize()
{
    return sizeof(UcSubpictureData);
}

static void
uc_spic_set_palette( volatile __u8* hwregs, CorePalette *palette )
{
    int i;

    if (palette) {
        for (i = 0; i < 16; i++) {
            /* TODO: Check r-g-b order. */
            VIDEO_OUT(hwregs, RAM_TABLE_CONTROL,
                (palette->entries[i].r << 24) |
                (palette->entries[i].g << 16) |
                (palette->entries[i].b <<  8) |
                (i << 4) | RAM_TABLE_RGB_ENABLE);
        }
    }
}

static void uc_spic_enable( volatile __u8 *hwregs, bool enable )
{
    VIDEO_OUT(hwregs, SUBP_CONTROL_STRIDE,
        (VIDEO_IN(hwregs, SUBP_CONTROL_STRIDE) & ~SUBP_HQV_ENABLE) |
        (enable ? SUBP_HQV_ENABLE : 0));
}

static void
uc_spic_set_buffer( volatile __u8 *hwregs, CoreSurface *surface )
{
    if (surface) {
        VIDEO_OUT(hwregs, SUBP_STARTADDR,
            surface->front_buffer->video.offset);
        VIDEO_OUT(hwregs, SUBP_CONTROL_STRIDE,
            (VIDEO_IN(hwregs, SUBP_CONTROL_STRIDE) & ~SUBP_STRIDE_MASK) |
            (surface->front_buffer->video.pitch & SUBP_STRIDE_MASK) |
            SUBP_AI44 );
    }
}

static DFBResult
uc_spic_init_layer( CoreLayer                   *layer,
                    void                        *driver_data,
                    void                        *layer_data,
                    DFBDisplayLayerDescription  *description,
                    DFBDisplayLayerConfig       *config,
                    DFBColorAdjustment          *adjustment )
{
    /* Set layer type, capabilities and name */

    description->caps = DLCAPS_SURFACE | DLCAPS_OPACITY;
    description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
    snprintf(description->name,
        DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VIA Unichrome DVD Subpicture");

    adjustment->flags = DCAF_NONE;

    /* Fill out the default configuration */

    config->flags  = DLCONF_WIDTH | DLCONF_HEIGHT |
                     DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;

    config->width  = 720;
    config->height = 576;

    config->pixelformat = DSPF_ALUT44;
    config->buffermode  = DLBM_FRONTONLY;
    config->options     = DLOP_NONE;

    return DFB_OK;
}

static DFBResult
uc_spic_test_region( CoreLayer                  *layer,
                     void                       *driver_data,
                     void                       *layer_data,
                     CoreLayerRegionConfig      *config,
                     CoreLayerRegionConfigFlags *failed)
{
    CoreLayerRegionConfigFlags fail = 0;

    /* Check layer options */

    if (config->options & ~UC_SPIC_OPTIONS)
        fail |= CLRCF_OPTIONS;

    /* Check pixelformats */

    switch (config->format) {
          case DSPF_ALUT44:
              break;
          //case DSPF_LUTA44:
              // IA44 does not exist in DirectFB, but hw supports it.
          default:
              fail |= CLRCF_FORMAT;
    }

    /* Check width and height */

    if (config->width > 8195 || config->width < 1)
        fail |= CLRCF_WIDTH;

    if (config->height > 4096 || config->height < 1)
        fail |= CLRCF_HEIGHT;

    if (failed) *failed = fail;
    if (fail) return DFB_UNSUPPORTED;

    return DFB_OK;
}

static DFBResult
uc_spic_set_region( CoreLayer                  *layer,
                    void                       *driver_data,
                    void                       *layer_data,
                    void                       *region_data,
                    CoreLayerRegionConfig      *config,
                    CoreLayerRegionConfigFlags  updated,
                    CoreSurface                *surface,
                    CorePalette                *palette )
{
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;

    uc_spic_set_palette(ucdrv->hwregs, palette);
    uc_spic_set_buffer(ucdrv->hwregs, surface);
    uc_spic_enable(ucdrv->hwregs, (config->opacity > 0));

    return DFB_OK;
}

static DFBResult
uc_spic_remove( CoreLayer *layer,
                void      *driver_data,
                void      *layer_data,
                void      *region_data )
{
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;

    uc_spic_enable(ucdrv->hwregs, false);
    return DFB_OK;
}

static DFBResult
uc_spic_flip_region( CoreLayer           *layer,
                     void                *driver_data,
                     void                *layer_data,
                     void                *region_data,
                     CoreSurface         *surface,
                     DFBSurfaceFlipFlags  flags )
{
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;

    dfb_surface_flip_buffers(surface, false);
    uc_spic_set_buffer(ucdrv->hwregs, surface);

    return DFB_OK;
}

DisplayLayerFuncs ucSubpictureFuncs = {
    LayerDataSize:      uc_spic_datasize,
    InitLayer:          uc_spic_init_layer,
    SetRegion:          uc_spic_set_region,
    RemoveRegion:       uc_spic_remove,
    TestRegion:         uc_spic_test_region,
    FlipRegion:         uc_spic_flip_region,
};
