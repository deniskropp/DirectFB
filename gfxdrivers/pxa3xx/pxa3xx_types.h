#ifndef __PXA3XX__TYPES_H__
#define __PXA3XX__TYPES_H__

#include <pxa3xx-gcu.h>


#define PXA3XX_GFX_MAX_PREPARE             8192


typedef volatile struct pxa3xx_gcu_shared PXA3XXGfxSharedArea;


typedef struct {
     /* state validation */
     int                      v_flags;

     /* cached values */
     unsigned long            dst_phys;
     int                      dst_pitch;
     int                      dst_bpp;
     int                      dst_index;

     unsigned long            src_phys;
     int                      src_pitch;
     int                      src_bpp;
     int                      src_index;

     unsigned long            mask_phys;
     int                      mask_pitch;
     DFBSurfacePixelFormat    mask_format;
     int                      mask_index;
     DFBPoint                 mask_offset;
     DFBSurfaceMaskFlags      mask_flags;

     DFBSurfaceDrawingFlags   dflags;
     DFBSurfaceBlittingFlags  bflags;
     DFBSurfaceRenderOptions  render_options;

     DFBColor                 color;
} PXA3XXDeviceData;


typedef struct {
     PXA3XXDeviceData        *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;

     int                      gfx_fd;
     PXA3XXGfxSharedArea     *gfx_shared;

     int                      prep_num;
     __u32                    prep_buf[PXA3XX_GFX_MAX_PREPARE];

     volatile void           *mmio_base;
} PXA3XXDriverData;

#endif

