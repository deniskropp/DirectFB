//#ifdef GP2D_DEBUG_BLT
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

#include "gp2d_driver.h"

#include "gp2d_blt.h"


D_DEBUG_DOMAIN( GP2D_BLT, "GP2D/BLT", "Renesas GP2D Drawing Engine" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DEST           = 0x00000001,
     CLIP           = 0x00000002,
     DEST_CLIP      = 0x00000003,

     COLOR          = 0x00000100,

     ALPHA_DRAW     = 0x00001000,
     ALPHA_BLIT     = 0x00002000,

     SOURCE         = 0x00010000,
     STRANS         = 0x00020000,
     MATRIX         = 0x00040000,

     ALL            = 0x00073103,
};

/*
 * State handling macros.
 */

#define GP2D_VALIDATE(flags)          do { gdev->v_flags |=  (flags); } while (0)
#define GP2D_INVALIDATE(flags)        do { gdev->v_flags &= ~(flags); } while (0)

#define GP2D_CHECK_VALIDATE(flag)     do {                                                          \
                                             if ((gdev->v_flags & flag) != flag)                    \
                                                  gp2d_validate_##flag( gdrv, gdev, state );        \
                                        } while (0)

/**********************************************************************************************************************/

DFBResult
gp2d_blt_gen_free( GP2DDriverData *gdrv,
                   unsigned int    num )
{
     int          ret = 0;
     unsigned int i;

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     for (i=0; i<num; i++) {
          GP2DBuffer *buffer = D_CALLOC( 1, sizeof(GP2DBuffer) );

          if (!buffer) {
               ret = D_OOM();
               goto error;
          }

          int                        ret;
          struct drm_gp2d_gem_create gem_create;
          struct drm_gp2d_gem_mmap   gem_mmap;

          gem_create.size  = GP2DGFX_BUFFER_SIZE;
          gem_create.flags = 0;

          ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_GEM_CREATE, &gem_create );
          if (ret) {
               D_PERROR( "GP2D: DRM_IOCTL_GP2D_GEM_CREATE failed!\n" );
               goto error;
          }

          buffer->handle = gem_create.handle;
          buffer->size   = gem_create.size;

          D_MAGIC_SET( buffer, GP2DBuffer );

          direct_list_append( &gdrv->free, &buffer->link );


          gem_mmap.handle = gem_create.handle;
          gem_mmap.size   = gem_create.size;

          ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_GEM_MMAP, &gem_mmap );
          if (ret) {
               D_PERROR( "GP2D: DRM_IOCTL_GP2D_GEM_MMAP failed!\n" );
               goto error;
          }

          buffer->mapped = (void*)(long) gem_mmap.mapped;
     }

     return DFB_OK;

error:
     // TODO: free buffers

//     gem_close.handle = gem_create.handle;
//
//     ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GEM_CLOSE, &gem_close );
//     if (ret) {
//          D_PERROR( "GP2D: DRM_IOCTL_GEM_CLOSE failed!\n" );
//          return;
//     }
     return ret;
}

GP2DBuffer *
gp2d_get_buffer( GP2DDriverData *gdrv,
                 unsigned int    size )
{
     DFBResult   ret;
     GP2DBuffer *buffer;

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     if (size > GP2DGFX_BUFFER_SIZE) {
          ret = gp2d_create_buffer( gdrv, size, &buffer );
          if (ret) {
               D_DERROR( ret, "GP2D/Buffer: gp2d_create_buffer( %zu ) failed!\n", size );
               return NULL;
          }

          return DFB_OK;
     }

     direct_mutex_lock( &gdrv->buffer_lock );

     while (!gdrv->free)
          direct_waitqueue_wait( &gdrv->buffer_wq, &gdrv->buffer_lock );

     buffer = (GP2DBuffer*) gdrv->free;
     D_MAGIC_ASSERT( buffer, GP2DBuffer );

     direct_list_remove( &gdrv->free, gdrv->free );

     direct_mutex_unlock( &gdrv->buffer_lock );

     return buffer;
}

DFBResult
gp2d_create_buffer( GP2DDriverData  *gdrv,
                    unsigned int     size,
                    GP2DBuffer     **ret_buffer )
{
     int                         ret;
     struct drm_gp2d_gem_create  gem_create;
     struct drm_gp2d_gem_mmap    gem_mmap;
     struct drm_gem_close        gem_close;
     GP2DBuffer                 *buffer;

     D_DEBUG_AT( GP2D_BLT, "%s( %u )\n", __FUNCTION__, size );

     buffer = D_CALLOC( 1, sizeof(GP2DBuffer) );
     if (!buffer)
          return D_OOM();

     gem_create.size  = size;
     gem_create.flags = 0;

     ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_GEM_CREATE, &gem_create );
     if (ret) {
          ret = errno2result( errno );
          D_PERROR( "GP2D: DRM_IOCTL_GP2D_GEM_CREATE failed!\n" );
          goto error_create;
     }

     buffer->handle = gem_create.handle;
     buffer->size   = gem_create.size;

     D_MAGIC_SET( buffer, GP2DBuffer );

     direct_list_append( &gdrv->free, &buffer->link );


     gem_mmap.handle = gem_create.handle;
     gem_mmap.size   = gem_create.size;

     ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_GEM_MMAP, &gem_mmap );
     if (ret) {
          ret = errno2result( errno );
          D_PERROR( "GP2D: DRM_IOCTL_GP2D_GEM_MMAP failed!\n" );
          goto error_mmap;
     }

     buffer->mapped = (void*)(long) gem_mmap.mapped;

     *ret_buffer = buffer;

     return DFB_OK;


error_mmap:
     gem_close.handle = gem_create.handle;

     if (drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GEM_CLOSE, &gem_close ))
          D_PERROR( "GP2D: DRM_IOCTL_GEM_CLOSE failed!\n" );

error_create:
     D_FREE( buffer );

     return ret;
}

void
gp2d_put_buffer( GP2DDriverData *gdrv,
                 GP2DBuffer     *buffer )
{
     D_DEBUG_AT( GP2D_BLT, "%s( buffer %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, GP2DBuffer );

     if (buffer->size == GP2DGFX_BUFFER_SIZE) {
          direct_mutex_lock( &gdrv->buffer_lock );

          direct_list_remove( &gdrv->emitted, &buffer->link );
          direct_list_append( &gdrv->free, &buffer->link );

          buffer->used = 0;
          buffer->task = NULL;

          direct_waitqueue_broadcast( &gdrv->buffer_wq );

          direct_mutex_unlock( &gdrv->buffer_lock );
     }
     else {
          struct drm_gem_close gem_close;

          gem_close.handle = buffer->handle;

          if (drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GEM_CLOSE, &gem_close ))
               D_PERROR( "GP2D: DRM_IOCTL_GEM_CLOSE failed!\n" );

          D_MAGIC_CLEAR( buffer );

          D_FREE( buffer );
     }
}

DFBResult
gp2d_exec_buffer( GP2DDriverData *gdrv,
                  GP2DBuffer     *buffer )
{
     int                         ret;
     struct drm_gp2d_gem_execute gem_execute;

     D_DEBUG_AT( GP2D_BLT, "%s( buffer %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, GP2DBuffer );
     D_ASSERT( buffer->used <= buffer->size );

     gem_execute.handle    = buffer->handle;
     gem_execute.size      = buffer->used;
     gem_execute.user_data = buffer;

     direct_mutex_lock( &gdrv->buffer_lock );
     direct_list_append( &gdrv->emitted, &buffer->link );
     direct_mutex_unlock( &gdrv->buffer_lock );

     ret = drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_GEM_EXECUTE, &gem_execute );
     if (ret) {
          ret = errno2result( errno );
          D_PERROR( "GP2D: DRM_IOCTL_GP2D_GEM_EXECUTE failed!\n" );

          if (buffer->task)
               Task_DoneFail( buffer->task, ret );

          D_ASSERT( gdrv->emitted != NULL );
          D_ASSERT( buffer == (GP2DBuffer*) gdrv->emitted->prev );

          gp2d_put_buffer( gdrv, buffer );
     }

     return ret;
}

/**********************************************************************************************************************/

static void
flush_prepared( GP2DDriverData *gdrv )
{
     GP2DBuffer *buffer;

     D_DEBUG_AT( GP2D_BLT, "%s() <- prep:%d\n", __FUNCTION__, gdrv->prep_num );

     D_ASSERT( gdrv->current != NULL );
     D_ASSERT( gdrv->prep_num <= (gdrv->current->size - 4) / 4 );

     buffer = gdrv->current;
     D_MAGIC_ASSERT( buffer, GP2DBuffer );

     *(u32*)(buffer->mapped + buffer->used) = 0;
     buffer->used += 4;

     gp2d_exec_buffer( gdrv, buffer );

     gdrv->prep_num = 0;
     gdrv->current  = NULL;
}

static inline __u32 *
start_buffer( GP2DDriverData *gdrv,
              int             space )
{
     unsigned int size = space * 4;

     //if (gdrv->current)
     //     D_DEBUG_AT( GP2D_BLT, "%s( %d ) <- used:%u size:%u\n", __FUNCTION__, space, gdrv->current->used, gdrv->current->size );

     /* Check for space in local buffer. */
     if (gdrv->current && gdrv->current->used + size + 4 > gdrv->current->size) {
          /* Flush local buffer. */
          flush_prepared( gdrv );

          D_ASSERT( gdrv->prep_num == 0 );
          D_ASSERT( gdrv->current == NULL );
     }

     if (!gdrv->current) {
          if (size < GP2DGFX_BUFFER_SIZE)
               size = GP2DGFX_BUFFER_SIZE;

          gdrv->current = gp2d_get_buffer( gdrv, size );
     }

     /* Return next write position. */
     return gdrv->current->mapped + gdrv->prep_num * 4;
}

static inline void
submit_buffer( GP2DDriverData *gdrv,
               int             entries )
{
     D_DEBUG_AT( GP2D_BLT, "%s( %d ) <- used:%u size:%u\n", __FUNCTION__, entries, gdrv->current->used, gdrv->current->size );

     D_ASSERT( gdrv->current != NULL );
     D_ASSERT( gdrv->current->used + entries * 4 + 4 <= gdrv->current->size );
     D_ASSERT( gdrv->prep_num + entries <= (gdrv->current->size - 4) / 4 );

     /* Increment next write position. */
     gdrv->prep_num += entries;

     gdrv->current->used += entries * 4;
}

/**********************************************************************************************************************/

static inline void
gp2d_validate_DEST_CLIP( GP2DDriverData *gdrv,
                         GP2DDeviceData *gdev,
                         CardState      *state )
{
     __u32 *prep = start_buffer( gdrv, 21 );

     D_DEBUG_AT( GP2D_BLT, "%s( fd %lu, pitch %d )\n", __FUNCTION__, state->dst.offset, state->dst.pitch );
     D_DEBUG_AT( GP2D_BLT, "  -> %s\n", ToString_CoreSurfaceAllocation( state->dst.allocation ) );
     D_DEBUG_AT( GP2D_BLT, "  -> clip %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &state->clip ) );

     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_UCLMIR;
     prep[2] = GP2D_XY( state->clip.x1, state->clip.y1 );

     prep[3] = GP2D_OPCODE_WPR;
     prep[4] = GP2D_REG_UCLMAR;
     prep[5] = GP2D_XY( state->clip.x2, state->clip.y2);

     if (gdev->v_flags & DEST) {
          submit_buffer( gdrv, 6 );
     }
     else {
          CoreSurface       *surface = state->destination;
          CoreSurfaceBuffer *buffer  = state->dst.buffer;

          gdev->dst_fd     = state->dst.offset;
          gdev->dst_pitch  = state->dst.pitch;
          gdev->dst_bpp    = DFB_BYTES_PER_PIXEL( buffer->format );
          gdev->dst_index  = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;
          gdev->dst_size   = state->dst.allocation->config.size;
          gdev->mode_32bit = (gdev->dst_bpp > 2);

          gdev->rclr &= ~(GP2D_RCLR_GBM | GP2D_RCLR_DPF);

          gdev->rclr |= GP2D_RCLR_GBM_16BIT;

          if (DFB_PIXELFORMAT_HAS_ALPHA( buffer->format ))
               gdev->rclr |= GP2D_RCLR_DPF_ARGB1555;
          else
               gdev->rclr |= GP2D_RCLR_DPF_RGB16;

          D_DEBUG_AT( GP2D_BLT, "  -> RCLR 0x%08x\n", gdev->rclr );

          /* Set destination start address. */
          prep[ 6] = GP2D_OPCODE_WPR;
          prep[ 7] = GP2D_REG_RSAR;
          prep[ 8] = gdev->dst_fd;

          /* Set destination stride. */
          prep[ 9] = GP2D_OPCODE_WPR;
          prep[10] = GP2D_REG_DSTRR;
          prep[11] = gdev->dst_pitch / gdev->dst_bpp;

          /* Set destination pixelformat in rendering control. */
          prep[12] = GP2D_OPCODE_WPR;
          prep[13] = GP2D_REG_RCLR;
          prep[14] = gdev->rclr;

          /* Set system clipping rectangle. */
          prep[15] = GP2D_OPCODE_WPR;
          prep[16] = GP2D_REG_SCLMAR;
          prep[17] = GP2D_XY( surface->config.size.w - 1, surface->config.size.h - 1 );

          /* Set system clipping rectangle. */
          prep[18] = GP2D_OPCODE_WPR;
          prep[19] = GP2D_REG_MD0R;
          prep[20] = DFB_BYTES_PER_PIXEL(buffer->format) > 2 ? GP2D_MS0R_GBM2_ARGB : GP2D_MS0R_GBM2_GBM;

          submit_buffer( gdrv, 21 );
     }

     /* Set the flags. */
     GP2D_VALIDATE( DEST_CLIP );
}

static inline void
gp2d_validate_ALPHA_DRAW( GP2DDriverData *gdrv,
                          GP2DDeviceData *gdev,
                          CardState      *state )
{
     __u32 *prep = start_buffer( gdrv, 9 );

     D_DEBUG_AT( GP2D_BLT, "%s( color %02x %02x %02x %02x )\n", __FUNCTION__,
                 state->color.a, state->color.r, state->color.g, state->color.b );

     gdev->rclr &= ~(GP2D_RCLR_AVALUE | GP2D_RCLR_SAU);

     if (state->color.a >= 0x80)
          gdev->rclr |= GP2D_RCLR_AVALUE_1;
     else
          gdev->rclr |= GP2D_RCLR_AVALUE_0;

     gdev->rclr |= GP2D_RCLR_SAU_AVALUE;

     D_DEBUG_AT( GP2D_BLT, "  -> RCLR 0x%08x\n", gdev->rclr );

     /* Set rendering control. */
     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_RCLR;
     prep[2] = gdev->rclr;

     prep[3] = GP2D_OPCODE_WPR;
     prep[4] = GP2D_REG_ALPHR;
     prep[5] = state->color.a;

     prep[6] = GP2D_OPCODE_WPR;
     prep[7] = GP2D_REG_AVALUE8R;
     prep[8] = state->color.a;

     submit_buffer( gdrv, 9 );

     /* Set the flags. */
     GP2D_VALIDATE( ALPHA_DRAW );
     GP2D_INVALIDATE( ALPHA_BLIT );
}

static inline void
gp2d_validate_ALPHA_BLIT( GP2DDriverData *gdrv,
                          GP2DDeviceData *gdev,
                          CardState      *state )
{
     __u32 *prep = start_buffer( gdrv, 9 );

     D_DEBUG_AT( GP2D_BLT, "%s( color %02x %02x %02x %02x )\n", __FUNCTION__,
                 state->color.a, state->color.r, state->color.g, state->color.b );

     gdev->rclr &= ~GP2D_RCLR_SAU;
     gdev->rclr |= GP2D_RCLR_SAU_SOURCE;



     gdev->rclr &= ~(GP2D_RCLR_AVALUE | GP2D_RCLR_SAU);

     gdev->rclr |= GP2D_RCLR_AVALUE_0;

     gdev->rclr |= GP2D_RCLR_SAU_AVALUE;


     D_DEBUG_AT( GP2D_BLT, "  -> RCLR 0x%08x\n", gdev->rclr );

     /* Set rendering control. */
     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_RCLR;
     prep[2] = gdev->rclr;

     prep[3] = GP2D_OPCODE_WPR;
     prep[4] = GP2D_REG_ALPHR;
     prep[5] = state->color.a;

     prep[6] = GP2D_OPCODE_WPR;
     prep[7] = GP2D_REG_AVALUE8R;
     prep[8] = 0xff;

     submit_buffer( gdrv, 9 );

     /* Set the flags. */
     GP2D_VALIDATE( ALPHA_BLIT );
     GP2D_INVALIDATE( ALPHA_DRAW );
}

static inline void
gp2d_validate_COLOR( GP2DDriverData *gdrv,
                     GP2DDeviceData *gdev,
                     CardState      *state )
{
     __u32 *prep = start_buffer( gdrv, 3 );

     D_DEBUG_AT( GP2D_BLT, "%s( color %02x %02x %02x %02x )\n", __FUNCTION__,
                 state->color.a, state->color.r, state->color.g, state->color.b );

     gdev->color_bits = dfb_pixel_from_color( state->destination->config.format, &state->color );

     D_DEBUG_AT( GP2D_BLT, "  -> bits 0x%08x\n", gdev->color_bits );

     gdev->rclr &= ~GP2D_RCLR_SPF;

     if (DFB_PIXELFORMAT_HAS_ALPHA( state->destination->config.format ))
          gdev->rclr |= GP2D_RCLR_SPF_ARGB1555;
     else
          gdev->rclr |= GP2D_RCLR_SPF_RGB16;

     D_DEBUG_AT( GP2D_BLT, "  -> RCLR 0x%08x\n", gdev->rclr );

     /* Set source pixelformat in rendering control. */
     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_RCLR;
     prep[2] = gdev->rclr;

     submit_buffer( gdrv, 3 );

     /* Set the flags. */
     GP2D_VALIDATE( COLOR );
     GP2D_INVALIDATE( SOURCE );
}

static inline void
gp2d_validate_SOURCE( GP2DDriverData *gdrv,
                      GP2DDeviceData *gdev,
                      CardState      *state )
{
     __u32             *prep   = start_buffer( gdrv, 9 );
     CoreSurfaceBuffer *buffer = state->src.buffer;

     D_DEBUG_AT( GP2D_BLT, "%s( fd %lu, pitch %d )\n", __FUNCTION__, state->src.offset, state->src.pitch );
     D_DEBUG_AT( GP2D_BLT, "  -> %s\n", ToString_CoreSurfaceAllocation( state->src.allocation ) );

     gdev->rclr &= ~GP2D_RCLR_SPF;

     if (DFB_PIXELFORMAT_HAS_ALPHA( buffer->format ))
          gdev->rclr |= GP2D_RCLR_SPF_ARGB1555;
     else
          gdev->rclr |= GP2D_RCLR_SPF_RGB16;

     D_DEBUG_AT( GP2D_BLT, "  -> RCLR 0x%08x\n", gdev->rclr );

     /* Set source start address. */
     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_SSAR;
     prep[2] = gdev->src_fd;

     /* Set source stride. */
     prep[3] = GP2D_OPCODE_WPR;
     prep[4] = GP2D_REG_SSTRR;
     prep[5] = gdev->src_pitch / gdev->src_bpp;

     /* Set source pixelformat in rendering control. */
     prep[6] = GP2D_OPCODE_WPR;
     prep[7] = GP2D_REG_RCLR;
     prep[8] = gdev->rclr;

     submit_buffer( gdrv, 9 );

     /* Set the flags. */
     GP2D_VALIDATE( SOURCE );
     GP2D_INVALIDATE( COLOR );
}

static inline void
gp2d_validate_STRANS( GP2DDriverData *gdrv,
                      GP2DDeviceData *gdev,
                      CardState      *state )
{
     __u32 *prep = start_buffer( gdrv, 3 );

     D_DEBUG_AT( GP2D_BLT, "%s( src_colorkey 0x%08x )\n", __FUNCTION__, state->src_colorkey );

     prep[0] = GP2D_OPCODE_WPR;
     prep[1] = GP2D_REG_STCR;
     prep[2] = state->src_colorkey;

     submit_buffer( gdrv, 3 );

     /* Set the flags. */
     GP2D_VALIDATE( STRANS );
}

static inline void
gp2d_validate_MATRIX( GP2DDriverData *gdrv,
                      GP2DDeviceData *gdev,
                      CardState      *state )
{
     D_DEBUG_AT( GP2D_BLT, "%s( options 0x%08x )\n", __FUNCTION__, state->render_options );

     if (state->render_options & DSRO_MATRIX) {
          __u32 *prep = start_buffer( gdrv, 14 );
          float  m[9];
          int    i;

          for (i=0; i<9; i++) {
               m[i] = (state->matrix[i] / 65536.0f);

               D_DEBUG_AT( GP2D_BLT, "  -> [%d] %10.5f\n", i, m[i] );
          }

          prep[0] = GP2D_OPCODE_WPR;
          prep[1] = GP2D_REG_MTRAR | (8 << 16);

          direct_memcpy( &prep[2], m, 4 * 9 );

          prep[11] = GP2D_OPCODE_WPR;
          prep[12] = GP2D_REG_GTRCR;
          prep[13] = GP2D_GTRCR_GTE | GP2D_GTRCR_AFE;

          submit_buffer( gdrv, 14 );
     }
     else {
          __u32 *prep = start_buffer( gdrv, 3 );

          prep[0] = GP2D_OPCODE_WPR;
          prep[1] = GP2D_REG_GTRCR;
          prep[2] = 0;

          submit_buffer( gdrv, 3 );
     }

     /* Set the flags. */
     GP2D_VALIDATE( MATRIX );
}

/**********************************************************************************************************************/

DFBResult
gp2dEngineSync( void *drv, void *dev )
{
     DFBResult       ret  = DFB_OK;
     GP2DDriverData *gdrv = drv;

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     while (drmIoctl( gdrv->gfx_fd, DRM_IOCTL_GP2D_WAITIDLE, NULL ) < 0) {
          if (errno == EINTR)
               continue;

          ret = errno2result( errno );
          D_PERROR( "GP2D/BLT: GP2DGFX_IOCTL_WAIT_IDLE failed!\n" );

          break;
     }

     return ret;
}

void
gp2dEngineReset( void *drv, void *dev )
{
     GP2DDriverData *gdrv = drv;
     __u32            *prep;

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     prep = start_buffer( gdrv, 4 );

     /* Reset current pointer. */
     prep[0] = GP2D_OPCODE_MOVE;
     prep[1] = 0;

     /* Reset local offset. */
     prep[2] = GP2D_OPCODE_LCOFS;
     prep[3] = 0;

     submit_buffer( gdrv, 4 );
}

void
gp2dEmitCommands( void *drv, void *dev )
{
     GP2DDriverData *gdrv = drv;

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     if (gdrv->prep_num)
          flush_prepared( gdrv );
}

void
gp2dFlushTextureCache( void *drv, void *dev )
{
     GP2DDriverData *gdrv = drv;
     __u32            *prep = start_buffer( gdrv, 1 );

     D_DEBUG_AT( GP2D_BLT, "%s()\n", __FUNCTION__ );

     prep[0] = GP2D_OPCODE_SYNC | GP2D_SYNC_TCLR;

     submit_buffer( gdrv, 1 );
}

/**********************************************************************************************************************/

void
gp2dCheckState( void                *drv,
                void                *dev,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     D_DEBUG_AT( GP2D_BLT, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(GP2D_SUPPORTED_DRAWINGFUNCTIONS | GP2D_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_ARGB1555:
               break;

          default:
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported destination format '%s'\n",
                           dfb_pixelformat_name(state->destination->config.format) );
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~GP2D_SUPPORTED_DRAWINGFLAGS) {
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported drawing flags '%s'\n",
                           ToString_DFBSurfaceDrawingFlags( state->drawingflags & ~GP2D_SUPPORTED_DRAWINGFLAGS ) );
               return;
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               // FIXME: get better documentation
               /* Return if the destination format is not supported. */
               switch (state->destination->config.format) {
                    case DSPF_RGB16:
                    case DSPF_RGB32:
                         break;

                    default:
                         D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                         return;
               }

               switch (accel) {
                    case DFXL_FILLRECTANGLE:
                    case DFXL_FILLTRIANGLE:
                         break;
                    default:
                         D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                         return;
               }

               /* Return if blending with unsupported blend functions is requested. */
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }

               /* XOR only without blending. */
               if (state->drawingflags & DSDRAW_XOR) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }
          }

          /* Enable acceleration of drawing functions. */
          state->accel |= accel;
     } else {
          DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~GP2D_SUPPORTED_BLITTINGFLAGS) {
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported blitting flags '%s'\n",
                           ToString_DFBSurfaceDrawingFlags( flags & ~GP2D_SUPPORTED_BLITTINGFLAGS ) );
               return;
          }

          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB1555:
               case DSPF_RGB16:
                    if (DFB_BYTES_PER_PIXEL( state->destination->config.format ) != 2) {
                         D_DEBUG_AT( GP2D_BLT, "  -> %d (%d:%s)\n", __LINE__,
                                     DFB_BYTES_PER_PIXEL( state->destination->config.format ),
                                     dfb_pixelformat_name(state->destination->config.format) );
                         return;
                    }
                    break;

               case DSPF_RGB32:
               case DSPF_ARGB:
                    if (DFB_BYTES_PER_PIXEL( state->destination->config.format ) != 4) {
                         D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                         return;
                    }
               case DSPF_A8:
                    break;

               default:
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported source format '%s'\n",
                                dfb_pixelformat_name(state->source->config.format) );
                    return;
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               // FIXME: get better documentation
               /* Return if the destination format is not supported. */
               switch (state->destination->config.format) {
                    case DSPF_RGB16:
                    case DSPF_RGB32:
                         break;

                    default:
                         D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                         return;
               }

               switch (state->source->config.format) {
                    case DSPF_ARGB1555:
                    case DSPF_RGB16:
                    case DSPF_A8:
                         break;

                    default:
                         D_DEBUG_AT( GP2D_BLT, "  -> unsupported source format '%s' for blending\n",
                                     dfb_pixelformat_name(state->source->config.format) );
                         return;
               }

               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & DSBLIT_COLORIZE) {
               if (state->source->config.format != DSPF_A8) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }

               if (!(state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }

               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
                    D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
                    return;
               }
          }

          /* XOR only without blending etc. */
          if (flags & DSBLIT_XOR &&
              flags & ~(DSBLIT_SRC_COLORKEY | DSBLIT_ROTATE180 | DSBLIT_XOR)) {
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
               return;
          }

          /* Return if colorizing for non-font surfaces is requested. */
          if ((flags & DSBLIT_COLORIZE) && !(state->source->type & CSTF_FONT)) {
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
               return;
          }

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( flags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               D_DEBUG_AT( GP2D_BLT, "  -> unsupported (line %d)\n", __LINE__ );
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
gp2dSetState( void                *drv,
              void                *dev,
              GraphicsDeviceFuncs *funcs,
              CardState           *state,
              DFBAccelerationMask  accel )
{
     GP2DDriverData       *gdrv     = drv;
     GP2DDeviceData       *gdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( GP2D_BLT, "%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          GP2D_INVALIDATE( ALL );
     } else if (modified) {
          /* Invalidate matrix. */
          if (modified & (SMF_MATRIX | SMF_RENDER_OPTIONS))
               GP2D_INVALIDATE( MATRIX );

          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               GP2D_INVALIDATE( DEST | COLOR );

          /* Invalidate clipping registers. */
          if (modified & SMF_CLIP)
               GP2D_INVALIDATE( CLIP );

          /* Invalidate color registers. */
          if (modified & SMF_COLOR)
               GP2D_INVALIDATE( ALPHA_DRAW | ALPHA_BLIT | COLOR );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               GP2D_INVALIDATE( SOURCE );

          /* Invalidate source colorkey. */
          if (modified & SMF_SRC_COLORKEY)
               GP2D_INVALIDATE( STRANS );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination and clip. */
     GP2D_CHECK_VALIDATE( DEST_CLIP );

     /* Use transformation matrix? */
     GP2D_CHECK_VALIDATE( MATRIX );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWLINE:
               /* ...require valid color. */
               GP2D_CHECK_VALIDATE( COLOR );

               /* If blending is used, validate the alpha value. */
//               if (state->drawingflags & DSDRAW_BLEND)
                    GP2D_CHECK_VALIDATE( ALPHA_DRAW );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = GP2D_SUPPORTED_DRAWINGFUNCTIONS;

               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               if (state->source) {
                    CoreSurfaceBuffer *buffer = state->src.buffer;

                    gdev->src_addr   = state->src.addr;
                    gdev->src_fd     = state->src.offset;
                    gdev->src_pitch  = state->src.pitch;
                    gdev->src_bpp    = DFB_BYTES_PER_PIXEL( buffer->format );
                    gdev->src_index  = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;
                    gdev->src_size   = state->src.allocation->config.size;
                    gdev->src_format = buffer->format;
               }

               /* If colorize is used, validate the color value. */
               if (state->blittingflags & DSBLIT_COLORIZE)
                    GP2D_CHECK_VALIDATE( COLOR );
               else
                    /* ...require valid source. */
                    GP2D_CHECK_VALIDATE( SOURCE );

               /* If blending is used, validate the alpha value. */
//               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                    GP2D_CHECK_VALIDATE( ALPHA_BLIT );

               /* If colorkeying is used, validate the colorkey. */
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    GP2D_CHECK_VALIDATE( STRANS );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = GP2D_SUPPORTED_BLITTINGFUNCTIONS;

               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;

     }

     gdev->dflags         = state->drawingflags;
     gdev->bflags         = state->blittingflags;
     gdev->render_options = state->render_options;
     gdev->color          = state->color;

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

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
gp2dFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );

     if (gdev->render_options & DSRO_MATRIX) {
          __u32 *prep = start_buffer( gdrv, 6 );

          int x1, x2, y1, y2;

          x1 = rect->x;
          y1 = rect->y;
          x2 = rect->x + rect->w - 1;
          y2 = rect->y + rect->h - 1;

          prep[0] = GP2D_OPCODE_POLYGON_4C | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE | GP2D_DRAWMODE_BLKE;

          if (gdev->dflags & DSDRAW_BLEND)
               prep[0] |= GP2D_DRAWMODE_ALPHA;

          prep[1] = gdev->color_bits;

          prep[2] = GP2D_XY( x1, y1 );
          prep[3] = GP2D_XY( x2, y1 );
          prep[4] = GP2D_XY( x2, y2 );
          prep[5] = GP2D_XY( x1, y2 );

          submit_buffer( gdrv, 6 );
     }
     else {
          __u32 *prep = start_buffer( gdrv, 6 );

          prep[0] = GP2D_OPCODE_BITBLTC | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE | GP2D_DRAWMODE_BLKE;

          if (gdev->dflags & DSDRAW_BLEND)
               prep[0] |= GP2D_DRAWMODE_ALPHA;

          prep[1] = 0xcc;
          prep[2] = gdev->color_bits;
          prep[3] = rect->w - 1;
          prep[4] = rect->h - 1;
          prep[5] = GP2D_XY( rect->x, rect->y );

          submit_buffer( gdrv, 6 );
     }

     return true;
}

/**********************************************************************************************************************/

/*
 * Render rectangle outlines using the current hardware state.
 */
bool
gp2dDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;
     __u32            *prep = start_buffer(gdrv, gdev->mode_32bit ? 9 : 8 );

     int x1, x2, y1, y2;

     x1 = rect->x;
     y1 = rect->y;
     x2 = rect->x + rect->w - 1;
     y2 = rect->y + rect->h - 1;

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );

     prep[0] = GP2D_OPCODE_LINE_C | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE;

     if (gdev->dflags & DSDRAW_BLEND)
          prep[0] |= GP2D_DRAWMODE_ALPHA;

     if (gdev->mode_32bit) {
          prep[1] = gdev->color_bits;
          prep[2] = 5;
          prep[3] = 0;

          prep[4] = GP2D_XY( x1, y1 );
          prep[5] = GP2D_XY( x2, y1 );
          prep[6] = GP2D_XY( x2, y2 );
          prep[7] = GP2D_XY( x1, y2 );
          prep[8] = GP2D_XY( x1, y1 );

          submit_buffer( gdrv, 9 );
     }
     else {
          prep[1] = (gdev->color_bits << 16 ) | 5;
          prep[2] = 0;

          prep[3] = GP2D_XY( x1, y1 );
          prep[4] = GP2D_XY( x2, y1 );
          prep[5] = GP2D_XY( x2, y2 );
          prep[6] = GP2D_XY( x1, y2 );
          prep[7] = GP2D_XY( x1, y1 );

          submit_buffer( gdrv, 8 );
     }

     return true;
}

/**********************************************************************************************************************/

/*
 * Render a triangle using the current hardware state.
 */
bool
gp2dFillTriangle( void *drv, void *dev, DFBTriangle *triangle )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;
     __u32            *prep = start_buffer( gdrv, 6 );

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %dx, %d - %d, %d )\n", __FUNCTION__,
                 DFB_TRIANGLE_VALS( triangle ) );

     prep[0] = GP2D_OPCODE_POLYGON_4C | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE | GP2D_DRAWMODE_BLKE;

     if (gdev->dflags & DSDRAW_BLEND)
          prep[0] |= GP2D_DRAWMODE_ALPHA;

     prep[1] = gdev->color_bits;

     prep[2] = GP2D_XY( triangle->x1, triangle->y1 );
     prep[3] = GP2D_XY( triangle->x2, triangle->y2 );
     prep[4] = GP2D_XY( triangle->x3, triangle->y3 );
     prep[5] = GP2D_XY( triangle->x3, triangle->y3 );

     submit_buffer( gdrv, 6 );

     /*
      * TODO: use rlined to draw the aa'ed outline of a polygon
      *       if also aval, set blke to 1
      */



     return true;
}

/**********************************************************************************************************************/

/*
 * Render a line with the specified width using the current hardware state.
 */
bool
gp2dDrawLine( void *drv, void *dev, DFBRegion *line )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;
     __u32            *prep = start_buffer(gdrv, gdev->mode_32bit ? 6 : 5 );

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %d, %d )\n", __FUNCTION__,
                 line->x1, line->y1, line->x2, line->y2 );

     prep[0] = GP2D_OPCODE_LINE_C | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE;

     if (gdev->render_options & DSRO_ANTIALIAS)
          prep[0] |= GP2D_DRAWMODE_ANTIALIAS;

     if (gdev->mode_32bit) {
          prep[1] = gdev->color_bits;
          prep[2] = 2;
          prep[3] = 0;

          prep[4] = GP2D_XY( line->x1, line->y1 );
          prep[5] = GP2D_XY( line->x2, line->y2 );

          submit_buffer( gdrv, 6 );
     }
     else {
          prep[1] = (gdev->color_bits << 16) | 2;
          prep[2] = 0;

          prep[3] = GP2D_XY( line->x1, line->y1 );
          prep[4] = GP2D_XY( line->x2, line->y2 );

          submit_buffer( gdrv, 5);
     }

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
gp2dBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %dx%d <- %d, %d )\n", __FUNCTION__,
                 dx, dy, rect->w, rect->h, rect->x, rect->y );

     if (gdev->src_format == DSPF_A8) {
          int    w       = ((rect->w + 7) & ~7);
          int    cmd_num = 8 + (gdrv->prep_num & 1);
          int    map_num = w / 4 * rect->h;
          __u32 *prep    = start_buffer( gdrv, cmd_num + map_num + 1 );
          int    y;

          /* workaround case where start_buffer restarts with a new buffer (prep_num changes) */
          cmd_num = 8 + (gdrv->prep_num & 1);

          D_DEBUG_AT( GP2D_BLT, "  -> AAFC  color:0x%08x src:%d/%p\n", gdev->color_bits, gdev->src_fd, gdev->src_addr );

          prep[0] = GP2D_OPCODE_AAFC | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE | GP2D_DRAWMODE_REL;

          prep[1] = gdev->color_bits;
          prep[2] = cmd_num * 4;
          prep[3] = w - 1;
          prep[4] = rect->h - 1;
          prep[5] = GP2D_XY( dx, dy );

          prep[6] = GP2D_OPCODE_JUMP | GP2D_DRAWMODE_REL;
          prep[7] = (2 + (cmd_num & 1) + map_num) * 4;

          D_ASSERT( !((gdrv->prep_num * 4 + prep[2]) & 7) );

          for (y=0; y<rect->h; y++) {
               direct_memcpy( &prep[cmd_num] + w / 4 * y,
                              gdev->src_addr + rect->x + (rect->y + y) * gdev->src_pitch, w );
          }

          submit_buffer( gdrv, cmd_num + map_num );

          return true;
     }

     if ((gdev->render_options & DSRO_MATRIX) || (gdev->bflags & (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90))) {
          __u32 *prep = start_buffer( gdrv, 8 );

          int x1, x2, y1, y2;

          x1 = dx;
          y1 = dy;
          x2 = dx + rect->w - 1;
          y2 = dy + rect->h - 1;

          if (gdev->bflags & DSBLIT_FLIP_HORIZONTAL)
               D_UTIL_SWAP( x1, x2 );

          if (gdev->bflags & DSBLIT_FLIP_VERTICAL)
               D_UTIL_SWAP( y1, y2 );

          prep[0] = GP2D_OPCODE_POLYGON_4A | GP2D_DRAWMODE_SS | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE;

          if (gdev->bflags & DSBLIT_BLEND_COLORALPHA)
               prep[0] |= GP2D_DRAWMODE_ALPHA;

          if (gdev->bflags & DSBLIT_SRC_COLORKEY)
               prep[0] |= GP2D_DRAWMODE_STRANS;

          prep[1] = GP2D_XY( 0, 0 );
          prep[2] = GP2D_XY( rect->w, rect->h );
          prep[3] = GP2D_XY( rect->x, rect->y );

          if (gdev->bflags & DSBLIT_ROTATE90) {
               prep[4] = GP2D_XY( x1, y2 );
               prep[5] = GP2D_XY( x1, y1 );
               prep[6] = GP2D_XY( x2, y1 );
               prep[7] = GP2D_XY( x2, y2 );
          }
          else {
               prep[4] = GP2D_XY( x1, y1 );
               prep[5] = GP2D_XY( x2, y1 );
               prep[6] = GP2D_XY( x2, y2 );
               prep[7] = GP2D_XY( x1, y2 );
          }

          submit_buffer( gdrv, 8 );
     }
     else {
          __u32 *prep = start_buffer( gdrv, 6 );

          prep[0] = GP2D_OPCODE_BITBLTA | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE;

          if (gdev->bflags & DSBLIT_BLEND_COLORALPHA)
               prep[0] |= GP2D_DRAWMODE_ALPHA;

          if (gdev->bflags & DSBLIT_SRC_COLORKEY)
               prep[0] |= GP2D_DRAWMODE_STRANS;

          if (gdev->src_fd == gdev->dst_fd) {
               if (dy > rect->y)
                    prep[0] |= GP2D_DRAWMODE_DSTDIR_Y | GP2D_DRAWMODE_SRCDIR_Y;
               else if (dy == rect->y) {
                    if (dx > rect->x)
                         prep[0] |= GP2D_DRAWMODE_DSTDIR_X | GP2D_DRAWMODE_SRCDIR_X;
               }
          }

          prep[1] = 0xcc;
          prep[2] = GP2D_XY( rect->x, rect->y );
          prep[3] = rect->w - 1;
          prep[4] = rect->h - 1;
          prep[5] = GP2D_XY( dx, dy );

          submit_buffer( gdrv, 6 );
     }

     return true;
}

/*
 * StretchBlit a rectangle using the current hardware state.
 */
bool
gp2dStretchBlit( void *drv, void *dev,
                 DFBRectangle *srect,
                 DFBRectangle *drect )
{
     GP2DDriverData *gdrv = drv;
     GP2DDeviceData *gdev = dev;
     __u32            *prep = start_buffer( gdrv, 8 );

     int x1, x2, y1, y2;

     x1 = drect->x;
     y1 = drect->y;
     x2 = drect->x + drect->w - 1;
     y2 = drect->y + drect->h - 1;

     D_DEBUG_AT( GP2D_BLT, "%s( %d, %d - %dx%d <- %d, %d - %dx%d )\n", __FUNCTION__,
                 drect->x, drect->y, drect->w, drect->h,
                 srect->x, srect->y, srect->w, srect->h );

     prep[0] = GP2D_OPCODE_POLYGON_4A | GP2D_DRAWMODE_SS | GP2D_DRAWMODE_CLIP | GP2D_DRAWMODE_MTRE;

     if (gdev->bflags & DSBLIT_BLEND_COLORALPHA)
          prep[0] |= GP2D_DRAWMODE_ALPHA;

     if (gdev->bflags & DSBLIT_SRC_COLORKEY)
          prep[0] |= GP2D_DRAWMODE_STRANS;

     prep[1] = GP2D_XY( 0, 0 );
     prep[2] = GP2D_XY( srect->w, srect->h );
     prep[3] = GP2D_XY( srect->x, srect->y );
     prep[4] = GP2D_XY( x1, y1 );
     prep[5] = GP2D_XY( x2, y1 );
     prep[6] = GP2D_XY( x2, y2 );
     prep[7] = GP2D_XY( x1, y2 );

     submit_buffer( gdrv, 8 );

     return true;
}

