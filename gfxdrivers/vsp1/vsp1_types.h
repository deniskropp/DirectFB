#ifndef __VSP1__TYPES_H__
#define __VSP1__TYPES_H__

#include <directfb.h>
#include <core/coretypes.h>

#include "media-ctl/mediactl.h"
#include "media-ctl/v4l2subdev.h"
#include "media-ctl/tools.h"

#include "vsp-renderer.h"


#ifndef V4L2_PIX_FMT_FLAG_PREMUL_ALPHA
#define V4L2_PIX_FMT_FLAG_PREMUL_ALPHA	0x00000001
#endif



/*
 
 
     vsp1GenBlit()
 
          vsp1GenSetup()
 
      
 
 
*/



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

     u32                      premul_alpha;
} VSP1DeviceData;


typedef struct {
     DirectLink                    link;

     int                           magic;

     struct v4l2_renderer_output  *output;
     int                           sources;

     u32                           fd;
     int                           pitch;
     DFBDimension                  size;

     DFBUpdates			   updates;
     DFBRegion			   updates_regions[4];
} VSP1Buffer;

typedef struct {
     int                          magic;
     CoreSurface                 *surface;
     CoreSurfaceBuffer           *buffer;
     DFBSurfaceBufferID           buffer_id;
     CoreSurfaceAllocation       *allocation;
     CoreSurfaceBufferLock        lock;
} VSP1FakeSource;

typedef struct {
     VSP1DeviceData          *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;


     struct media_device     *media;
     char                    *device_name;


     void                    *vsp_renderer_data;


     bool                     idle;
     bool                     quit;


     VSP1Buffer              *current;


     VSP1FakeSource          *fake_sources[4];
     int                      fake_source_index;


     DirectMutex              q_lock;
     DirectWaitQueue          q_idle;
     DirectWaitQueue          q_submit;

     DirectLink              *queue;

     DirectThread            *event_thread;
} VSP1DriverData;




#endif

