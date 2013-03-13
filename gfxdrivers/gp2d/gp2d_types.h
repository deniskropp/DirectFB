#ifndef __GP2D__TYPES_H__
#define __GP2D__TYPES_H__

#include <directfb.h>
#include <core/coretypes.h>

#include <drm/gp2d_drm.h>


#define GP2DGFX_BUFFER_SIZE             65536


typedef struct {
     /* state validation */
     int                      v_flags;

     /* prepared register values */
     u32                      ble_srcf;
     u32                      ble_dstf;

     /* cached values */
     int                      dst_fd;
     int                      dst_pitch;
     int                      dst_bpp;
     int                      dst_index;
     DFBDimension             dst_size;

     void                    *src_addr;
     int                      src_fd;
     int                      src_pitch;
     int                      src_bpp;
     int                      src_index;
     DFBDimension             src_size;
     DFBSurfacePixelFormat    src_format;

     DFBSurfaceDrawingFlags   dflags;
     DFBSurfaceBlittingFlags  bflags;
     DFBSurfaceRenderOptions  render_options;

     DFBColor                 color;

     bool                     mode_32bit;

     /* gp2d */
     u32                      rclr;
     u32                      color_bits;
} GP2DDeviceData;


typedef struct {
     DirectLink               link;

     int                      magic;

     u32                      handle;
     unsigned int             size;
     void                    *mapped;

     unsigned int             used;
     DFB_SurfaceTask         *task;
} GP2DBuffer;

typedef struct {
     GP2DDeviceData          *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;

     int                      gfx_fd;

     int                      prep_num;

     GP2DBuffer              *current;

     DirectLink              *emitted;
     DirectLink              *free;

     DirectMutex              buffer_lock;
     DirectWaitQueue          buffer_wq;

     DirectThread            *event_thread;
} GP2DDriverData;




#endif

