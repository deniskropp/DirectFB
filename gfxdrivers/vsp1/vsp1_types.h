#ifndef __VSP1__TYPES_H__
#define __VSP1__TYPES_H__

#include <directfb.h>
#include <core/coretypes.h>

#include "media-ctl/mediactl.h"
#include "media-ctl/v4l2subdev.h"
#include "media-ctl/tools.h"

#include "vsp-renderer.h"


typedef struct {
     /* state validation */
     int                      v_flags;

     bool                     disabled;

     /* cached values */
     int                      dst_fd;
     int                      dst_pitch;
     int                      dst_bpp;
     int                      dst_index;
     DFBDimension             dst_size;
     DFBSurfacePixelFormat    dst_format;

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
     DFBRegion                clip;
} VSP1DeviceData;


typedef struct {
     DirectLink               link;

     int                      magic;

     u32                      handle;
     unsigned int             size;
     void                    *mapped;
} VSP1Buffer;

typedef struct {
     VSP1DeviceData          *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;


     struct media_device     *media;
     char                    *device_name;

     void                    *vsp_renderer_data;

     struct v4l2_renderer_output *output;
     struct v4l2_surface_state   *source;

     int                          sources;


     DirectMutex              q_lock;
     DirectWaitQueue          q_wait;

     DirectLink              *queue;

     DirectThread            *event_thread;
} VSP1DriverData;




#endif

