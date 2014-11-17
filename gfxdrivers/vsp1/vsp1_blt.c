//#ifdef VSP1_DEBUG_BLT
//     #define DIRECT_ENABLE_DEBUG
//#endif


#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <errno.h>

#include <asm/types.h>

#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/Debug.h>
#include <core/Task.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "v4l2-compat.h"

#include "vsp1_driver.h"

#include "vsp1_blt.h"


D_DEBUG_DOMAIN( VSP1_BLT, "VSP1/BLT", "Renesas VSP1 Blitting Engine" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DESTINATION    = 0x00000001,

     SOURCE         = 0x00010000,
     FAKE_SOURCE    = 0x00100000,

     ALL            = 0x00110011,
};

/*
 * State handling macros.
 */

#define VSP1_VALIDATE(flags)          do { gdev->v_flags |=  (flags); } while (0)
#define VSP1_INVALIDATE(flags)        do { gdev->v_flags &= ~(flags); } while (0)

#define VSP1_CHECK_VALIDATE(flag)     do {                                                          \
                                             if ((gdev->v_flags & flag) != flag)                    \
                                                  vsp1_validate_##flag( gdrv, gdev, state );        \
                                        } while (0)

/**********************************************************************************************************************/

static inline void
vsp1_validate_DESTINATION( VSP1DriverData *gdrv,
                           VSP1DeviceData *gdev,
                           CardState      *state )
{
     CoreSurfaceAllocation *alloc = state->dst.allocation;

     D_DEBUG_AT( VSP1_BLT, "%s( fd %lu, pitch %d )\n", __FUNCTION__, state->dst.offset, state->dst.pitch );
     D_DEBUG_AT( VSP1_BLT, "  -> %s\n", ToString_CoreSurfaceAllocation( state->dst.allocation ) );

     /* Get the file descriptor from surface pool (requires drmkms-use-prime-fd option) */
     gdev->dst_fd     = state->dst.offset;
     gdev->dst_pitch  = state->dst.pitch;
     gdev->dst_bpp    = DFB_BYTES_PER_PIXEL( alloc->config.format );
     gdev->dst_index  = DFB_PIXELFORMAT_INDEX( alloc->config.format ) % DFB_NUM_PIXELFORMATS;
     gdev->dst_size   = alloc->config.size;
     gdev->dst_format = alloc->config.format;

     /* Set the flags. */
     VSP1_VALIDATE( DESTINATION );
}

static inline void
vsp1_validate_SOURCE( VSP1DriverData *gdrv,
                      VSP1DeviceData *gdev,
                      CardState      *state )
{
     CoreSurfaceAllocation *alloc = state->src.allocation;

     D_DEBUG_AT( VSP1_BLT, "%s( fd %lu, pitch %d )\n", __FUNCTION__, state->src.offset, state->src.pitch );
     D_DEBUG_AT( VSP1_BLT, "  -> %s\n", ToString_CoreSurfaceAllocation( state->src.allocation ) );

     /* Get the file descriptor from surface pool (requires drmkms-use-prime-fd option) */
     gdev->src_fd     = state->src.offset;
     gdev->src_pitch  = state->src.pitch;
     gdev->src_bpp    = DFB_BYTES_PER_PIXEL( alloc->config.format );
     gdev->src_index  = DFB_PIXELFORMAT_INDEX( alloc->config.format ) % DFB_NUM_PIXELFORMATS;
     gdev->src_size   = alloc->config.size;
     gdev->src_format = alloc->config.format;

     /* Set the flags. */
     VSP1_VALIDATE( SOURCE );
}

static inline void
vsp1_validate_FAKE_SOURCE( VSP1DriverData *gdrv,
                           VSP1DeviceData *gdev,
                           CardState      *state )
{
     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     /* Set the flags. */
     VSP1_VALIDATE( FAKE_SOURCE );
}

/**********************************************************************************************************************/

DFBResult
vsp1EngineSync( void *drv, void *dev )
{
//     VSP1DriverData *gdrv = drv;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );
#if 0
     v4l2_device_interface.finish_compose( gdrv->vsp_renderer_data );

     gdrv->sources = 0;
#endif
     return DFB_OK;
}

void
vsp1EmitCommands( void *drv, void *dev )
{
//     VSP1DriverData *gdrv = drv;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );
#if 0
     if (gdrv->sources) {
          D_DEBUG_AT( VSP1_BLT, "  -> flushing...\n" );
          
          v4l2_device_interface.flush( gdrv->vsp_renderer_data, false );
          
          gdrv->sources = 0;
     }
#endif
}

/**********************************************************************************************************************/

void
vsp1CheckState( void                *drv,
                void                *dev,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     D_DEBUG_AT( VSP1_BLT, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(VSP1_SUPPORTED_DRAWINGFUNCTIONS | VSP1_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_ARGB:
               break;

          default:
               D_DEBUG_AT( VSP1_BLT, "  -> unsupported destination format '%s'\n",
                           dfb_pixelformat_name(state->destination->config.format) );
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~VSP1_SUPPORTED_DRAWINGFLAGS) {
               D_DEBUG_AT( VSP1_BLT, "  -> unsupported drawing flags '%s'\n",
                           ToString_DFBSurfaceDrawingFlags( state->drawingflags & ~VSP1_SUPPORTED_DRAWINGFLAGS ) );
               return;
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               /* Return if blending with unsupported blend functions is requested. */
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA) {
                    D_DEBUG_AT( VSP1_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }
          }

          /* Enable acceleration of drawing functions. */
          state->accel |= accel;
     } else {
          DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~VSP1_SUPPORTED_BLITTINGFLAGS) {
               D_DEBUG_AT( VSP1_BLT, "  -> unsupported blitting flags '%s'\n",
                           ToString_DFBSurfaceDrawingFlags( flags & ~VSP1_SUPPORTED_BLITTINGFLAGS ) );
               return;
          }

          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB:
                    break;

               default:
                    D_DEBUG_AT( VSP1_BLT, "  -> unsupported source format '%s'\n",
                                dfb_pixelformat_name(state->source->config.format) );
                    return;
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA) {
                    D_DEBUG_AT( VSP1_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }
          }

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( flags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               D_DEBUG_AT( VSP1_BLT, "  -> unsupported (line %d)\n", __LINE__ );
               return;
          }

          /* Enable acceleration of blitting functions. */
          state->accel |= accel;
     }
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
vsp1SetState( void                *drv,
              void                *dev,
              GraphicsDeviceFuncs *funcs,
              CardState           *state,
              DFBAccelerationMask  accel )
{
     VSP1DriverData         *gdrv     = drv;
     VSP1DeviceData         *gdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( VSP1_BLT, "%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );

     if (state->dst.offset == ~0UL) {
          D_ONCE( "vsp1 destination without file descriptor" );
          gdev->disabled = true;
          return;
     }
     else
          gdev->disabled = false;

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          VSP1_INVALIDATE( ALL );
     } else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               VSP1_INVALIDATE( DESTINATION );

          /* Invalidate clipping registers. */
//          if (modified & SMF_CLIP)
//               VSP1_INVALIDATE( CLIP );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               VSP1_INVALIDATE( SOURCE );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination and clip. */
     VSP1_CHECK_VALIDATE( DESTINATION );
//     VSP1_CHECK_VALIDATE( CLIP );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               /* ...require valid color. */
               VSP1_CHECK_VALIDATE( FAKE_SOURCE );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VSP1_SUPPORTED_DRAWINGFUNCTIONS;

               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               /* ...require valid source. */
               VSP1_CHECK_VALIDATE( SOURCE );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VSP1_SUPPORTED_BLITTINGFUNCTIONS;

               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;

     }

     gdev->dflags         = state->drawingflags;
     gdev->bflags         = state->blittingflags;
     gdev->render_options = state->render_options;
     gdev->color          = state->color;
     gdev->clip           = state->clip;

     dfb_simplify_blittingflags( &gdev->bflags );

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further validation.
      * If the hw independent state is not modified, this function won't get called
      * for subsequent rendering functions, unless they aren't defined by 3).
      */
     state->mod_hw = 0;
}

/**********************************************************************************************************************/

static bool
vsp1GenFill( void *drv, void *dev, int dx, int dy, int dw, int dh )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }



     D_DEBUG_AT( VSP1_BLT, "  -> creating output...\n" );

     gdrv->output = v4l2_device_interface.create_output( gdrv->vsp_renderer_data, gdev->dst_size.w, gdev->dst_size.h );


     D_DEBUG_AT( VSP1_BLT, "  -> setting output buffer... (fd %d)\n", gdev->dst_fd );

     struct v4l2_bo_state output_state;

     output_state.dmafd  = gdev->dst_fd;
     output_state.stride = gdev->dst_pitch;

     v4l2_device_interface.set_output_buffer( gdrv->output, &output_state );



     D_DEBUG_AT( VSP1_BLT, "  -> beginning...\n" );

     if (gdrv->sources == 0) {
          v4l2_device_interface.begin_compose( gdrv->vsp_renderer_data, gdrv->output );

          v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, &((struct vsp_device*) (gdrv->vsp_renderer_data))->output_surface_state->base );
     }


     D_DEBUG_AT( VSP1_BLT, "  -> creating surface...\n" );

     gdrv->source = v4l2_device_interface.create_surface( gdrv->vsp_renderer_data );

     gdrv->source->width        = 8;
     gdrv->source->height       = 8;
     gdrv->source->pixel_format = V4L2_PIX_FMT_ABGR32;
     gdrv->source->bpp          = 4;
     gdrv->source->num_planes   = 1;

     for (int i = 0; i < 1/*kbuf->num_planes*/; i++) {
          gdrv->source->planes[i].stride = gdrv->fake_source_lock.pitch;
          gdrv->source->planes[i].dmafd  = gdrv->fake_source_lock.offset;
     }


     D_DEBUG_AT( VSP1_BLT, "  -> attaching buffer... (fd %lu)\n", gdrv->fake_source_lock.offset );

     v4l2_device_interface.attach_buffer( gdrv->source );



     u32 *ptr = gdrv->fake_source_lock.addr;
     int  x, y;

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               ptr[x+y*gdrv->fake_source_lock.pitch/4] = PIXEL_ARGB( 0xff, gdev->color.r, gdev->color.g, gdev->color.b );
          }
     }



     D_DEBUG_AT( VSP1_BLT, "  -> drawing view...\n" );

     gdrv->source->src_rect.left   = 0;
     gdrv->source->src_rect.top    = 0;
     gdrv->source->src_rect.width  = 8;
     gdrv->source->src_rect.height = 8;

     gdrv->source->dst_rect.left   = dx;
     gdrv->source->dst_rect.top    = dy;
     gdrv->source->dst_rect.width  = dw;
     gdrv->source->dst_rect.height = dh;

     gdrv->source->alpha = (gdev->dflags & DSDRAW_BLEND) ? gdev->color.a / 255.0f : 255.0f;

     v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, gdrv->source );

     v4l2_device_interface.finish_compose( gdrv->vsp_renderer_data );

#if 0
     gdrv->sources++;

     if (gdrv->sources == 3) {
          D_DEBUG_AT( VSP1_BLT, "  -> flushing...\n" );

          v4l2_device_interface.flush( gdrv->vsp_renderer_data, false );

          gdrv->sources = 0;
     }
#endif
     return true;
}

/**********************************************************************************************************************/

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
vsp1FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     return vsp1GenFill( drv, dev, rect->x, rect->y, rect->w, rect->h );
}

/**********************************************************************************************************************/

static bool
vsp1GenBlit( void *drv, void *dev,
             DFBRectangle *rect,
             int dx, int dy, int dw, int dh )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }



     D_DEBUG_AT( VSP1_BLT, "  -> creating output...\n" );

     gdrv->output = v4l2_device_interface.create_output( gdrv->vsp_renderer_data, gdev->dst_size.w, gdev->dst_size.h );


     D_DEBUG_AT( VSP1_BLT, "  -> setting output buffer... (fd %d)\n", gdev->dst_fd );

     struct v4l2_bo_state output_state;

     output_state.dmafd  = gdev->dst_fd;
     output_state.stride = gdev->dst_pitch;

     v4l2_device_interface.set_output_buffer( gdrv->output, &output_state );



     D_DEBUG_AT( VSP1_BLT, "  -> beginning...\n" );

     if (gdrv->sources == 0) {
          v4l2_device_interface.begin_compose( gdrv->vsp_renderer_data, gdrv->output );

          v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, &((struct vsp_device*) (gdrv->vsp_renderer_data))->output_surface_state->base );
     }


     D_DEBUG_AT( VSP1_BLT, "  -> creating surface...\n" );

     gdrv->source = v4l2_device_interface.create_surface( gdrv->vsp_renderer_data );

     gdrv->source->width        = gdev->src_size.w;
     gdrv->source->height       = gdev->src_size.h;
     gdrv->source->pixel_format = V4L2_PIX_FMT_ABGR32;
     gdrv->source->bpp          = 4;
     gdrv->source->num_planes   = 1;

     for (int i = 0; i < 1/*kbuf->num_planes*/; i++) {
          gdrv->source->planes[i].stride = gdev->src_pitch;
          gdrv->source->planes[i].dmafd  = gdev->src_fd;
     }


     D_DEBUG_AT( VSP1_BLT, "  -> attaching buffer... (fd %d)\n", gdev->src_fd );

     v4l2_device_interface.attach_buffer( gdrv->source );



     D_DEBUG_AT( VSP1_BLT, "  -> drawing view...\n" );

     gdrv->source->src_rect.left   = rect->x;
     gdrv->source->src_rect.top    = rect->y;
     gdrv->source->src_rect.width  = rect->w;
     gdrv->source->src_rect.height = rect->h;

     gdrv->source->dst_rect.left   = dx;
     gdrv->source->dst_rect.top    = dy;
     gdrv->source->dst_rect.width  = dw;
     gdrv->source->dst_rect.height = dh;

     gdrv->source->alpha = (gdev->bflags & DSBLIT_BLEND_COLORALPHA) ? gdev->color.a / 255.0f : 255.0f;

     v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, gdrv->source );

     v4l2_device_interface.finish_compose( gdrv->vsp_renderer_data );

#if 0
     gdrv->sources++;

     if (gdrv->sources == 3) {
          D_DEBUG_AT( VSP1_BLT, "  -> flushing...\n" );

          v4l2_device_interface.flush( gdrv->vsp_renderer_data, false );

          gdrv->sources = 0;
     }
#endif
     return true;
}

/**********************************************************************************************************************/

/*
 * Blit a rectangle using the current hardware state.
 */
bool
vsp1Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s( %d, %d - %dx%d <- %d, %d )\n", __FUNCTION__,
                 dx, dy, rect->w, rect->h, rect->x, rect->y );

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     return vsp1GenBlit( gdrv, gdev, rect, dx, dy, rect->w, rect->h );
}

/*
 * StretchBlit a rectangle using the current hardware state.
 */
bool
vsp1StretchBlit( void *drv, void *dev,
                 DFBRectangle *srect,
                 DFBRectangle *drect )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     return vsp1GenBlit( gdrv, gdev, srect, drect->x, drect->y, drect->w, drect->h );
}

