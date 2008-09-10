#ifdef SH7723_DEBUG_BLT
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

#include "sh7722.h"

#include "sh7723_blt.h"


D_DEBUG_DOMAIN( SH7723_BLT, "SH7723/BLT", "Renesas SH7723 Drawing Engine" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DEST         = 0x00000001,
     CLIP         = 0x00000002,
     DEST_CLIP    = 0x00000003,
     
     COLOR16      = 0x00000100,

     ALPHA        = 0x00001000,

     SOURCE       = 0x00010000,
     STRANS       = 0x00020000,

     ALL          = 0x00031103,
};

/*
 * State handling macros.
 */

#define SH7723_VALIDATE(flags)          do { sdev->v_flags |=  (flags); } while (0)
#define SH7723_INVALIDATE(flags)        do { sdev->v_flags &= ~(flags); } while (0)

#define SH7723_CHECK_VALIDATE(flag)     do {                                                        \
                                             if ((sdev->v_flags & flag) != flag)                    \
                                                  sh7723_validate_##flag( sdrv, sdev, state );      \
                                        } while (0)

#define DUMP_INFO() D_DEBUG_AT( SH7723_BLT, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                            sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                            sdrv->gfx_shared->hw_start,                             \
                                            sdrv->gfx_shared->hw_end,                               \
                                            sdrv->gfx_shared->next_start,                           \
                                            sdrv->gfx_shared->next_end,                             \
                                            sdrv->gfx_shared->next_valid ? "" : "not " );

/**********************************************************************************************************************/

static inline bool
start_hardware( SH7722DriverData *sdrv )
{
     SH772xGfxSharedArea *shared = sdrv->gfx_shared;

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     if (shared->hw_running || !shared->next_valid || shared->next_end == shared->next_start)
          return false;

     shared->hw_running = true;
     shared->hw_start   = shared->next_start;
     shared->hw_end     = shared->next_end;

     shared->next_start = shared->next_end = (shared->hw_end + 1 + 3) & ~3;
     shared->next_valid = false;

     shared->num_words += shared->hw_end - shared->hw_start;

     shared->num_starts++;

     DUMP_INFO();

     D_ASSERT( shared->buffer[shared->hw_end] == M2DG_OPCODE_TRAP );

     SH7722_TDG_SETREG32( sdrv, M2DG_DLSAR, shared->buffer_phys + shared->hw_start*4 );
     SH7722_TDG_SETREG32( sdrv, M2DG_SCLR,  1 );
     return true;
}

__attribute__((noinline))
static void
flush_prepared( SH7722DriverData *sdrv )
{
     SH772xGfxSharedArea *shared  = sdrv->gfx_shared;
     unsigned int         timeout = 2;

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     D_ASSERT( sdrv->prep_num < SH772xGFX_BUFFER_WORDS );
     D_ASSERT( sdrv->prep_num <= D_ARRAY_SIZE(sdrv->prep_buf) );

     /* Something prepared? */
     while (sdrv->prep_num) {
          int next_end;

          /* Mark shared information as invalid. From this point on the interrupt handler
           * will not continue with the next block, and we'll start the hardware ourself. */
          shared->next_valid = false;

          /* Check if there's enough space at the end.
           * Wait until hardware has started next block before it gets too big. */
          if (shared->next_end + sdrv->prep_num >= SH772xGFX_BUFFER_WORDS ||
              shared->next_end - shared->next_start >= SH772xGFX_BUFFER_WORDS/4) {
               /* If there's no next block waiting, start at the beginning. */
               if (shared->next_start == shared->next_end)
                    shared->next_start = shared->next_end = 0;
               else {
                    D_ASSERT( shared->buffer[shared->hw_end] == M2DG_OPCODE_TRAP );

                    /* Mark area as valid again. */
                    shared->next_valid = true;

                    /* Start in case it got idle while doing the checks. */
                    if (!start_hardware( sdrv )) {
                         /*
                          * Hardware has not been started (still running).
                          * Check for timeout. */
                         if (!timeout--) {
                              D_ERROR( "SH7723/Blt: Timeout waiting for processing!\n" );
                              direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                                 sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                                 sdrv->gfx_shared->hw_start,                             \
                                                 sdrv->gfx_shared->hw_end,                               \
                                                 sdrv->gfx_shared->next_start,                           \
                                                 sdrv->gfx_shared->next_end,                             \
                                                 sdrv->gfx_shared->next_valid ? "" : "not " );
                              D_ASSERT( shared->buffer[shared->hw_end] == M2DG_OPCODE_TRAP );
                              sh7723EngineReset( sdrv, sdrv->dev );
                         }

                         /* Wait til next block is started. */
                         ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_WAIT_NEXT );
                    }

                    /* Start over with the checks. */
                    continue;
               }
          }

          /* We are appending in case there was already a next block. */
          next_end = shared->next_end + sdrv->prep_num;

          /* Reset the timeout counter. */
          timeout = 2;

          /* While the hardware is running... */
          while (shared->hw_running) {
               D_ASSERT( shared->buffer[shared->hw_end] == M2DG_OPCODE_TRAP );

               /* ...make sure we don't over lap with its current buffer, otherwise wait. */
               if (shared->hw_start > next_end || shared->hw_end < shared->next_start)
                    break;

               /* Check for timeout. */
               if (!timeout--) {
                    D_ERROR( "SH7723/Blt: Timeout waiting for space!\n" );
                    direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                       sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                       sdrv->gfx_shared->hw_start,                             \
                                       sdrv->gfx_shared->hw_end,                               \
                                       sdrv->gfx_shared->next_start,                           \
                                       sdrv->gfx_shared->next_end,                             \
                                       sdrv->gfx_shared->next_valid ? "" : "not " );
                    D_ASSERT( shared->buffer[shared->hw_end] == M2DG_OPCODE_TRAP );
                    sh7723EngineReset( sdrv, sdrv->dev );
               }

               /* Wait til next block is started. */
               ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_WAIT_NEXT );
          }

          /* Copy from local to shared buffer. */
          direct_memcpy( (void*) &shared->buffer[shared->next_end], &sdrv->prep_buf[0], sdrv->prep_num * sizeof(__u32) );

          /* Terminate the block. */
          shared->buffer[next_end] = M2DG_OPCODE_TRAP;

          /* Update next block information and mark valid. */
          shared->next_end   = next_end;
          shared->next_valid = true;

          /* Reset local counter. */
          sdrv->prep_num = 0;
     }

     /* Start in case it is idle. */
     start_hardware( sdrv );
}

static inline __u32 *
start_buffer( SH7722DriverData *sdrv,
              int               space )
{
     /* Check for space in local buffer. */
     if (sdrv->prep_num + space > SH7722GFX_MAX_PREPARE) {
          /* Flush local buffer. */
          flush_prepared( sdrv );

          D_ASSERT( sdrv->prep_num == 0 );
     }

     /* Return next write position. */
     return &sdrv->prep_buf[sdrv->prep_num];
}

static inline void
submit_buffer( SH7722DriverData *sdrv,
               int               entries )
{
     D_ASSERT( sdrv->prep_num + entries <= SH7722GFX_MAX_PREPARE );

     /* Increment next write position. */
     sdrv->prep_num += entries;
}

/**********************************************************************************************************************/

static inline void
sh7723_validate_DEST_CLIP( SH7722DriverData *sdrv,
                           SH7722DeviceData *sdev,
                           CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 18 );

     D_DEBUG_AT( SH7723_BLT, "%s( 0x%08lx [%d] - %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 state->dst.phys, state->dst.pitch, DFB_RECTANGLE_VALS_FROM_REGION( &state->clip ) );

     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x0d4;
     prep[2] = SH7723_XY( state->clip.x1, state->clip.y1 ) ;

     prep[3] = M2DG_OPCODE_WPR;
     prep[4] = 0x0d8;
     prep[5] = SH7723_XY( state->clip.x2, state->clip.y2) ;
     
     if (sdev->v_flags & DEST) {
          submit_buffer( sdrv, 6 );
     }
     else {
          CoreSurface       *surface = state->destination;
          CoreSurfaceBuffer *buffer  = state->dst.buffer;
     
          sdev->dst_phys  = state->dst.phys;
          sdev->dst_pitch = state->dst.pitch;
          sdev->dst_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
          sdev->dst_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

          sdev->rclr &= ~0x00140000;
     
          switch (buffer->format) {
               case DSPF_RGB16:
                    sdev->rclr |= 0x00040000;
                    break;
     
               case DSPF_ARGB1555:
                    sdev->rclr |= 0x00140000;
                    break;
     
               default:
                    D_BUG("Unexpected pixelformat\n");
                    return;
          }
     
          /* Set destination start address. */
          prep[ 6] = M2DG_OPCODE_WPR;
          prep[ 7] = 0x50;
          prep[ 8] = sdev->dst_phys;
     
          /* Set destination stride. */
          prep[ 9] = M2DG_OPCODE_WPR;
          prep[10] = 0x5c;
          prep[11] = sdev->dst_pitch / sdev->dst_bpp;
     
          /* Set destination pixelformat in rendering control. */
          prep[12] = M2DG_OPCODE_WPR;
          prep[13] = 0xc0;
          prep[14] = sdev->rclr;
     
          /* Set system clipping rectangle. */
          prep[15] = M2DG_OPCODE_WPR;
          prep[16] = 0xd0;
          prep[17] = SH7723_XY( surface->config.size.w - 1, surface->config.size.h - 1 );
     
          submit_buffer( sdrv, 18 );
     }

     /* Set the flags. */
     SH7723_VALIDATE( DEST_CLIP );
}

static inline void
sh7723_validate_COLOR16( SH7722DriverData *sdrv,
                         SH7722DeviceData *sdev,
                         CardState        *state )
{
     sdev->color16 = dfb_pixel_from_color( state->destination->config.format, &state->color );

     /* Set the flags. */
     SH7723_VALIDATE( COLOR16 );
}

static inline void
sh7723_validate_ALPHA( SH7722DriverData *sdrv,
                       SH7722DeviceData *sdev,
                       CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 3 );

     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x088;
     prep[2] = state->color.a;

     submit_buffer( sdrv, 3 );

     /* Set the flags. */
     SH7723_VALIDATE( ALPHA );
}

static inline void
sh7723_validate_SOURCE( SH7722DriverData *sdrv,
                        SH7722DeviceData *sdev,
                        CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 6 );

     CoreSurfaceBuffer *buffer = state->src.buffer;

     sdev->src_phys  = state->src.phys;
     sdev->src_pitch = state->src.pitch;
     sdev->src_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
     sdev->src_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

     /* Set source start address. */
     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x4c;
     prep[2] = sdev->src_phys;

     /* Set source stride. */
     prep[3] = M2DG_OPCODE_WPR;
     prep[4] = 0x58;
     prep[5] = sdev->src_pitch / sdev->src_bpp;

     submit_buffer( sdrv, 6 );

     /* Set the flags. */
     SH7723_VALIDATE( SOURCE );
}

static inline void
sh7723_validate_STRANS( SH7722DriverData *sdrv,
                        SH7722DeviceData *sdev,
                        CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 3 );

     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x080;
     prep[2] = state->src_colorkey;

     submit_buffer( sdrv, 3 );

     /* Set the flags. */
     SH7723_VALIDATE( STRANS );
}

/**********************************************************************************************************************/

DFBResult
sh7723EngineSync( void *drv, void *dev )
{
     DFBResult            ret    = DFB_OK;
     SH7722DriverData    *sdrv   = drv;
     SH772xGfxSharedArea *shared = sdrv->gfx_shared;

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     while (shared->hw_running && ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_WAIT_IDLE ) < 0) {
          if (errno == EINTR)
               continue;

          ret = errno2result( errno );
          D_PERROR( "SH7723/BLT: SH7723GFX_IOCTL_WAIT_IDLE failed!\n" );

          direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                             sdrv->gfx_shared->hw_running ? "" : "not ",             \
                             sdrv->gfx_shared->hw_start,                             \
                             sdrv->gfx_shared->hw_end,                               \
                             sdrv->gfx_shared->next_start,                           \
                             sdrv->gfx_shared->next_end,                             \
                             sdrv->gfx_shared->next_valid ? "" : "not " );

          break;
     }

     if (ret == DFB_OK) {
          D_ASSERT( !shared->hw_running );
          D_ASSERT( !shared->next_valid );
     }

     return ret;
}

void
sh7723EngineReset( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;
     __u32            *prep;

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_RESET );

     prep = start_buffer( sdrv, 4 );

     /* Reset current pointer. */
     prep[0] = M2DG_OPCODE_MOVE;
     prep[1] = 0;

     /* Reset local offset. */
     prep[2] = M2DG_OPCODE_LCOFS;
     prep[3] = 0;

     submit_buffer( sdrv, 4 );
}

void
sh7723EmitCommands( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     flush_prepared( sdrv );
}

void
sh7723FlushTextureCache( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;
     __u32            *prep = start_buffer( sdrv, 1 );

     D_DEBUG_AT( SH7723_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     prep[0] = M2DG_OPCODE_SYNC | M2DG_SYNC_TCLR;

     submit_buffer( sdrv, 1 );
}

/**********************************************************************************************************************/

void
sh7723CheckState( void                *drv,
                  void                *dev,
                  CardState           *state,
                  DFBAccelerationMask  accel )
{
     D_DEBUG_AT( SH7723_BLT, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(SH7723_SUPPORTED_DRAWINGFUNCTIONS | SH7723_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_RGB16:
//          case DSPF_ARGB1555:
               break;

          default:
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~SH7723_SUPPORTED_DRAWINGFLAGS)
               return;

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               switch (accel) {
                    case DFXL_FILLRECTANGLE:
                    case DFXL_FILLTRIANGLE:
                         break;
                    default:
                         return;
               }

               /* Return if blending with unsupported blend functions is requested. */
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA)
                    return;

               /* XOR only without blending. */
               if (state->drawingflags & DSDRAW_XOR)
                    return;
          }

          /* Enable acceleration of drawing functions. */
          state->accel |= accel;
     } else {
          DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~SH7723_SUPPORTED_BLITTINGFLAGS)
               return;

          /* Return if the source format is not supported. */
          if (state->source->config.format != state->destination->config.format)
               return;
      
          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA)
                    return;
          }

          /* XOR only without blending etc. */
          if (flags & DSBLIT_XOR &&
              flags & ~(DSBLIT_SRC_COLORKEY | DSBLIT_ROTATE180 | DSBLIT_XOR))
               return;

          /* Return if colorizing for non-font surfaces is requested. */
          if ((flags & DSBLIT_COLORIZE) && !(state->source->type & CSTF_FONT))
               return;

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( flags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
               return;

          /* Enable acceleration of blitting functions. */
          state->accel |= accel;
     }
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
sh7723SetState( void                *drv,
                void                *dev,
                GraphicsDeviceFuncs *funcs,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     SH7722DriverData       *sdrv     = drv;
     SH7722DeviceData       *sdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( SH7723_BLT, "%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );

     DUMP_INFO();

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          SH7723_INVALIDATE( ALL );
     } else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               SH7723_INVALIDATE( DEST | COLOR16 );

          /* Invalidate clipping registers. */
          if (modified & SMF_CLIP)
               SH7723_INVALIDATE( CLIP );
     
          /* Invalidate color registers. */
          if (modified & SMF_COLOR)
               SH7723_INVALIDATE( ALPHA | COLOR16 );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               SH7723_INVALIDATE( SOURCE );

          /* Invalidate source colorkey. */
          if (modified & SMF_SRC_COLORKEY)
               SH7723_INVALIDATE( STRANS );
     }          
      
     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination and clip. */
     SH7723_CHECK_VALIDATE( DEST_CLIP );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWLINE:
               /* ...require valid color. */
               SH7723_CHECK_VALIDATE( COLOR16 );

               /* If blending is used, validate the alpha value. */
               if (state->drawingflags & DSDRAW_BLEND)
                    SH7723_CHECK_VALIDATE( ALPHA );
               
               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = SH7723_SUPPORTED_DRAWINGFUNCTIONS;

               break;

          case DFXL_BLIT:
               /* ...require valid source. */
               SH7723_CHECK_VALIDATE( SOURCE );

               /* If blending is used, validate the alpha value. */
               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                    SH7723_CHECK_VALIDATE( ALPHA );

               /* If colorkeying is used, validate the colorkey. */
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    SH7723_CHECK_VALIDATE( STRANS );
               
               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = SH7723_SUPPORTED_BLITTINGFUNCTIONS;

               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;

     }

     sdev->dflags         = state->drawingflags;
     sdev->bflags         = state->blittingflags;
     sdev->render_options = state->render_options;
     sdev->color          = state->color;
     
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
sh7723FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 6 );

     D_DEBUG_AT( SH7723_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     prep[0] = M2DG_OPCODE_BITBLTC | M2DG_DRAWMODE_CLIP;

     if (sdev->dflags & DSDRAW_BLEND) 
          prep[0] |= M2DG_DRAWMODE_ALPHA;

     prep[1] = 0xcc; 
     prep[2] = sdev->color16;
     prep[3] = rect->w - 1;
     prep[4] = rect->h - 1;
     prep[5] = SH7723_XY( rect->x, rect->y );

     submit_buffer( sdrv, 6 );

     return true;
}

/**********************************************************************************************************************/

/*
 * Render rectangle outlines using the current hardware state.
 */
bool
sh7723DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer(sdrv, 8 );

     int x1, x2, y1, y2;

     x1 = rect->x;
     y1 = rect->y; 
     x2 = rect->x + rect->w - 1;
     y2 = rect->y + rect->h - 1;

     D_DEBUG_AT( SH7723_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     prep[0] = M2DG_OPCODE_LINE_C | M2DG_DRAWMODE_CLIP; 

     if (sdev->dflags & DSDRAW_BLEND) 
          prep[0] |= M2DG_DRAWMODE_ALPHA;

     prep[1] = (sdev->color16 << 16 ) | 5;
     prep[2] = 0;

     prep[3] = SH7723_XY( x1, y1 );
     prep[4] = SH7723_XY( x2, y1 );
     prep[5] = SH7723_XY( x2, y2 );
     prep[6] = SH7723_XY( x1, y2 );
     prep[7] = SH7723_XY( x1, y1 );

     submit_buffer( sdrv, 8 );

     return true;
}

/**********************************************************************************************************************/

/*
 * Render a triangle using the current hardware state.
 */
bool 
sh7723FillTriangle( void *drv, void *dev, DFBTriangle *triangle )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 6 );

     D_DEBUG_AT( SH7723_BLT, "%s( %d, %d - %dx, %d - %d, %d )\n", __FUNCTION__,
                 DFB_TRIANGLE_VALS( triangle ) );
     DUMP_INFO();

     prep[0] = M2DG_OPCODE_POLYGON_4C | M2DG_DRAWMODE_CLIP;

     if (sdev->dflags & DSDRAW_BLEND) 
          prep[0] |= M2DG_DRAWMODE_ALPHA;

     prep[1] = sdev->color16;

     prep[2] = SH7723_XY( triangle->x1, triangle->y1 );
     prep[3] = SH7723_XY( triangle->x2, triangle->y2 );
     prep[4] = SH7723_XY( triangle->x3, triangle->y3 );
     prep[5] = SH7723_XY( triangle->x3, triangle->y3 );

     submit_buffer( sdrv, 6 );

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
sh7723DrawLine( void *drv, void *dev, DFBRegion *line )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 5 );

     D_DEBUG_AT( SH7723_BLT, "%s( %d, %d - %d, %d )\n", __FUNCTION__,
                 line->x1, line->y1, line->x2, line->y2 );
     DUMP_INFO();

     prep[0] = M2DG_OPCODE_LINE_C | M2DG_DRAWMODE_CLIP;

     if (sdev->render_options & DSRO_ANTIALIAS)
          prep[0] |= M2DG_DRAWMODE_ANTIALIAS;

     prep[1] = (sdev->color16 << 16) | 2;
     prep[2] = 0;

     prep[3] = SH7723_XY( line->x1, line->y1 );
     prep[4] = SH7723_XY( line->x2, line->y2 );

     submit_buffer( sdrv, 5);

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
sh7723Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 6 );

     D_DEBUG_AT( SH7723_BLT, "%s( %d, %d - %dx%d <- %d, %d )\n", __FUNCTION__,
                 dx, dy, rect->w, rect->h, rect->x, rect->y );
     DUMP_INFO();

     prep[0] = M2DG_OPCODE_BITBLTA | M2DG_DRAWMODE_CLIP;

     if (sdev->bflags & DSBLIT_BLEND_COLORALPHA) 
          prep[0] |= M2DG_DRAWMODE_ALPHA;

     if (sdev->bflags & DSBLIT_SRC_COLORKEY) 
          prep[0] |= M2DG_DRAWMODE_STRANS;

     if (sdev->src_phys == sdev->dst_phys) {
          if (dy > rect->y)
               prep[0] |= M2DG_DRAWMODE_DSTDIR_Y | M2DG_DRAWMODE_SRCDIR_Y;
          else if (dy == rect->y) {
               if (dx > rect->x) 
                    prep[0] |= M2DG_DRAWMODE_DSTDIR_X | M2DG_DRAWMODE_SRCDIR_X;
          }
     }
          
     prep[1] = 0xcc; 
     prep[2] = SH7723_XY( rect->x, rect->y );
     prep[3] = rect->w - 1;
     prep[4] = rect->h - 1;
     prep[5] = SH7723_XY( dx, dy );

     submit_buffer( sdrv, 6 );

     return true;
}
