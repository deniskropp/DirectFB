#ifndef __SH7722__TYPES_H__
#define __SH7722__TYPES_H__

#include <core/layers.h>

#include <sh7722gfx.h>


#define SH7722GFX_MAX_PREPARE      8192
#define SH7722GFX_JPEG_RELOAD_SIZE (384*1024)


typedef enum {
     SH7722_LAYER_INPUT1,
     SH7722_LAYER_INPUT2,
     SH7722_LAYER_INPUT3,
     SH7722_LAYER_MULTIWIN
} SH7722LayerID;


typedef struct {
     SH7722LayerID            layer;
} SH7722LayerData;

typedef struct {
     int                      magic;

     CoreLayerRegionConfig    config;

     CoreSurface             *surface;
     CorePalette             *palette;
} SH7722RegionData;


typedef struct {
     unsigned int             added;
     unsigned int             visible;
} SH7722MultiLayerData;

typedef struct {
     int                      magic;

     int                      index;
     CoreLayerRegionConfig    config;

     CoreSurface             *surface;
     CorePalette             *palette;
} SH7722MultiRegionData;


typedef struct {
     int                      lcd_width;
     int                      lcd_height;
     int                      lcd_offset;
     int                      lcd_pitch;
     int                      lcd_size;
     unsigned long            lcd_phys;

     /* state validation */
     int                      v_flags;

     /* prepared register values */
     u32                      ble_srcf;
     u32                      ble_dstf;

     /* cached values */
     u32                      dst_offset;
     int                      dst_pitch;
     int                      dst_bpp;

     u32                      src_offset;
     int                      src_pitch;
     int                      src_bpp;

     DFBSurfaceDrawingFlags   dflags;
     DFBSurfaceBlittingFlags  bflags;
     DFBSurfaceRenderOptions  render_options;

     bool                     ckey_b_enabled;
     bool                     color_change_enabled;

     unsigned int             input_mask;

     s32                      matrix[6];
     DFBColor                 color;

     /* locking */
     FusionSkirmish           beu_lock;

     /* JPEG */
     FusionSkirmish           jpeg_lock;
     int                      jpeg_offset;
     int                      jpeg_size;
     unsigned long            jpeg_phys;
} SH7722DeviceData;


typedef struct {
     SH7722DeviceData        *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;

     CoreScreen              *screen;

     CoreLayer               *multi;
     CoreLayer               *input1;
     CoreLayer               *input2;
     CoreLayer               *input3;

     int                      gfx_fd;
     SH7722GfxSharedArea     *gfx_shared;

     int                      prep_num;
     __u32                    prep_buf[SH7722GFX_MAX_PREPARE];

     volatile void           *mmio_base;

     int                      num_inputs;

     volatile void           *lcd_virt;

     volatile void           *jpeg_virt;
} SH7722DriverData;

#endif

