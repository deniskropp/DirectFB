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

static void
vsp1_buffer_add_source( VSP1DriverData     *gdrv,
                        VSP1DeviceData     *gdev,
                        VSP1Buffer         *buffer,
                        int                 fd,
                        int                 pitch,
                        int                 width,
                        int                 height,
                        int                 sx,
                        int                 sy,
                        int                 sw,
                        int                 sh,
                        int                 dx,
                        int                 dy,
                        int                 dw,
                        int                 dh );

static DFBResult
vsp1_buffer_create( VSP1DriverData  *gdrv,
                    VSP1DeviceData  *gdev,
                    int              fd,
                    int              pitch,
                    int              width,
                    int              height,
                    bool             init,
                    VSP1Buffer     **ret_buffer )
{
     DFBResult   ret;
     VSP1Buffer *buffer;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_buffer != NULL );

     /* Allocate new buffer */
     buffer = D_CALLOC( 1, sizeof(VSP1Buffer) );
     if (!buffer) {
          ret = D_OOSHM();
          goto error;
     }


     /* Initialize buffer */

     D_DEBUG_AT( VSP1_BLT, "  -> creating output... %dx%d\n", width, height );

     buffer->output = v4l2_device_interface.create_output( gdrv->vsp_renderer_data, width, height );
     buffer->fd     = fd;
     buffer->pitch  = pitch;
     buffer->size.w = width;
     buffer->size.h = height;

     dfb_updates_init( &buffer->updates, buffer->updates_regions, D_ARRAY_SIZE(buffer->updates_regions) );


     D_DEBUG_AT( VSP1_BLT, "  -> setting output buffer... (fd %d)\n", fd );

     struct v4l2_bo_state output_state;

     output_state.dmafd  = fd;
     output_state.stride = pitch;

     v4l2_device_interface.set_output_buffer( buffer->output, &output_state );


     D_DEBUG_AT( VSP1_BLT, "  -> beginning...\n" );

     v4l2_device_interface.begin_compose( gdrv->vsp_renderer_data, buffer->output );


     D_MAGIC_SET( buffer, VSP1Buffer );


     if (init) {
          D_DEBUG_AT( VSP1_BLT, "  -> drawing destination as background...\n" );

//          vsp1_buffer_add_source( gdrv, gdev, buffer, fd, pitch, width, height, 0, 0, width, height, 0, 0, width, height );
//
//   OR:
//

          v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, &((struct vsp_device*) (gdrv->vsp_renderer_data))->output_surface_state->base );

          buffer->sources++;
     }


     D_DEBUG_AT( VSP1_BLT, "  -> %p\n", buffer );

     /* Return new buffer */
     *ret_buffer = buffer;

     return DFB_OK;


error:
     if (buffer) {
          D_FREE( buffer );
     }

     return ret;
}

static void
vsp1_buffer_destroy( VSP1Buffer *buffer )
{
     D_DEBUG_AT( VSP1_BLT, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, VSP1Buffer );

     // FIXME: add deinit calls
     dfb_updates_deinit( &buffer->updates );

     D_MAGIC_CLEAR( buffer );

     D_FREE( buffer );
}

static void
vsp1_buffer_add_source( VSP1DriverData     *gdrv,
                        VSP1DeviceData     *gdev,
                        VSP1Buffer         *buffer,
                        int                 fd,
                        int                 pitch,
                        int                 width,
                        int                 height,
                        int                 sx,
                        int                 sy,
                        int                 sw,
                        int                 sh,
                        int                 dx,
                        int                 dy,
                        int                 dw,
                        int                 dh )
{
     D_DEBUG_AT( VSP1_BLT, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, VSP1Buffer );


     D_DEBUG_AT( VSP1_BLT, "  -> creating surface... %dx%d\n", width, height );

     struct v4l2_surface_state *source;

     source = v4l2_device_interface.create_surface( gdrv->vsp_renderer_data );

     source->width        = width;
     source->height       = height;
     source->pixel_format = V4L2_PIX_FMT_ABGR32 | gdev->premul_alpha;
     source->bpp          = 4;
     source->num_planes   = 1;

     source->planes[0].stride = pitch;
     source->planes[0].dmafd  = fd;


     D_DEBUG_AT( VSP1_BLT, "  -> attaching buffer... (fd %d)\n", fd );

     v4l2_device_interface.attach_buffer( source );


     D_DEBUG_AT( VSP1_BLT, "  -> drawing view...\n" );

     source->src_rect.left   = sx;
     source->src_rect.top    = sy;
     source->src_rect.width  = sw;
     source->src_rect.height = sh;

     source->dst_rect.left   = dx;
     source->dst_rect.top    = dy;
     source->dst_rect.width  = dw;
     source->dst_rect.height = dh;

     source->alpha = (gdev->dflags & DSDRAW_BLEND) ? gdev->color.a / 255.0f : 1.0f;

     dfb_updates_add_rect( &buffer->updates, dx, dy, dw, dh );

     v4l2_device_interface.draw_view( gdrv->vsp_renderer_data, source );

     buffer->sources++;
}

/**********************************************************************************************************************/

//D_UNUSED
static void
vsp1_buffer_submit( VSP1DriverData *gdrv,
                    VSP1DeviceData *gdev,
                    VSP1Buffer     *buffer )
{
     D_DEBUG_AT( VSP1_BLT, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, VSP1Buffer );


     D_ASSERT( gdrv->idle == true );

     gdrv->idle = false;

     D_DEBUG_AT( VSP1_BLT, "  -> flush()\n" );

     direct_list_append( &gdrv->queue, &buffer->link );

     D_ASSERT( buffer->updates.num_regions > 0 );
     struct v4l2_rect r = { DFB_RECTANGLE_VALS_FROM_REGION( &buffer->updates.bounding ) };
     v4l2_device_interface.flush( gdrv->vsp_renderer_data, &r );

     direct_waitqueue_broadcast( &gdrv->q_submit );
}

void
vsp1_buffer_finished( VSP1DriverData *gdrv,
                      VSP1DeviceData *gdev,
                      VSP1Buffer     *buffer )
{
     D_DEBUG_AT( VSP1_BLT, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, VSP1Buffer );

     vsp1_buffer_destroy( buffer );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static DFBResult
vsp1GenFlush( VSP1DriverData  *gdrv,
              VSP1DeviceData  *gdev,
              int              min_sources )
{
     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     min_sources = 1;

     if (gdrv->current && gdrv->current->sources >= min_sources) {
          D_DEBUG_AT( VSP1_BLT, "  -> submit...\n" );

#if VSP1_USE_THREAD
          vsp1_buffer_submit( gdrv, gdev, gdrv->current );
#else
          v4l2_device_interface.flush( gdrv->vsp_renderer_data );

          v4l2_device_interface.finish_compose( gdrv->vsp_renderer_data );
          vsp1_buffer_finished( gdrv, gdrv->dev, gdrv->current );
#endif

          gdrv->current = NULL;
     }
     else
          D_DEBUG_AT( VSP1_BLT, "  -> nothing to submit!\n" );

     return DFB_OK;
}

static DFBResult
vsp1GenWait( VSP1DriverData *gdrv,
             VSP1DeviceData *gdev )
{
     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     while (!gdrv->idle) {
          D_DEBUG_AT( VSP1_BLT, "  -> waiting...\n" );

          direct_waitqueue_wait( &gdrv->q_idle, &gdrv->q_lock );
     }

     D_DEBUG_AT( VSP1_BLT, "  -> finished waiting from %s()\n", __FUNCTION__ );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
vsp1GenSetup( VSP1DriverData *gdrv,
              VSP1DeviceData *gdev,
              bool            init,
              bool            fake,
              int             width,
              int             height )
{
     DFBResult ret = DFB_OK;
     int       fd;
     int       pitch;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     if (fake) {
          VSP1FakeSource *fake_source;

          fake_source = gdrv->fake_sources[gdrv->fake_source_index++];

          if (gdrv->fake_source_index == 4)
               gdrv->fake_source_index = 0;

          fd     = fake_source->lock.offset;
          pitch  = fake_source->lock.pitch;
     }
     else {
          fd     = gdev->dst_fd;;
          pitch  = gdev->dst_pitch;
     }

     if (gdrv->current) {
          if (gdrv->current->fd != fd ||
              gdrv->current->pitch != pitch ||
              gdrv->current->size.w != width ||
              gdrv->current->size.h != height)
          {
               D_DEBUG_AT( VSP1_BLT, "  -> different destination\n" );

               vsp1GenFlush( gdrv, gdev, 1 );
          }
     }

     vsp1GenWait( gdrv, gdev );

     if (!gdrv->current) {
          D_DEBUG_AT( VSP1_BLT, "  -> creating new buffer...\n" );

          ret = vsp1_buffer_create( gdrv, gdev, fd, pitch, width, height, init, &gdrv->current );
     }

     return ret;
}

/**********************************************************************************************************************/

static bool
vsp1GenFill( void *drv, void *dev, int dx, int dy, int dw, int dh )
{
     DFBResult       ret;
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;
     VSP1FakeSource *fake_source;
     int             sw, sh;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     sw = dw/11 + 1;
     sh = dh/11 + 1;

     if (sw < 8)
          sw = 8;

     if (sh < 8)
          sh = 8;



     fake_source = gdrv->fake_sources[gdrv->fake_source_index++];

     if (gdrv->fake_source_index == 4)
          gdrv->fake_source_index = 0;

     u32 *ptr = fake_source->lock.addr;
     int  x, y;

     for (y=0; y<sh; y++) {
          for (x=0; x<sw; x++) {
               ptr[x+y*fake_source->lock.pitch/4] = PIXEL_ARGB( gdev->color.a, gdev->color.r, gdev->color.g, gdev->color.b );
          }
     }


     ret = vsp1GenSetup( gdrv, gdev,
//                         dx > 0 || dy > 0 || (dx + dw) < gdev->dst_size.w || (dy + dh) < gdev->dst_size.h,
                         false, false, gdev->dst_size.w, gdev->dst_size.h );
     if (ret)
          return false;

     if (gdev->dflags & DSDRAW_BLEND) {
          vsp1_buffer_add_source( gdrv, gdev, gdrv->current,
                                  gdrv->current->fd, gdrv->current->pitch,
                                  gdrv->current->size.w, gdrv->current->size.h,
                                  dx, dy, dw, dh,
                                  dx, dy, dw, dh );
     }

     vsp1_buffer_add_source( gdrv, gdev, gdrv->current, fake_source->lock.offset,
                             fake_source->lock.pitch, sw, sh,
                             0, 0, sw, sh, dx, dy, dw, dh );

     vsp1GenFlush( gdrv, gdev, 2 );

     return true;
}

static bool
vsp1GenBlit( void *drv, void *dev,
             int sx, int sy, int sw, int sh,
             int dx, int dy, int dw, int dh,
             int src_fd, int src_pitch, int src_w, int src_h )
{
     DFBResult       ret;
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;
     float           scale_w;
     float           scale_h;
     float           scale;

     D_DEBUG_AT( VSP1_BLT, "%s( %d, %d - %dx%d <- %dx%d - %d, %d,  fd %d, pitch %d )\n", __FUNCTION__,
                 dx, dy, dw, dh, sw, sh, sx, sy, src_fd, src_pitch );

     while (true) {
          if (dw > sw)
               scale_w = (float) dw / (float) sw;
          else
               scale_w = (float) sw / (float) dw;

          if (dh > sh)
               scale_h = (float) dh / (float) sh;
          else
               scale_h = (float) sh / (float) dh;

          scale = (scale_w > scale_h) ? scale_w : scale_h;

          D_DEBUG_AT( VSP1_BLT, "  -> scale %f (%f, %f)\n", scale, scale_w, scale_h );

          if (scale <= 11.0f)
               break;

          int tw = (sw > dw) ? (sw / 11 + 1) : (sw * 11);
          int th = (sh > dh) ? (sh / 11 + 1) : (sh * 11);

          ret = vsp1GenSetup( gdrv, gdev, false, true, tw, th );
          if (ret)
               return false;

          vsp1_buffer_add_source( gdrv, gdev, gdrv->current,
                                  src_fd, src_pitch, src_w, src_h,
                                  sx, sy, sw, sh, 0, 0, tw, th );

          src_fd    = gdrv->current->fd;
          src_pitch = gdrv->current->pitch;
          src_w     = tw;
          src_h     = th;

          vsp1GenFlush( gdrv, gdev, 1 );

          sx = 0;
          sy = 0;
          sw = tw;
          sh = th;
     }


     ret = vsp1GenSetup( gdrv, gdev,
//                         dx > 0 || dy > 0 || (dx + dw) < gdev->dst_size.w || (dy + dh) < gdev->dst_size.h ||
                         false, false, gdev->dst_size.w, gdev->dst_size.h );
     if (ret)
          return false;

     if (gdev->bflags & DSBLIT_BLEND_ALPHACHANNEL) {
          vsp1_buffer_add_source( gdrv, gdev, gdrv->current,
                                  gdrv->current->fd, gdrv->current->pitch,
                                  gdrv->current->size.w, gdrv->current->size.h,
                                  dx, dy, dw, dh,
                                  dx, dy, dw, dh );
     }

     vsp1_buffer_add_source( gdrv, gdev, gdrv->current,
                             src_fd, src_pitch, src_w, src_h,
                             sx, sy, sw, sh, dx, dy, dw, dh );

     vsp1GenFlush( gdrv, gdev, (sw == dw && sh == dh) ? 4 : 1 );

     return true;
}

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

/**********************************************************************************************************************/

DFBResult
vsp1EngineSync( void *drv, void *dev )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &gdrv->q_lock );

     vsp1GenFlush( gdrv, gdev, 1 );
     vsp1GenWait( gdrv, gdev );

     direct_mutex_unlock( &gdrv->q_lock );

     return DFB_OK;
}

void
vsp1EmitCommands( void *drv, void *dev )
{
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &gdrv->q_lock );

     vsp1GenFlush( gdrv, gdev, 1 );

     direct_mutex_unlock( &gdrv->q_lock );
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
               if ((state->src_blend != DSBF_ONE &&
                    state->src_blend != DSBF_SRCALPHA) || state->dst_blend != DSBF_INVSRCALPHA) {
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

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          if (state->src_blend == DSBF_SRCALPHA)
               gdev->premul_alpha = 0;
          else if (!(state->blittingflags & DSBLIT_SRC_PREMULTIPLY))
               gdev->premul_alpha = 0;
          else
               gdev->premul_alpha = V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
     }
     else
          gdev->premul_alpha = V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;

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

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
vsp1FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     bool            result;
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     if (rect->w * rect->h < 2048)
          return false;

     direct_mutex_lock( &gdrv->q_lock );

     result = vsp1GenFill( gdrv, gdev, rect->x, rect->y, rect->w, rect->h );

     direct_mutex_unlock( &gdrv->q_lock );

     return result;
}

/**********************************************************************************************************************/

/*
 * Blit a rectangle using the current hardware state.
 */
bool
vsp1Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     bool            result;
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     D_DEBUG_AT( VSP1_BLT, "%s( %d, %d - %dx%d <- %d, %d )\n", __FUNCTION__,
                 dx, dy, rect->w, rect->h, rect->x, rect->y );

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     direct_mutex_lock( &gdrv->q_lock );

     result = vsp1GenBlit( gdrv, gdev, rect->x, rect->y, rect->w, rect->h, dx, dy, rect->w, rect->h, gdev->src_fd, gdev->src_pitch, gdev->src_size.w, gdev->src_size.h );

     direct_mutex_unlock( &gdrv->q_lock );

     return result;
}

/*
 * StretchBlit a rectangle using the current hardware state.
 */
bool
vsp1StretchBlit( void *drv, void *dev,
                 DFBRectangle *srect,
                 DFBRectangle *drect )
{
     bool            result;
     VSP1DriverData *gdrv = drv;
     VSP1DeviceData *gdev = dev;

     if (gdev->disabled) {
          D_DEBUG_AT( VSP1_BLT, "  -> disabled\n" );
          return false;
     }

     direct_mutex_lock( &gdrv->q_lock );

     result = vsp1GenBlit( gdrv, gdev, srect->x, srect->y, srect->w, srect->h, drect->x, drect->y, drect->w, drect->h, gdev->src_fd, gdev->src_pitch, gdev->src_size.w, gdev->src_size.h );

     direct_mutex_unlock( &gdrv->q_lock );

     return result;
}

