// Shared header file for uc_hwmap.c and uc_hwset.c.

#ifndef __UC_HW_H__
#define __UC_HW_H__

#include "unichrome.h"
#include "uc_fifo.h"

// GPU - mapping functions (uc_hwmap.c)

int uc_map_dst_format(DFBSurfacePixelFormat format,
                      __u32* colormask, __u32* alphamask);
int uc_map_src_format_3d(DFBSurfacePixelFormat format);
void uc_map_blending_fn(struct uc_hw_alpha* hwalpha,
                        DFBSurfaceBlendFunction sblend,
                        DFBSurfaceBlendFunction dblend,
                        DFBSurfacePixelFormat format);
void uc_map_blitflags(struct uc_hw_texture* tex,
                      DFBSurfaceBlittingFlags bflags,
                      DFBSurfacePixelFormat sformat);

// GPU - setting functions (uc_hwset.c)

void uc_set_blending_fn( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state);

void uc_set_texenv     ( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_clip       ( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_destination( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_source_2d  ( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_source_3d  ( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_color_2d   ( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

void uc_set_colorkey_2d( UcDriverData *ucdrv,
                         UcDeviceData *ucdev,
                         CardState    *state );

#endif // __UC_HW_H__
