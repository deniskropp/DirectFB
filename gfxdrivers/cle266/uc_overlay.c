/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "unichrome.h"
#include "uc_overlay.h"
#include "vidregs.h"
#include "mmio.h"


// Forward declaration
static DFBResult
uc_ovl_disable(CoreLayer *layer,
               void      *driver_data,
               void      *layer_data);


static int uc_ovl_datasize()
{
    return sizeof(UcOverlayData);
}


static DFBResult
uc_ovl_init_layer(GraphicsDevice        *device,
                  CoreLayer             *layer,
                  DisplayLayerInfo      *layer_info,
                  DFBDisplayLayerConfig *default_config,
                  DFBColorAdjustment    *default_adj,
                  void                  *driver_data,
                  void                  *layer_data)
{
    UcDriverData* ucdrv = (UcDriverData*) driver_data;
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;

    // Set layer type, capabilities and name

    layer_info->desc.caps = UC_OVL_CAPS;
    layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
    snprintf(layer_info->desc.name,
        DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VIA CLE266 Overlay");

    default_adj->flags = DCAF_NONE;

    // Fill out the default configuration

    dfb_primary_layer_rectangle(0.0f, 0.0f, 1.0f, 1.0f, &(ucovl->v1.win));

    default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
        DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;

    default_config->width = ucovl->v1.win.w;
    default_config->height = ucovl->v1.win.h;

    default_config->pixelformat = DSPF_YUY2;
    default_config->buffermode  = DLBM_FRONTONLY;
    default_config->options     = DLOP_NONE;

    // Reset overlay

    ucovl->extfifo_on = false;
    ucovl->hwrev = ucdrv->hwrev;
    ucovl->scrwidth = ucovl->v1.win.w;
    ucovl->hwregs = ucdrv->hwregs;

    ucovl->v1.isenabled = false;
    ucovl->v1.cfg = *default_config;
    ucovl->v1.ox = 0;
    ucovl->v1.oy = 0;
    ucovl->v1.opacity = 255;
    ucovl->v1.level = 1;

    uc_ovl_disable(layer, driver_data, layer_data);

    return DFB_OK;
}


static DFBResult
uc_ovl_enable(CoreLayer *layer,
              void      *driver_data,
              void      *layer_data)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;

    ucovl->v1.isenabled = true;
    return uc_ovl_update(ucovl, UC_OVL_CHANGE, dfb_layer_surface(layer));
}


static DFBResult
uc_ovl_disable(CoreLayer *layer,
               void      *driver_data,
               void      *layer_data)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    __u8* vio = ucovl->hwregs;

    ucovl->v1.isenabled = false;

    uc_ovl_vcmd_wait(vio);

    //VIDEO_OUT(vio, V_ALPHA_CONTROL, 0);
    VIDEO_OUT(ucovl->hwregs, V_ALPHA_CONTROL, uc_ovl_map_alpha(255));

    VIDEO_OUT(vio, V_FIFO_CONTROL, UC_MAP_V1_FIFO_CONTROL(16,12,8));
    //  VIDEO_OUT(vio, ALPHA_V3_FIFO_CONTROL, 0x0407181f);

    if (ucovl->hwrev == 0x10) {
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
        VIDEO_IN(vio, V_COMPOSE_MODE) | V1_COMMAND_FIRE);

    return DFB_OK;
}


static DFBResult
uc_ovl_test_configuration(CoreLayer                  *layer,
                          void                       *driver_data,
                          void                       *layer_data,
                          DFBDisplayLayerConfig      *config,
                          DFBDisplayLayerConfigFlags *failed)
{
    DFBDisplayLayerConfigFlags fail = 0;

    // Check layer options

    if (config->options & ~UC_OVL_CAPS)
        fail |= DLCONF_OPTIONS;

    // Check pixelformats

    switch (config->pixelformat) {
          case DSPF_YUY2:
              break;
          case DSPF_UYVY:
              fail |= DLCONF_PIXELFORMAT;   // Nope...  doesn't work.
              break;
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
              break;
          default:
              fail |= DLCONF_PIXELFORMAT;
    }

    // Check width and height

    if (config->width > 4096 || config->width < 32)
        fail |= DLCONF_WIDTH;

    if (config->height > 4096 || config->height < 32)
        fail |= DLCONF_HEIGHT;

    // Check buffer mode

    if (config->flags & DLCONF_BUFFERMODE) {
        if (config->buffermode & DLBM_BACKSYSTEM)
            fail |= DLCONF_BUFFERMODE;
    }

    if (failed) *failed = fail;
    if (fail) return DFB_UNSUPPORTED;

    return DFB_OK;
}


static DFBResult
uc_ovl_set_configuration(CoreLayer             *layer,
                         void                  *driver_data,
                         void                  *layer_data,
                         DFBDisplayLayerConfig *config )
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;

    ucovl->v1.cfg = *config;

    return uc_ovl_update(ucovl, UC_OVL_CHANGE, dfb_layer_surface(layer));
}


static DFBResult
uc_ovl_set_opacity(CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   __u8       opacity)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    ucovl->v1.opacity = opacity;
    VIDEO_OUT(ucovl->hwregs, V_ALPHA_CONTROL, uc_ovl_map_alpha(opacity));
    return DFB_OK;
}


static DFBResult
uc_ovl_set_location(CoreLayer *layer,
                    void      *driver_data,
                    void      *layer_data,
                    float      x,
                    float      y,
                    float      width,
                    float      height)
{
    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    DFBRectangle win;

    /* get new destination rectangle */
    dfb_primary_layer_rectangle(x, y, width, height, &win);

    // Bounds checking
    if ((win.x < -8192) || (win.x > 8192) ||
        (win.y < -8192) || (win.y > 8192) ||
        (win.w < 32) || (win.w > 4096) ||
        (win.h < 32) || (win.h > 4096))
    {
        DEBUGMSG("Layer size or position is out of bounds.");
        return DFB_INVAREA;
    }

    ucovl->v1.win = win;

    return uc_ovl_update(ucovl, UC_OVL_CHANGE, dfb_layer_surface(layer));
}


static DFBResult
uc_ovl_set_dst_color_key(CoreLayer *layer,
                         void      *driver_data,
                         void      *layer_data,
                         __u8       r,
                         __u8       g,
                         __u8       b)
{
    return DFB_UNIMPLEMENTED;
}


static DFBResult
uc_ovl_flip_buffers(CoreLayer           *layer,
                    void                *driver_data,
                    void                *layer_data,
                    DFBSurfaceFlipFlags  flags)
{
    //printf("Entering %s ... \n", __PRETTY_FUNCTION__);

    UcOverlayData* ucovl = (UcOverlayData*) layer_data;
    CoreSurface *surface = dfb_layer_surface(layer);
    DFBResult    ret;

    if (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) &&
        !dfb_config->pollvsync_after)
        dfb_layer_wait_vsync( layer );

    dfb_surface_flip_buffers(surface);

    ret = uc_ovl_update(ucovl, UC_OVL_FLIP, dfb_layer_surface(layer));
    if (ret)
        return ret;

    if ((flags & DSFLIP_WAIT) &&
        (dfb_config->pollvsync_after || !(flags & DSFLIP_ONSYNC)))
        dfb_layer_wait_vsync(layer);

    return DFB_OK;
}

static DFBResult
uc_ovl_wait_vsync(CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data)
{
    // Forward the function call to the primary layer.
    return dfb_layer_wait_vsync(dfb_layer_at(DLID_PRIMARY));
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

    if (level == 0) return DFB_INVARG;
    if (level < 0) {
        // Enable underlay mode.
        VIDEO_OUT(ucovl->hwregs, V_ALPHA_CONTROL, uc_ovl_map_alpha(-1));
    }
    else {
        // Enable overlay mode (default)
        VIDEO_OUT(ucovl->hwregs, V_ALPHA_CONTROL,
            uc_ovl_map_alpha(ucovl->v1.opacity));
    }

    ucovl->v1.level = level;
    return DFB_OK;
}

DisplayLayerFuncs ucOverlayFuncs = {
    LayerDataSize:      uc_ovl_datasize,
    InitLayer:          uc_ovl_init_layer,
    Enable:             uc_ovl_enable,
    Disable:            uc_ovl_disable,
    TestConfiguration:  uc_ovl_test_configuration,
    SetConfiguration:   uc_ovl_set_configuration,
    SetOpacity:         uc_ovl_set_opacity,
    SetScreenLocation:  uc_ovl_set_location,
    SetDstColorKey:     uc_ovl_set_dst_color_key,
    FlipBuffers:        uc_ovl_flip_buffers,
    WaitVSync:          uc_ovl_wait_vsync,
    GetLevel:           uc_ovl_get_level,
    SetLevel:           uc_ovl_set_level,
};
