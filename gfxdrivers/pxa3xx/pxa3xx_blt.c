#ifdef PXA3XX_DEBUG_BLT
#define DIRECT_ENABLE_DEBUG
#endif


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

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>

#include "pxa3xx.h"
#include "pxa3xx_blt.h"


D_DEBUG_DOMAIN( PXA3XX_BLT, "PXA3XX/BLT", "Marvell PXA3xx Blit" );

/**********************************************************************************************************************/

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DEST         = 0x00000001,
     SOURCE       = 0x00000002,
     COLOR        = 0x00000004,

     ALL          = 0x00000007
};

/*
 * Map pixel formats.
 */
static int pixel_formats[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)  ] =  1,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)   ] =  3,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)   ] =  4,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)   ] =  6,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)] =  7,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)    ] =  9,
};

/*
 * State handling macros.
 */

#define PXA3XX_VALIDATE(flags)          do { pdev->v_flags |=  (flags); } while (0)
#define PXA3XX_INVALIDATE(flags)        do { pdev->v_flags &= ~(flags); } while (0)

#define PXA3XX_CHECK_VALIDATE(flag)     do {                                                        \
                                             if ((pdev->v_flags & flag) != flag)                    \
                                                  pxa3xx_validate_##flag( pdrv, pdev, state );      \
                                        } while (0)

#define DUMP_INFO() D_DEBUG_AT( PXA3XX_BLT, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                            pdrv->gfx_shared->hw_running ? "" : "not ",             \
                                            pdrv->gfx_shared->hw_start,                             \
                                            pdrv->gfx_shared->hw_end,                               \
                                            pdrv->gfx_shared->next_start,                           \
                                            pdrv->gfx_shared->next_end,                             \
                                            pdrv->gfx_shared->next_valid ? "" : "not " );

/**********************************************************************************************************************/

static inline bool
check_blend_functions( const CardState *state )
{
     switch (state->src_blend) {
          case DSBF_SRCALPHA:
               break;

          default:
               return false;
     }

     switch (state->dst_blend) {
          case DSBF_INVSRCALPHA:
               break;

          default:
               return false;
     }

     return true;
}

/**********************************************************************************************************************/

#define PXA_GCU_REG(x)        (*(volatile u32*)(pdrv->mmio_base+(x)))

#define PXA_GCRBBR           PXA_GCU_REG(0x020)
#define PXA_GCRBLR           PXA_GCU_REG(0x024)
#define PXA_GCRBTR           PXA_GCU_REG(0x02C)

#define mb() __asm__ __volatile__ ("" : : : "memory")

static inline DFBResult
start_hardware( PXA3XXDriverData *pdrv )
{
     PXA3XXGfxSharedArea *shared = pdrv->gfx_shared;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     D_ASSERT( shared->next_valid );

     if (shared->hw_running || shared->next_end == shared->next_start)
          return DFB_FAILURE;

     shared->hw_running = true;
     shared->hw_start   = shared->next_start;
     shared->hw_end     = shared->next_end;

     shared->next_start = shared->next_end = (shared->hw_end + 63) & ~0x3f;

     shared->num_words += shared->hw_end - shared->hw_start;

     shared->num_starts++;

     DUMP_INFO();

     D_ASSERT( shared->buffer[shared->hw_end] == 0x08000000 );

#ifdef PXA3XX_GCU_REG_USE_IOCTLS
     ioctl( pdrv->gfx_fd, PXA3XX_GCU_IOCTL_START );
#else
     mb();

     PXA_GCRBLR = 0;

     PXA_GCRBBR = shared->buffer_phys + shared->hw_start*4;
     PXA_GCRBTR = shared->buffer_phys + shared->hw_end*4;
     PXA_GCRBLR = ((shared->hw_end - shared->hw_start + 63) & ~0x3f) * 4;

     mb();
#endif

     return DFB_OK;
}

__attribute__((noinline))
static DFBResult
flush_prepared( PXA3XXDriverData *pdrv )
{
     PXA3XXGfxSharedArea *shared  = pdrv->gfx_shared;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     D_ASSERT( pdrv->prep_num < PXA3XX_GCU_BUFFER_WORDS );
     D_ASSERT( pdrv->prep_num <= D_ARRAY_SIZE(pdrv->prep_buf) );

     /* Something prepared? */
     while (pdrv->prep_num) {
          int timeout = 2;
          int next_end;

          /* Mark shared information as invalid. From this point on the interrupt handler
           * will not continue with the next block, and we'll start the hardware ourself. */
          shared->next_valid = false;

          mb();

          /* Check if there's enough space at the end.
           * Wait until hardware has started next block before it gets too big. */
          if (shared->next_end + pdrv->prep_num >= PXA3XX_GCU_BUFFER_WORDS ||
              shared->next_end - shared->next_start >= PXA3XX_GCU_BUFFER_WORDS/4)
          {
               /* If there's no next block waiting, start at the beginning. */
               if (shared->next_start == shared->next_end)
                    shared->next_start = shared->next_end = 0;
               else {
                    D_ASSERT( shared->buffer[shared->hw_end] == 0x08000000 );

                    /* Mark area as valid again. */
                    shared->next_valid = true;

                    mb();

                    /* Start in case it got idle while doing the checks. */
                    if (start_hardware( pdrv )) {
                         /*
                          * Hardware has not been started (still running).
                          * Check for timeout. */
                         if (!timeout--) {
                              D_ERROR( "PXA3XX/Blt: Timeout waiting for processing!\n" );
                              direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                                 pdrv->gfx_shared->hw_running ? "" : "not ",             \
                                                 pdrv->gfx_shared->hw_start,                             \
                                                 pdrv->gfx_shared->hw_end,                               \
                                                 pdrv->gfx_shared->next_start,                           \
                                                 pdrv->gfx_shared->next_end,                             \
                                                 pdrv->gfx_shared->next_valid ? "" : "not " );
                              D_ASSERT( shared->buffer[shared->hw_end] == 0x08000000 );
//                              pxa3xxEngineReset( pdrv, pdrv->dev );

                              return DFB_TIMEOUT;
                         }

                         /* Wait til next block is started. */
                         ioctl( pdrv->gfx_fd, PXA3XX_GCU_IOCTL_WAIT_NEXT );
                    }

                    /* Start over with the checks. */
                    continue;
               }
          }

          /* We are appending in case there was already a next block. */
          next_end = shared->next_end + pdrv->prep_num;

          /* Reset the timeout counter. */
          timeout = 20;

          /* While the hardware is running... */
          while (shared->hw_running) {
               D_ASSERT( shared->buffer[shared->hw_end] == 0x08000000 );

               /* ...make sure we don't over lap with its current buffer, otherwise wait. */
               if (shared->hw_start > next_end || shared->hw_end < shared->next_start)
                    break;

               /* Check for timeout. */
               if (!timeout--) {
                    D_ERROR( "PXA3XX/Blt: Timeout waiting for space!\n" );
                    direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                       pdrv->gfx_shared->hw_running ? "" : "not ",             \
                                       pdrv->gfx_shared->hw_start,                             \
                                       pdrv->gfx_shared->hw_end,                               \
                                       pdrv->gfx_shared->next_start,                           \
                                       pdrv->gfx_shared->next_end,                             \
                                       pdrv->gfx_shared->next_valid ? "" : "not " );
                    D_ASSERT( shared->buffer[shared->hw_end] == 0x08000000 );
//                    pxa3xxEngineReset( pdrv, pdrv->dev );

                    return DFB_TIMEOUT;
               }

               /* Wait til next block is started. */
               ioctl( pdrv->gfx_fd, PXA3XX_GCU_IOCTL_WAIT_NEXT );
          }

          /* Copy from local to shared buffer. */
          direct_memcpy( (void*) &shared->buffer[shared->next_end], &pdrv->prep_buf[0], pdrv->prep_num * sizeof(u32) );

          /* Terminate the block. */
          shared->buffer[next_end] = 0x08000000;

          /* Update next block information and mark valid. */
          shared->next_end   = next_end;

          mb();

          shared->next_valid = true;

          /* Reset local counter. */
          pdrv->prep_num = 0;

          /* Start in case it is idle. */
          return start_hardware( pdrv );
     }

     return DFB_OK;
}

static inline u32 *
start_buffer( PXA3XXDriverData *pdrv,
              int               space )
{
     /* Check for space in local buffer. */
     if (pdrv->prep_num + space > PXA3XX_GFX_MAX_PREPARE) {
          /* Flush local buffer. */
          flush_prepared( pdrv );

          D_ASSERT( pdrv->prep_num == 0 );
     }

     /* Return next write position. */
     return &pdrv->prep_buf[pdrv->prep_num];
}

static inline void
submit_buffer( PXA3XXDriverData *pdrv,
               int               entries )
{
     D_ASSERT( pdrv->prep_num + entries <= PXA3XX_GFX_MAX_PREPARE );

     /* Increment next write position. */
     pdrv->prep_num += entries;
}

/**********************************************************************************************************************/

static inline void
pxa3xx_validate_DEST( PXA3XXDriverData *pdrv,
                      PXA3XXDeviceData *pdev,
                      CardState        *state )
{
     CoreSurfaceBuffer *buffer = state->dst.buffer;
     u32               *prep   = start_buffer( pdrv, 6 );

     D_DEBUG_AT( PXA3XX_BLT, "%s( 0x%08lx [%d] )\n", __FUNCTION__,
                 state->dst.phys, state->dst.pitch );

     pdev->dst_phys  = state->dst.phys;
     pdev->dst_pitch = state->dst.pitch;
     pdev->dst_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
     pdev->dst_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

     /* Set destination. */
     prep[0] = 0x020000A2;
     prep[1] = pdev->dst_phys;
     prep[2] = (pixel_formats[pdev->dst_index] << 19) | (pdev->dst_pitch << 5) | pdev->dst_bpp;

     prep[3] = 0x02000012;
     prep[4] = prep[1];
     prep[5] = prep[2];

     submit_buffer( pdrv, 6 );

     /* Set the flags. */
     PXA3XX_VALIDATE( DEST );
}

static inline void
pxa3xx_validate_SOURCE( PXA3XXDriverData *pdrv,
                        PXA3XXDeviceData *pdev,
                        CardState        *state )
{
     CoreSurfaceBuffer *buffer = state->src.buffer;
     u32               *prep   = start_buffer( pdrv, 3 );

     pdev->src_phys  = state->src.phys;
     pdev->src_pitch = state->src.pitch;
     pdev->src_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
     pdev->src_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

     /* Set source. */
     prep[0] = 0x02000002;
     prep[1] = pdev->src_phys;
     prep[2] = (pixel_formats[pdev->src_index] << 19) | (pdev->src_pitch << 5) | pdev->src_bpp;

     submit_buffer( pdrv, 3 );

     /* Set the flag. */
     PXA3XX_VALIDATE( SOURCE );
}

static inline void
pxa3xx_validate_COLOR( PXA3XXDriverData *pdrv,
                       PXA3XXDeviceData *pdev,
                       CardState        *state )
{
     u32 *prep = start_buffer( pdrv, 2 );

     prep[0] = 0x04000011 | (pixel_formats[pdev->dst_index] << 8);
     prep[1] = PIXEL_ARGB( state->color.a,
                           state->color.r,
                           state->color.g,
                           state->color.b );

     submit_buffer( pdrv, 2 );

     /* Set the flag. */
     PXA3XX_VALIDATE( COLOR );
}

/**********************************************************************************************************************/

DFBResult
pxa3xxEngineSync( void *drv, void *dev )
{
     DFBResult            ret    = DFB_OK;
     PXA3XXDriverData    *pdrv   = drv;
     PXA3XXGfxSharedArea *shared = pdrv->gfx_shared;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     while (shared->hw_running && ioctl( pdrv->gfx_fd, PXA3XX_GCU_IOCTL_WAIT_IDLE ) < 0) {
          if (errno == EINTR)
               continue;

          ret = errno2result( errno );
          D_PERROR( "PXA3XX/BLT: PXA3XX_GCU_IOCTL_WAIT_IDLE failed!\n" );

          direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                             pdrv->gfx_shared->hw_running ? "" : "not ",             \
                             pdrv->gfx_shared->hw_start,                             \
                             pdrv->gfx_shared->hw_end,                               \
                             pdrv->gfx_shared->next_start,                           \
                             pdrv->gfx_shared->next_end,                             \
                             pdrv->gfx_shared->next_valid ? "" : "not " );

          break;
     }

     if (ret == DFB_OK) {
          D_ASSERT( !shared->hw_running );
     }

     return ret;
}

void
pxa3xxEngineReset( void *drv, void *dev )
{
     PXA3XXDriverData *pdrv = drv;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     ioctl( pdrv->gfx_fd, PXA3XX_GCU_IOCTL_RESET );
}

void
pxa3xxEmitCommands( void *drv, void *dev )
{
     PXA3XXDriverData *pdrv = drv;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     flush_prepared( pdrv );
}

/**********************************************************************************************************************/

void
pxa3xxCheckState( void                *drv,
                  void                *dev,
                  CardState           *state,
                  DFBAccelerationMask  accel )
{
     D_DEBUG_AT( PXA3XX_BLT, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(PXA3XX_SUPPORTED_DRAWINGFUNCTIONS | PXA3XX_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     if (!pixel_formats[DFB_PIXELFORMAT_INDEX(state->destination->config.format)])
          return;

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~PXA3XX_SUPPORTED_DRAWINGFLAGS)
               return;

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               /* Check blend functions. */
               if (!check_blend_functions( state ))
                    return;
          }

          /* Enable acceleration of drawing functions. */
          state->accel |= PXA3XX_SUPPORTED_DRAWINGFUNCTIONS;
     }
     else {
          DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~PXA3XX_SUPPORTED_BLITTINGFLAGS)
               return;

          /* Return if the source format is not supported. */
          if (!pixel_formats[DFB_PIXELFORMAT_INDEX(state->source->config.format)])
               return;

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               /* Check blend functions. */
               if (!check_blend_functions( state ))
                    return;
          }

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( flags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
               return;

          /* Enable acceleration of blitting functions. */
          state->accel |= PXA3XX_SUPPORTED_BLITTINGFUNCTIONS;
     }
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
pxa3xxSetState( void                *drv,
                void                *dev,
                GraphicsDeviceFuncs *funcs,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     PXA3XXDriverData       *pdrv     = drv;
     PXA3XXDeviceData       *pdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( PXA3XX_BLT, "%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );
     DUMP_INFO();

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          PXA3XX_INVALIDATE( ALL );
     }
     else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               PXA3XX_INVALIDATE( DEST );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               PXA3XX_INVALIDATE( SOURCE );

          /* Invalidate color registers. */
          if (modified & SMF_COLOR)
               PXA3XX_INVALIDATE( COLOR );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination. */
     PXA3XX_CHECK_VALIDATE( DEST );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               /* ...require valid color. */
               PXA3XX_CHECK_VALIDATE( COLOR );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = PXA3XX_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               /* ...require valid source. */
               PXA3XX_CHECK_VALIDATE( SOURCE );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = PXA3XX_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     pdev->dflags         = state->drawingflags;
     pdev->bflags         = state->blittingflags;
     pdev->render_options = state->render_options;
     pdev->color          = state->color;

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
pxa3xxFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     PXA3XXDriverData *pdrv = drv;
     u32              *prep = start_buffer( pdrv, 4 );

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     prep[0] = 0x40000003;
     prep[1] = rect->x;
     prep[2] = rect->y;
     prep[3] = PXA3XX_WH( rect->w, rect->h );

     submit_buffer( pdrv, 4 );

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
pxa3xxBlit( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     PXA3XXDriverData *pdrv = drv;
     PXA3XXDeviceData *pdev = dev;

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

     if (pdev->bflags & DSBLIT_BLEND_ALPHACHANNEL) {
          u32 *prep = start_buffer( pdrv, 8 );

          prep[0] = 0x47000107;
          prep[1] = x;
          prep[2] = y;
          prep[3] = rect->x;
          prep[4] = rect->y;
          prep[5] = rect->x;
          prep[6] = rect->y;
          prep[7] = PXA3XX_WH( rect->w, rect->h );

          submit_buffer( pdrv, 8 );
     }
     else {
          u32  rotation = 0;
          u32 *prep     = start_buffer( pdrv, 8 );

          if (pdev->bflags & DSBLIT_ROTATE90)
               rotation = 3;
          else if (pdev->bflags & DSBLIT_ROTATE180)
               rotation = 2;
          else if (pdev->bflags & DSBLIT_ROTATE270)
               rotation = 1;

          prep[0] = 0x4A000005 | (rotation << 4); // FIXME: use 32byte alignment hint
          prep[1] = x;
          prep[2] = y;
          prep[3] = rect->x;
          prep[4] = rect->y;
          prep[5] = PXA3XX_WH( rect->w, rect->h );

          submit_buffer( pdrv, 6 );

/* RASTER          prep[0] = 0x4BCC0007;
          prep[1] = x;
          prep[2] = y;
          prep[3] = rect->x;
          prep[4] = rect->y;
          prep[5] = rect->x;
          prep[6] = rect->y;
          prep[7] = PXA3XX_WH( rect->w, rect->h );

          submit_buffer( pdrv, 8 );
 */

/* PATTERN          prep[0] = 0x4C000006;
          prep[1] = x;
          prep[2] = y;
          prep[3] = rect->x;
          prep[4] = rect->y;
          prep[5] = PXA3XX_WH( rect->w, rect->h );
          prep[6] = PXA3XX_WH( rect->w, rect->h );

          submit_buffer( pdrv, 7 );
 */

/* BIAS         prep[0] = 0x49000016;
          prep[1] = x;
          prep[2] = y;
          prep[3] = rect->x;
          prep[4] = rect->y;
          prep[5] = PXA3XX_WH( rect->w, rect->h );
          prep[6] = 0;

          submit_buffer( pdrv, 7 );
 */
     }

     return true;
}

