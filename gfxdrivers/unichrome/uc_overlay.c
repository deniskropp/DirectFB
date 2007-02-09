/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include <config.h>

#include <fbdev/fbdev.h>

#include "unichrome.h"
#include "uc_overlay.h"
#include "uc_ioctl.h"
#include "vidregs.h"
#include "mmio.h"

#include <stdlib.h>
#include <unistd.h>

#include <direct/messages.h>

#include <core/system.h>

#include <misc/conf.h>

// Forward declaration
static DFBResult
uc_ovl_remove(CoreLayer *layer,
              void      *driver_data,
              void      *layer_data,
              void      *region_data);


static int uc_ovl_datasize()
{
    return sizeof(UcOverlayData);
}


static DFBResult
uc_ovl_init_layer( CoreLayer                   *layer,
                   void                        *driver_data,
                   void                        *layer_data,
                   DFBDisplayLayerDescription  *description,
                   DFBDisplayLayerConfig       *config,
                   DFBColorAdjustment          *adjustment )
{
    UcDriverData* ucdrv = (UcDriverData*) driver_data;
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;

    // Save a pointer to the layer data for access by the primary
    // This is needed to properly support levels and the primary alpha channel

    ucdrv->ovl = ucovl;

    // Set layer type, capabilities and name

    description->caps = UC_OVL_CAPS;
    description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
    snprintf(description->name,
        DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VIA Unichrome Video");

    adjustment->flags = DCAF_NONE;

    // Fill out the default configuration

    config->flags  = DLCONF_WIDTH | DLCONF_HEIGHT |
                     DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;

    ucovl->v1.win.w = 720;
    ucovl->v1.win.h = 576;
    ucovl->v1.win.x = 0;
    ucovl->v1.win.y = 0;

    config->width  = 720;
    config->height = 576;

    config->pixelformat = DSPF_YV12;
    config->buffermode  = DLBM_FRONTONLY;
    config->options     = DLOP_NONE;

    // Reset overlay

    ucovl->extfifo_on = false;
    ucovl->hwrev = ucdrv->hwrev;
    ucovl->scrwidth = ucovl->v1.win.w;

    ucovl->v1.isenabled = false;
    ucovl->v1.cfg = *config;
    ucovl->v1.ox = 0;
    ucovl->v1.oy = 0;
    ucovl->v1.dst_key.a = 0;
    ucovl->v1.dst_key.r = 0;
    ucovl->v1.dst_key.g = 0;
    ucovl->v1.dst_key.b = 0;
    ucovl->v1.dstkey_enabled = false;
    ucovl->v1.opacity = 0xff;
    ucovl->v1.level = 1;

//    adjustment->flags = DCAF_BRIGHTNESS | DCAF_CONTRAST |
//        DCAF_HUE | DCAF_SATURATION;
    adjustment->brightness = 0x8000;
    adjustment->contrast = 0x8000;
    adjustment->saturation = 0x8000;
    adjustment->hue = 0x8000;
    ucovl->v1.adj = *adjustment;

    uc_ovl_remove(layer, driver_data, layer_data, NULL);

    return DFB_OK;
}


static DFBResult
uc_ovl_set_region( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   void                       *region_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags  updated,
                   CoreSurface                *surface,
                   CorePalette                *palette )
{
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    DFBRectangle   win;
    
    /* remember configuration */
    ucovl->config = *config;

    /* get new destination rectangle */
    win = config->dest;

    // Bounds checking
    if ((win.x < -8192) || (win.x > 8192) ||
        (win.y < -8192) || (win.y > 8192) ||
        (win.w < 32) || (win.w > 4096) ||
        (win.h < 32) || (win.h > 4096))
    {
        D_DEBUG("Layer size or position is out of bounds.");
        return DFB_INVAREA;
    }

    ucovl->v1.isenabled = true;
    ucovl->v1.win = win;
    ucovl->v1.dst_key = config->dst_key;
    ucovl->v1.dstkey_enabled = config->options & DLOP_DST_COLORKEY;
    
    if (config->options & DLOP_OPACITY)
        ucovl->v1.opacity = config->opacity;
    else
        ucovl->v1.opacity = 0xff;

    // printf("uc_overlay: color-keying is %s\n",
    //     ucovl->v1.dstkey_enabled ? "enabled" : "disabled");

    ucovl->deinterlace = config->options & DLOP_DEINTERLACING;
    ucovl->surface     = surface;

    if (ucdrv->canfliponvsync) {
        FBDev *dfb_fbdev = dfb_system_data();
        int field_option = VIAFB_WAIT_FLIP; // wait for any pending flip
        ioctl(dfb_fbdev->fd, FBIO_WAITFORVSYNC, &field_option);
    }

    return uc_ovl_update(ucdrv, ucovl, UC_OVL_CHANGE, surface);
}


static DFBResult
uc_ovl_remove(CoreLayer *layer,
              void      *driver_data,
              void      *layer_data,
              void      *region_data)
{
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    volatile u8*   vio = ucdrv->hwregs;

    ucovl->v1.isenabled = false;

    uc_ovl_vcmd_wait(vio);

    VIDEO_OUT(vio, V_FIFO_CONTROL, UC_MAP_V1_FIFO_CONTROL(16,12,8));
    //  VIDEO_OUT(vio, ALPHA_V3_FIFO_CONTROL, 0x0407181f);

    if (ucovl->hwrev >= 0x10) {
        VIDEO_OUT(vio, V1_ColorSpaceReg_1, ColorSpaceValue_1_3123C0);
        VIDEO_OUT(vio, V1_ColorSpaceReg_2, ColorSpaceValue_2_3123C0);
    }
    else {
        VIDEO_OUT(vio, V1_ColorSpaceReg_1, ColorSpaceValue_1);
        VIDEO_OUT(vio, V1_ColorSpaceReg_2, ColorSpaceValue_2);
    }

    VIDEO_OUT(vio, HQV_CONTROL, VIDEO_IN(vio, HQV_CONTROL) & ~HQV_ENABLE);
    VIDEO_OUT(vio, V1_CONTROL, VIDEO_IN(vio, V1_CONTROL) & ~V1_ENABLE);
    //  VIDEO_OUT(vio, V3_CONTROL, VIDEO_IN(vio, V3_CONTROL) & ~V3_ENABLE);

    VIDEO_OUT(vio, V_COMPOSE_MODE,
        (VIDEO_IN(vio, V_COMPOSE_MODE) & ~ENABLE_COLOR_KEYING) | V1_COMMAND_FIRE);

    ucovl->surface = NULL;

    return DFB_OK;
}


static DFBResult
uc_ovl_test_region(CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed)
{
    CoreLayerRegionConfigFlags fail = 0;

    // Check layer options

    if (config->options & ~UC_OVL_OPTIONS)
        fail |= CLRCF_OPTIONS;

    // Check pixelformats

    switch (config->format) {
          case DSPF_YUY2:
              break;
          case DSPF_UYVY:
              fail |= CLRCF_FORMAT;   // Nope...  doesn't work.
              break;
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
              break;
          default:
              fail |= CLRCF_FORMAT;
    }

    // Check width and height

    if (config->width > 4096 || config->width < 32)
        fail |= CLRCF_WIDTH;

    if (config->height > 4096 || config->height < 32)
        fail |= CLRCF_HEIGHT;

    if (failed) *failed = fail;
    if (fail) return DFB_UNSUPPORTED;

    return DFB_OK;
}


static DFBResult
uc_ovl_flip_region( CoreLayer           *layer,
                    void                *driver_data,
                    void                *layer_data,
                    void                *region_data,
                    CoreSurface         *surface,
                    DFBSurfaceFlipFlags  flags )
{
    //printf("Entering %s ... \n", __PRETTY_FUNCTION__);

    UcDriverData*  ucdrv = (UcDriverData*) driver_data;
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    DFBResult    ret;
    FBDev *dfb_fbdev = dfb_system_data();

    dfb_surface_flip_buffers(surface, false);

    ucovl->field = 0;

    if (ucdrv->canfliponvsync)
    {
        if (ucovl->config.options & DLOP_FIELD_PARITY)
        {
            struct fb_flip flip;
            int field_option;

            field_option = VIAFB_WAIT_FLIP; // ensure last pending flip complete
            ioctl(dfb_fbdev->fd, FBIO_WAITFORVSYNC, &field_option);

            flip.device = VIAFB_FLIP_V1;
            flip.field = ucovl->config.parity;
            flip.count = 0; // until we implement this

            uc_ovl_map_buffer(surface->format,
                surface->front_buffer->video.offset,
                ucovl->v1.ox, ucovl->v1.oy, surface->width, surface->height,
                surface->front_buffer->video.pitch, 0,
                &flip.offset[0], &flip.offset[1], &flip.offset[2]);

            ioctl(dfb_fbdev->fd, FBIO_FLIPONVSYNC, &flip);
        }
        else
        {
            ret = uc_ovl_update(ucdrv, ucovl, UC_OVL_FLIP, surface);
            if (ret)
                return ret;
        }
    }
    else
    {
        if (ucovl->config.options & DLOP_FIELD_PARITY)
        {
            int field_option;
        
            if (ucovl->config.parity == 0)  // top field first?
                field_option = VIAFB_WAIT_BOTTOMFIELD;
            else
                field_option = VIAFB_WAIT_TOPFIELD;
            ioctl(dfb_fbdev->fd, FBIO_WAITFORVSYNC, &field_option);
            // that actually waits for VBLANK so we need a further delay
            // to be sure the field has started and that the flip will
            // take effect on the next field
            usleep(2500);
        }
    
        ret = uc_ovl_update(ucdrv, ucovl, UC_OVL_FLIP, surface);
        if (ret)
            return ret;
    }

    if (flags & DSFLIP_WAIT)
        dfb_layer_wait_vsync(layer);

    return DFB_OK;
}

static DFBResult
uc_ovl_get_level(CoreLayer    *layer,
                 void         *driver_data,
                 void         *layer_data,
                 int          *level)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    *level = ucovl->v1.level;
    return DFB_OK;
}

static DFBResult
uc_ovl_set_level(CoreLayer    *layer,
                 void         *driver_data,
                 void         *layer_data,
                 int          level)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    UcDriverData*  ucdrv = (UcDriverData*) driver_data;

    if (level == 0) return DFB_INVARG;
    
    if (level < 0) {
        // Enable underlay mode.
        VIDEO_OUT(ucdrv->hwregs, V_ALPHA_CONTROL,
            uc_ovl_map_alpha(ucovl->opacity_primary));
    }
    else {
        // Enable overlay mode (default)
        VIDEO_OUT(ucdrv->hwregs, V_ALPHA_CONTROL,
            uc_ovl_map_alpha(ucovl->v1.opacity));
    }
    VIDEO_OUT(ucdrv->hwregs, V_COMPOSE_MODE, V1_COMMAND_FIRE |
        (ucovl->v1.dstkey_enabled ? ENABLE_COLOR_KEYING : 0));

    ucovl->v1.level = level;
    return DFB_OK;
}

static DFBResult
uc_ovl_set_input_field( CoreLayer *layer,
                        void      *driver_data,
                        void      *layer_data,
                        void      *region_data,
                        int        field )
{
     UcOverlayData* ucovl = (UcOverlayData*) layer_data;
     UcDriverData*  ucdrv = (UcDriverData*) driver_data;

     ucovl->field = field;

     return uc_ovl_update(ucdrv, ucovl, UC_OVL_FIELD, ucovl->surface);
}

DisplayLayerFuncs ucOverlayFuncs = {
    LayerDataSize:      uc_ovl_datasize,
    InitLayer:          uc_ovl_init_layer,
    SetRegion:          uc_ovl_set_region,
    RemoveRegion:       uc_ovl_remove,
    TestRegion:         uc_ovl_test_region,
    FlipRegion:         uc_ovl_flip_region,
    GetLevel:           uc_ovl_get_level,
    SetLevel:           uc_ovl_set_level,
    SetInputField:      uc_ovl_set_input_field,
//    SetColorAdjustment: uc_ovl_set_adjustment
};
