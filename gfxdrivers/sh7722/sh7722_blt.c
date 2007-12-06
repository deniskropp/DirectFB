#ifdef SH7722_DEBUG
#define DIRECT_FORCE_DEBUG
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
#include "sh7722_blt.h"


D_DEBUG_DOMAIN( SH7722_BLT, "SH7722/BLT", "Renesas SH7722 Drawing Engine" );

D_DEBUG_DOMAIN( SH7722_StartStop, "SH7722/StartStop", "Renesas SH7722 Drawing Start/Stop" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DEST         = 0x00000001,
     CLIP         = 0x00000002,
     SOURCE       = 0x00000004,

     COLOR1       = 0x00000010,

     FGC          = 0x00000100,
     COLOR_KEY    = 0x00000200,
     COLOR_CHANGE = 0x00000400,

     BLEND_SRCF   = 0x00001000,
     BLEND_DSTF   = 0x00002000,
     FIXEDALPHA   = 0x00004000,

     MATRIX       = 0x00010000,

     ALL          = 0x00017717
};

/*
 * State handling macros.
 */

#define SH7722_VALIDATE(flags)          do { sdev->v_flags |=  (flags); } while (0)
#define SH7722_INVALIDATE(flags)        do { sdev->v_flags &= ~(flags); } while (0)

#define SH7722_CHECK_VALIDATE(flag)     do {                                                        \
                                             if (! (sdev->v_flags & flag))                          \
                                                  sh7722_validate_##flag( sdrv, sdev, state );      \
                                        } while (0)

#define DUMP_INFO() D_DEBUG_AT( SH7722_BLT, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                            sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                            sdrv->gfx_shared->hw_start,                             \
                                            sdrv->gfx_shared->hw_end,                               \
                                            sdrv->gfx_shared->next_start,                           \
                                            sdrv->gfx_shared->next_end,                             \
                                            sdrv->gfx_shared->next_valid ? "" : "not " );

#define AA_COEF     133

/**********************************************************************************************************************/

static bool sh7722FillRectangle        ( void *drv, void *dev, DFBRectangle *rect );
static bool sh7722FillRectangleMatrixAA( void *drv, void *dev, DFBRectangle *rect );

static bool sh7722DrawRectangle        ( void *drv, void *dev, DFBRectangle *rect );
static bool sh7722DrawRectangleMatrixAA( void *drv, void *dev, DFBRectangle *rect );

static bool sh7722DrawLine           ( void *drv, void *dev, DFBRegion *line );
static bool sh7722DrawLineMatrix     ( void *drv, void *dev, DFBRegion *line );
static bool sh7722DrawLineAA         ( void *drv, void *dev, DFBRegion *line );

/**********************************************************************************************************************/

static inline bool
check_blend_functions( const CardState *state )
{
     switch (state->src_blend) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
               return true;

          default:
               break;
     }
     switch (state->dst_blend) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
               return true;

          default:
               break;
     }

     return false;
}

/**********************************************************************************************************************/

static inline bool
start_hardware( SH7722DriverData *sdrv )
{
     SH7722GfxSharedArea *shared = sdrv->gfx_shared;

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

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

     D_ASSERT( shared->buffer[shared->hw_end] == 0xF0000000 );

     SH7722_SETREG32( sdrv, BEM_HC_DMA_ADR,   shared->buffer_phys + shared->hw_start*4 );
     SH7722_SETREG32( sdrv, BEM_HC_DMA_START, 1 );

     return true;
}

static void
flush_prepared( SH7722DriverData *sdrv )
{
     SH7722GfxSharedArea *shared  = sdrv->gfx_shared;
     unsigned int         timeout = 2;

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     D_ASSERT( sdrv->prep_num < SH7722GFX_BUFFER_WORDS );
     D_ASSERT( sdrv->prep_num <= D_ARRAY_SIZE(sdrv->prep_buf) );

     /* Something prepared? */
     while (sdrv->prep_num) {
          int next_end;

          /* Mark shared information as invalid. From this point on the interrupt handler
           * will not continue with the next block, and we'll start the hardware ourself. */
          shared->next_valid = false;

          /* Check if there's enough space at the end.
           * Wait until hardware has started next block before it gets too big. */
          if (shared->next_end + sdrv->prep_num >= SH7722GFX_BUFFER_WORDS ||
              shared->next_end - shared->next_start >= SH7722GFX_BUFFER_WORDS/4)
          {
               /* If there's no next block waiting, start at the beginning. */
               if (shared->next_start == shared->next_end)
                    shared->next_start = shared->next_end = 0;
               else {
                    D_ASSERT( shared->buffer[shared->hw_end] == 0xF0000000 );

                    /* Mark area as valid again. */
                    shared->next_valid = true;

                    /* Start in case it got idle while doing the checks. */
                    if (!start_hardware( sdrv )) {
                         /*
                          * Hardware has not been started (still running).
                          * Check for timeout. */
                         if (!timeout--) {
                              D_ERROR( "SH7722/Blt: Timeout waiting for processing!\n" );
                              direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                                 sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                                 sdrv->gfx_shared->hw_start,                             \
                                                 sdrv->gfx_shared->hw_end,                               \
                                                 sdrv->gfx_shared->next_start,                           \
                                                 sdrv->gfx_shared->next_end,                             \
                                                 sdrv->gfx_shared->next_valid ? "" : "not " );
                              D_ASSERT( shared->buffer[shared->hw_end] == 0xF0000000 );
                              sh7722EngineReset( sdrv, sdrv->dev );
                         }

                         /* Wait til next block is started. */
                         ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_WAIT_NEXT );
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
               D_ASSERT( shared->buffer[shared->hw_end] == 0xF0000000 );

               /* ...make sure we don't over lap with its current buffer, otherwise wait. */
               if (shared->hw_start > next_end || shared->hw_end < shared->next_start)
                    break;

               /* Check for timeout. */
               if (!timeout--) {
                    D_ERROR( "SH7722/Blt: Timeout waiting for space!\n" );
                    direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                                       sdrv->gfx_shared->hw_running ? "" : "not ",             \
                                       sdrv->gfx_shared->hw_start,                             \
                                       sdrv->gfx_shared->hw_end,                               \
                                       sdrv->gfx_shared->next_start,                           \
                                       sdrv->gfx_shared->next_end,                             \
                                       sdrv->gfx_shared->next_valid ? "" : "not " );
                    D_ASSERT( shared->buffer[shared->hw_end] == 0xF0000000 );
                    sh7722EngineReset( sdrv, sdrv->dev );
               }

               /* Wait til next block is started. */
               ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_WAIT_NEXT );
          }

          /* Copy from local to shared buffer. */
          direct_memcpy( (void*) &shared->buffer[shared->next_end], &sdrv->prep_buf[0], sdrv->prep_num * sizeof(__u32) );

          /* Terminate the block. */
          shared->buffer[next_end] = 0xF0000000;

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
sh7722_validate_DEST( SH7722DriverData *sdrv,
                      SH7722DeviceData *sdev,
                      CardState        *state )
{
     CoreSurface       *surface = state->destination;
     CoreSurfaceBuffer *buffer  = state->dst.buffer;
     __u32             *prep    = start_buffer( sdrv, 6 );

     sdev->dst_offset = state->dst.phys;
     sdev->dst_pitch  = state->dst.pitch;
     sdev->dst_bpp    = DFB_BYTES_PER_PIXEL( buffer->format );

     /* Set destination. */
     prep[0] = BEM_PE_DST;

     switch (buffer->format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               prep[1] = 0;
               break;

          case DSPF_RGB16:
               prep[1] = 1;
               break;

          case DSPF_ARGB1555:
          case DSPF_RGB555:
               prep[1] = 3;
               break;

          case DSPF_ARGB4444:
          case DSPF_RGB444:
               prep[1] = 4;
               break;

          default:
               D_BUG( "unexpected pixelformat" );
     }

     prep[2] = BEM_PE_DST_BASE;
     prep[3] = sdev->dst_offset;

     prep[4] = BEM_PE_DST_SIZE;
     prep[5] = SH7722_XY( sdev->dst_pitch / sdev->dst_bpp, surface->config.size.h );

     submit_buffer( sdrv, 6 );

     /* Set the flag. */
     SH7722_VALIDATE( DEST );
}

static inline void
sh7722_validate_CLIP( SH7722DriverData *sdrv,
                      SH7722DeviceData *sdev,
                      CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 4 );

     /* Set clip. */
     prep[0] = BEM_PE_SC0_MIN;
     prep[1] = SH7722_XY( state->clip.x1, state->clip.y1 );

     prep[2] = BEM_PE_SC0_MAX;
     prep[3] = SH7722_XY( state->clip.x2, state->clip.y2 );

     submit_buffer( sdrv, 4 );

     /* Set the flag. */
     SH7722_VALIDATE( CLIP );
}

static inline void
sh7722_validate_SOURCE( SH7722DriverData *sdrv,
                        SH7722DeviceData *sdev,
                        CardState        *state )
{
     CoreSurface       *surface = state->source;
     CoreSurfaceBuffer *buffer  = state->src.buffer;
     __u32             *prep    = start_buffer( sdrv, 6 );

     sdev->src_offset = state->src.phys;
     sdev->src_pitch  = state->src.pitch;
     sdev->src_bpp    = DFB_BYTES_PER_PIXEL( buffer->format );

     /* Set source. */
     prep[0] = BEM_TE_SRC;

     switch (buffer->format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               prep[1] = 0;
               break;

          case DSPF_RGB16:
               prep[1] = 1;
               break;

          case DSPF_ARGB1555:
          case DSPF_RGB555:
               prep[1] = 3;
               break;

          case DSPF_ARGB4444:
          case DSPF_RGB444:
               prep[1] = 4;
               break;

          default:
               D_BUG( "unexpected pixelformat" );
     }

     prep[2] = BEM_TE_SRC_BASE;
     prep[3] = sdev->src_offset;

     prep[4] = BEM_TE_SRC_SIZE;
     prep[5] = SH7722_XY( sdev->src_pitch / sdev->src_bpp, surface->config.size.h );

     submit_buffer( sdrv, 6 );

     /* Set the flag. */
     SH7722_VALIDATE( SOURCE );
}

static inline void
sh7722_validate_COLOR1( SH7722DriverData *sdrv,
                        SH7722DeviceData *sdev,
                        CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 2 );

     prep[0] = BEM_BE_COLOR1;
     prep[1] = PIXEL_ARGB( state->color.a,
                           state->color.r,
                           state->color.g,
                           state->color.b );

     submit_buffer( sdrv, 2 );

     /* Set the flag. */
     SH7722_VALIDATE( COLOR1 );
}

static inline void
sh7722_validate_FGC( SH7722DriverData *sdrv,
                     SH7722DeviceData *sdev,
                     CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 2 );

     prep[0] = BEM_WR_FGC;
     prep[1] = PIXEL_ARGB( state->color.a,
                           state->color.r,
                           state->color.g,
                           state->color.b );

     submit_buffer( sdrv, 2 );

     /* Set the flag. */
     SH7722_VALIDATE( FGC );
}

static inline void
sh7722_validate_COLOR_KEY( SH7722DriverData *sdrv,
                           SH7722DeviceData *sdev,
                           CardState        *state )
{
     CoreSurfaceBuffer *buffer = state->src.buffer;
     __u32             *prep   = start_buffer( sdrv, 4 );

     prep[0] = BEM_PE_CKEY;
     prep[1] = CKEY_EXCLUDE_ALPHA | CKEY_EXCLUDE_UNUSED | CKEY_B_ENABLE;

     prep[2] = BEM_PE_CKEY_B;

     switch (buffer->format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               prep[3] = state->src_colorkey;
               break;

          case DSPF_RGB16:
               prep[3] = RGB16_TO_RGB32( state->src_colorkey );
               break;

          case DSPF_ARGB1555:
          case DSPF_RGB555:
               prep[3] = ARGB1555_TO_RGB32( state->src_colorkey );
               break;

          case DSPF_ARGB4444:
          case DSPF_RGB444:
               prep[3] = ARGB4444_TO_RGB32( state->src_colorkey );
               break;

          default:
               D_BUG( "unexpected pixelformat" );
     }

     submit_buffer( sdrv, 4 );

     /* Set the flag. */
     SH7722_VALIDATE( COLOR_KEY );
}

static inline void
sh7722_validate_COLOR_CHANGE( SH7722DriverData *sdrv,
                              SH7722DeviceData *sdev,
                              CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 6 );

     prep[0] = BEM_PE_COLORCHANGE;
     prep[1] = COLORCHANGE_COMPARE_FIRST | COLORCHANGE_EXCLUDE_UNUSED;

     prep[2] = BEM_PE_COLORCHANGE_0;
     prep[3] = 0xffffff;

     prep[4] = BEM_PE_COLORCHANGE_1;
     prep[5] = PIXEL_ARGB( state->color.a,
                           state->color.r,
                           state->color.g,
                           state->color.b );

     submit_buffer( sdrv, 6 );

     /* Set the flag. */
     SH7722_VALIDATE( COLOR_CHANGE );
}

static inline void
sh7722_validate_FIXEDALPHA( SH7722DriverData *sdrv,
                            SH7722DeviceData *sdev,
                            CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 2 );

     prep[0] = BEM_PE_FIXEDALPHA;
     prep[1] = (state->color.a << 24) | (state->color.a << 16);

     submit_buffer( sdrv, 2 );

     /* Set the flag. */
     SH7722_VALIDATE( FIXEDALPHA );
}

static inline void
sh7722_validate_BLEND_SRCF( SH7722DriverData *sdrv,
                            SH7722DeviceData *sdev,
                            CardState        *state )
{
     switch (state->src_blend) {
          case DSBF_ZERO:
               sdev->ble_srcf = BLE_SRCF_ZERO;
               break;

          case DSBF_ONE:
               sdev->ble_srcf = BLE_SRCF_ONE;
               break;

          case DSBF_DESTCOLOR:
               sdev->ble_srcf = BLE_SRCF_DST;
               break;

          case DSBF_INVDESTCOLOR:
               sdev->ble_srcf = BLE_SRCF_1_DST;
               break;

          case DSBF_SRCALPHA:
               sdev->ble_srcf = BLE_SRCF_SRC_A;
               break;

          case DSBF_INVSRCALPHA:
               sdev->ble_srcf = BLE_SRCF_1_SRC_A;
               break;

          case DSBF_DESTALPHA:
               sdev->ble_srcf = BLE_SRCF_DST_A;
               break;

          case DSBF_INVDESTALPHA:
               sdev->ble_srcf = BLE_SRCF_1_DST_A;
               break;

          default:
               D_BUG( "unexpected blend function 0x%x", state->src_blend );
     }

     /* Set the flag. */
     SH7722_VALIDATE( BLEND_SRCF );
}

static inline void
sh7722_validate_BLEND_DSTF( SH7722DriverData *sdrv,
                            SH7722DeviceData *sdev,
                            CardState        *state )
{
     switch (state->dst_blend) {
          case DSBF_ZERO:
               sdev->ble_dstf = BLE_DSTF_ZERO;
               break;

          case DSBF_ONE:
               sdev->ble_dstf = BLE_DSTF_ONE;
               break;

          case DSBF_SRCCOLOR:
               sdev->ble_dstf = BLE_DSTF_SRC;
               break;

          case DSBF_INVSRCCOLOR:
               sdev->ble_dstf = BLE_DSTF_1_SRC;
               break;

          case DSBF_SRCALPHA:
               sdev->ble_dstf = BLE_DSTF_SRC_A;
               break;

          case DSBF_INVSRCALPHA:
               sdev->ble_dstf = BLE_DSTF_1_SRC_A;
               break;

          case DSBF_DESTALPHA:
               sdev->ble_dstf = BLE_DSTF_DST_A;
               break;

          case DSBF_INVDESTALPHA:
               sdev->ble_dstf = BLE_DSTF_1_DST_A;
               break;

          default:
               D_BUG( "unexpected blend function 0x%x", state->dst_blend );
     }

     /* Set the flag. */
     SH7722_VALIDATE( BLEND_DSTF );
}

static inline void
sh7722_validate_MATRIX( SH7722DriverData *sdrv,
                        SH7722DeviceData *sdev,
                        CardState        *state )
{
     __u32 *prep = start_buffer( sdrv, 12 );

     prep[0]  = BEM_BE_MATRIX_A;
     prep[1]  = state->matrix[0];
              
     prep[2]  = BEM_BE_MATRIX_B;
     prep[3]  = state->matrix[1];
              
     prep[4]  = BEM_BE_MATRIX_C;
     prep[5]  = state->matrix[2];
              
     prep[6]  = BEM_BE_MATRIX_D;
     prep[7]  = state->matrix[3];
              
     prep[8]  = BEM_BE_MATRIX_E;
     prep[9]  = state->matrix[4];

     prep[10] = BEM_BE_MATRIX_F;
     prep[11] = state->matrix[5];

     submit_buffer( sdrv, 12 );

     direct_memcpy( sdev->matrix, state->matrix, sizeof(s32) * 6 );

     /* Set the flag. */
     SH7722_VALIDATE( MATRIX );
}

/**********************************************************************************************************************/

static void
invalidate_ckey( SH7722DriverData *sdrv, SH7722DeviceData *sdev )
{
     __u32 *prep = start_buffer( sdrv, 4 );

     prep[0] = BEM_PE_CKEY;
     prep[1] = 0;

     prep[2] = BEM_PE_CKEY_B;
     prep[3] = 0;

     submit_buffer( sdrv, 4 );

     sdev->ckey_b_enabled = false;

     SH7722_INVALIDATE( COLOR_KEY );
}

static void
invalidate_color_change( SH7722DriverData *sdrv, SH7722DeviceData *sdev )
{
     __u32 *prep = start_buffer( sdrv, 2 );

     prep[0] = BEM_PE_COLORCHANGE;
     prep[1] = COLORCHANGE_DISABLE;

     submit_buffer( sdrv, 2 );

     sdev->color_change_enabled = false;

     SH7722_INVALIDATE( COLOR_CHANGE );
}

/**********************************************************************************************************************/

DFBResult
sh7722EngineSync( void *drv, void *dev )
{
     DFBResult            ret    = DFB_OK;
     SH7722DriverData    *sdrv   = drv;
     SH7722GfxSharedArea *shared = sdrv->gfx_shared;

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

/*     printf( "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                        sdrv->gfx_shared->hw_running ? "" : "not ",             \
                        sdrv->gfx_shared->hw_start,                             \
                        sdrv->gfx_shared->hw_end,                               \
                        sdrv->gfx_shared->next_start,                           \
                        sdrv->gfx_shared->next_end,                             \
                        sdrv->gfx_shared->next_valid ? "" : "not " );
*/
     while (shared->hw_running && ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_WAIT_IDLE ) < 0) {
          if (errno == EINTR)
               continue;

          ret = errno2result( errno );
          D_PERROR( "SH7722/BLT: SH7722GFX_IOCTL_WAIT_IDLE failed!\n" );

          direct_log_printf( NULL, "  -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                             sdrv->gfx_shared->hw_running ? "" : "not ",             \
                             sdrv->gfx_shared->hw_start,                             \
                             sdrv->gfx_shared->hw_end,                               \
                             sdrv->gfx_shared->next_start,                           \
                             sdrv->gfx_shared->next_end,                             \
                             sdrv->gfx_shared->next_valid ? "" : "not " );

          break;
     }

/*     printf( "   -> %srunning, hw %d-%d, next %d-%d - %svalid\n",     \
                        sdrv->gfx_shared->hw_running ? "" : "not ",             \
                        sdrv->gfx_shared->hw_start,                             \
                        sdrv->gfx_shared->hw_end,                               \
                        sdrv->gfx_shared->next_start,                           \
                        sdrv->gfx_shared->next_end,                             \
                        sdrv->gfx_shared->next_valid ? "" : "not " );
*/
     if (ret == DFB_OK) {
          D_ASSERT( !shared->hw_running );
          D_ASSERT( !shared->next_valid );
     }

     return ret;
}

void
sh7722EngineReset( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep;

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_RESET );

     prep = start_buffer( sdrv, 18 );

     prep[0] = BEM_PE_OPERATION;
     prep[1] = 0x00000000;

     prep[2] = BEM_PE_COLORCHANGE;
     prep[3] = 0x00000000;

     prep[4] = BEM_PE_CKEY;
     prep[5] = 0x00000000;

     prep[6] = BEM_PE_CKEY_B;
     prep[7] = 0;

     prep[8] = BEM_PE_FIXEDALPHA;
     prep[9] = 0x80000000;

     prep[10] = BEM_TE_SRC_CNV;
     prep[11] = 0x00100010;   /* full conversion of Ad, As, Cd and Cs */

     prep[12] = BEM_TE_FILTER;
     prep[13] = 0x00000000;   /* 0 = nearest, 3 = up bilinear / down average */

     prep[14] = BEM_PE_SC;
     prep[15] = 0x00000001;   /* enable clipping */

     prep[16] = BEM_BE_ORIGIN;
     prep[17] = SH7722_XY( 0, 0 );

     submit_buffer( sdrv, 18 );

     sdev->ckey_b_enabled = false;
}

void
sh7722EmitCommands( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

     flush_prepared( sdrv );
}

void
sh7722FlushTextureCache( void *drv, void *dev )
{
     SH7722DriverData *sdrv = drv;
     __u32            *prep = start_buffer( sdrv, 4 );

     D_DEBUG_AT( SH7722_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     prep[0] = BEM_PE_CACHE;
     prep[1] = 2;

     prep[2] = BEM_TE_INVALID;
     prep[3] = 1;

     submit_buffer( sdrv, 4 );
}

/**********************************************************************************************************************/

void
sh7722CheckState( void                *drv,
                  void                *dev,
                  CardState           *state,
                  DFBAccelerationMask  accel )
{
     D_DEBUG_AT( SH7722_BLT, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(SH7722_SUPPORTED_DRAWINGFUNCTIONS | SH7722_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB16:
          case DSPF_ARGB1555:
          case DSPF_RGB555:
          case DSPF_ARGB4444:
          case DSPF_RGB444:
               break;
          default:
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~SH7722_SUPPORTED_DRAWINGFLAGS)
               return;

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               /* No blending for LUT8 */
               if (state->destination->config.format == DSPF_LUT8)
                    return;

               /* Check blend functions. */
               if (!check_blend_functions( state ))
                    return;
          }

          /* Enable acceleration of drawing functions. */
          state->accel |= SH7722_SUPPORTED_DRAWINGFUNCTIONS;
     }
     else {
          /* Return if unsupported blitting flags are set. */
          if (state->blittingflags & ~SH7722_SUPPORTED_BLITTINGFLAGS) 
               return;

          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB:
               case DSPF_RGB32:
               case DSPF_RGB16:
               case DSPF_ARGB1555:
               case DSPF_RGB555:
               case DSPF_ARGB4444:
               case DSPF_RGB444:
                    break;

               default:
                    return;
          }

          /* Return if blending with unsupported blend functions is requested. */
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               /* No blending for LUT8 */
               if (state->destination->config.format == DSPF_LUT8)
                    return;

               /* Check blend functions. */
               if (!check_blend_functions( state ))
                    return;
          }

          /* Return if colorizing for non-font surfaces is requested. */
          if ((state->blittingflags & DSBLIT_COLORIZE) && !(state->source->type & CSTF_FONT))
               return;

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( state->blittingflags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
               return;

          /* Enable acceleration of blitting functions. */
          state->accel |= SH7722_SUPPORTED_BLITTINGFUNCTIONS;
     }
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
sh7722SetState( void                *drv,
                void                *dev,
                GraphicsDeviceFuncs *funcs,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     SH7722DriverData       *sdrv     = drv;
     SH7722DeviceData       *sdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( SH7722_BLT, "%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );
     DUMP_INFO();

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          SH7722_INVALIDATE( ALL );
     }
     else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               SH7722_INVALIDATE( DEST );

          /* Invalidate clipping registers. */
          if (modified & SMF_CLIP)
               SH7722_INVALIDATE( CLIP );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               SH7722_INVALIDATE( SOURCE | COLOR_KEY );
          else if (modified & SMF_SRC_COLORKEY)
               SH7722_INVALIDATE( COLOR_KEY );

          /* Invalidate color register. */
          if (modified & SMF_COLOR)
               SH7722_INVALIDATE( COLOR1 | FGC | FIXEDALPHA | COLOR_CHANGE );

          /* Invalidate blend functions. */
          if (modified & SMF_SRC_BLEND)
               SH7722_INVALIDATE( BLEND_SRCF );
          if (modified & SMF_DST_BLEND)
               SH7722_INVALIDATE( BLEND_DSTF );

          /* Invalidate matrix registers. */
          if (modified & SMF_MATRIX)
               SH7722_INVALIDATE( MATRIX );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination and clip. */
     SH7722_CHECK_VALIDATE( DEST );
     SH7722_CHECK_VALIDATE( CLIP );

     /* Use transformation matrix? */
     if (state->render_options & DSRO_MATRIX)
          SH7722_CHECK_VALIDATE( MATRIX );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               /* ...require valid color. */
               if (accel == DFXL_FILLRECTANGLE || accel == DFXL_FILLTRIANGLE)
                    SH7722_CHECK_VALIDATE( COLOR1 );
               else
                    SH7722_CHECK_VALIDATE( FGC );

               /* Use blending? */
               if (state->drawingflags & DSDRAW_BLEND) {
                    /* need valid source and destination blend factors */
                    SH7722_CHECK_VALIDATE( BLEND_SRCF );
                    SH7722_CHECK_VALIDATE( BLEND_DSTF );
                    SH7722_CHECK_VALIDATE( FIXEDALPHA );
               }

               /* Clear old ckeys */
               if (sdev->ckey_b_enabled) {
                    invalidate_ckey( sdrv, sdev );

                    /* Force SetState() to be called before the next blitting operation. */
                    state->set &= ~SH7722_SUPPORTED_BLITTINGFUNCTIONS;
               }

               /* Choose function. */
               switch (accel) {
                    case DFXL_FILLRECTANGLE:
                         if (state->render_options & (DSRO_MATRIX | DSRO_ANTIALIAS))
                              funcs->FillRectangle = sh7722FillRectangleMatrixAA;
                         else
                              funcs->FillRectangle = sh7722FillRectangle;
                         break;

                    case DFXL_DRAWRECTANGLE:
                         if (state->render_options & (DSRO_MATRIX | DSRO_ANTIALIAS))
                              funcs->DrawRectangle = sh7722DrawRectangleMatrixAA;
                         else
                              funcs->DrawRectangle = sh7722DrawRectangle;
                         break;

                    case DFXL_DRAWLINE:
                         if (state->render_options & DSRO_ANTIALIAS)
                              funcs->DrawLine = sh7722DrawLineAA;
                         else if (state->render_options & DSRO_MATRIX)
                              funcs->DrawLine = sh7722DrawLineMatrix;
                         else
                              funcs->DrawLine = sh7722DrawLine;
                         break;

                    default:
                         break;
               }

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set |= accel;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               /* ...require valid source. */
               SH7722_CHECK_VALIDATE( SOURCE );

               /* Use blending? */
               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
                    /* need valid source and destination blend factors */
                    SH7722_CHECK_VALIDATE( BLEND_SRCF );
                    SH7722_CHECK_VALIDATE( BLEND_DSTF );
               }

               /* Use alpha value from color? */
               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
                    /* need valid fixed alpha */
                    SH7722_CHECK_VALIDATE( FIXEDALPHA );
               }

               /* Use color keying? */
               if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                    /* Need valid color key settings (enabling). */
                    SH7722_CHECK_VALIDATE( COLOR_KEY );

                    sdev->ckey_b_enabled = true;
               }
               /* Disable color keying? */
               else if (sdev->ckey_b_enabled)
                    invalidate_ckey( sdrv, sdev );

               /* Use color change? */
               if (state->blittingflags & DSBLIT_COLORIZE) {
                    /* Need valid color change settings (enabling). */
                    SH7722_CHECK_VALIDATE( COLOR_CHANGE );

                    sdev->color_change_enabled = true;
               }
               /* Disable color change? */
               else if (sdev->color_change_enabled)
                    invalidate_color_change( sdrv, sdev );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = SH7722_SUPPORTED_BLITTINGFUNCTIONS;
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

static inline void
draw_rectangle( SH7722DriverData *sdrv,
                SH7722DeviceData *sdev,
                int x1, int y1,
                int x2, int y2,
                int x3, int y3,
                int x4, int y4,
                bool antialias,
                bool full )
{
     u32  ctrl = antialias ? WR_CTRL_ANTIALIAS : 0;
     u32 *prep = start_buffer( sdrv, full ? 24 : 12 );

     if (antialias) {
          prep[0] = BEM_WR_FGC;
          prep[1] = PIXEL_ARGB( (sdev->color.a * AA_COEF) >> 8,
                                (sdev->color.r * AA_COEF) >> 8,
                                (sdev->color.g * AA_COEF) >> 8,
                                (sdev->color.b * AA_COEF) >> 8 );

          prep[2] = BEM_PE_FIXEDALPHA;
          prep[3] = (sdev->color.a * AA_COEF) << 16;
     }
     else {
          prep[0] = BEM_WR_FGC;
          prep[1] = PIXEL_ARGB( sdev->color.a,
                                sdev->color.r,
                                sdev->color.g,
                                sdev->color.b );

          prep[2] = BEM_PE_FIXEDALPHA;
          prep[3] = (sdev->color.a << 24) << (sdev->color.a << 16);
     }

     prep[4] = BEM_WR_V1;
     prep[5] = SH7722_XY( x1, y1 );

     prep[6] = BEM_WR_V2;
     prep[7] = SH7722_XY( x2, y2 );

     prep[8] = BEM_PE_OPERATION;
     prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf)
                                             :
                                              (antialias ? 
                                               (BLE_FUNC_AxB_plus_CxD |
                                                BLE_SRCF_ONE |
                                                BLE_SRCA_FIXED |
                                                BLE_DSTF_1_SRC_A) : BLE_FUNC_NONE
                                              );

     prep[10] = BEM_WR_CTRL;
     prep[11] = WR_CTRL_LINE | ctrl;

     if (full) {
          prep[12] = BEM_WR_V2;
          prep[13] = SH7722_XY( x3, y3 );
          prep[14] = BEM_WR_CTRL;
          prep[15] = WR_CTRL_POLYLINE | ctrl;

          prep[16] = BEM_WR_V2;
          prep[17] = SH7722_XY( x4, y4 );
          prep[18] = BEM_WR_CTRL;
          prep[19] = WR_CTRL_POLYLINE | ctrl;

          prep[20] = BEM_WR_V2;
          prep[21] = SH7722_XY( x1, y1 );
          prep[22] = BEM_WR_CTRL;
          prep[23] = WR_CTRL_POLYLINE | ctrl;

          submit_buffer( sdrv, 24 );
     }
     else {
          prep[7]   = SH7722_XY( x3, y3 );
          prep[11] |= WR_CTRL_ENDPOINT;

          submit_buffer( sdrv, 12 );
     }
}

/*
 * Render a filled rectangle using the current hardware state.
 */
static bool
sh7722FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 8 );

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     prep[0] = BEM_BE_V1;
     prep[1] = SH7722_XY( rect->x, rect->y );

     prep[2] = BEM_BE_V2;
     prep[3] = SH7722_XY( rect->w, rect->h );

     prep[4] = BEM_PE_OPERATION;
     prep[5] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[6] = BEM_BE_CTRL;
     prep[7] = BE_CTRL_RECTANGLE | BE_CTRL_SCANMODE_LINE;

     submit_buffer( sdrv, 8 );

     return true;
}

/*
 * This version sends a quadrangle to have all four edges transformed.
 */
static bool
sh7722FillRectangleMatrixAA( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();


     if (sdev->render_options & DSRO_ANTIALIAS) {
          int x1 = rect->x;
          int y1 = rect->y;
          int x2 = rect->x + rect->w;
          int y2 = rect->y;
          int x3 = rect->x + rect->w;
          int y3 = rect->y + rect->h;
          int x4 = rect->x;
          int y4 = rect->y + rect->h;

          if (sdev->render_options & DSRO_MATRIX) {
               int t;
     
               t  = ((x1 * sdev->matrix[0]) +
                     (y1 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y1 = ((x1 * sdev->matrix[3]) +
                     (y1 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
               x1 = t;
     
               t  = ((x2 * sdev->matrix[0]) +
                     (y2 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y2 = ((x2 * sdev->matrix[3]) +
                     (y2 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
               x2 = t;
     
               t  = ((x3 * sdev->matrix[0]) +
                     (y3 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y3 = ((x3 * sdev->matrix[3]) +
                     (y3 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
               x3 = t;
     
               t  = ((x4 * sdev->matrix[0]) +
                     (y4 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y4 = ((x4 * sdev->matrix[3]) +
                     (y4 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
               x4 = t;
          }
     
          prep = start_buffer( sdrv, 28 );

          prep[0] = BEM_WR_FGC;
          prep[1] = PIXEL_ARGB( (sdev->color.a * AA_COEF) >> 8,
                                (sdev->color.r * AA_COEF) >> 8,
                                (sdev->color.g * AA_COEF) >> 8,
                                (sdev->color.b * AA_COEF) >> 8 );

          prep[2] = BEM_PE_FIXEDALPHA;
          prep[3] = (sdev->color.a * AA_COEF) << 16;

          prep[4] = BEM_WR_V1;
          prep[5] = SH7722_XY( x1, y1 );

          prep[6] = BEM_WR_V2;
          prep[7] = SH7722_XY( x2, y2 );

          prep[8] = BEM_PE_OPERATION;
          prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                     sdev->ble_srcf |
                                                     BLE_SRCA_FIXED |
                                                     sdev->ble_dstf)
                                                  :
                                                    (BLE_FUNC_AxB_plus_CxD |
                                                     BLE_SRCF_ONE |
                                                     BLE_SRCA_FIXED |
                                                     BLE_DSTF_1_SRC_A);

          prep[10] = BEM_WR_CTRL;
          prep[11] = WR_CTRL_LINE | WR_CTRL_ANTIALIAS;

          if (rect->h > 1 && rect->w > 1) {
               prep[12] = BEM_WR_V2;
               prep[13] = SH7722_XY( x3, y3 );
               prep[14] = BEM_WR_CTRL;
               prep[15] = WR_CTRL_POLYLINE | WR_CTRL_ANTIALIAS;

               prep[16] = BEM_WR_V2;
               prep[17] = SH7722_XY( x4, y4 );
               prep[18] = BEM_WR_CTRL;
               prep[19] = WR_CTRL_POLYLINE | WR_CTRL_ANTIALIAS;

               prep[20] = BEM_WR_V2;
               prep[21] = SH7722_XY( x1, y1 );
               prep[22] = BEM_WR_CTRL;
               prep[23] = WR_CTRL_POLYLINE | WR_CTRL_ANTIALIAS;

               prep[24] = BEM_WR_FGC;
               prep[25] = PIXEL_ARGB( sdev->color.a,
                                      sdev->color.r,
                                      sdev->color.g,
                                      sdev->color.b );

               prep[26] = BEM_PE_FIXEDALPHA;
               prep[27] = (sdev->color.a << 24) << (sdev->color.a << 16);

               submit_buffer( sdrv, 28 );
          }
          else {
               prep[7]   = SH7722_XY( x3, y3 );
               prep[11] |= WR_CTRL_ENDPOINT;

               prep[12] = BEM_WR_FGC;
               prep[13] = PIXEL_ARGB( sdev->color.a,
                                     sdev->color.r,
                                     sdev->color.g,
                                     sdev->color.b );

               prep[14] = BEM_PE_FIXEDALPHA;
               prep[15] = (sdev->color.a << 24) << (sdev->color.a << 16);

               submit_buffer( sdrv, 16 );
          }
     }


     prep = start_buffer( sdrv, 12 );

     prep[0] = BEM_BE_V1;
     prep[1] = SH7722_XY( rect->x, rect->y );

     prep[2] = BEM_BE_V2;
     prep[3] = SH7722_XY( rect->x, rect->y + rect->h );

     prep[4] = BEM_BE_V3;
     prep[5] = SH7722_XY( rect->x + rect->w, rect->y );

     prep[6] = BEM_BE_V4;
     prep[7] = SH7722_XY( rect->x + rect->w, rect->y + rect->h );

     prep[8] = BEM_PE_OPERATION;
     prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[10] = BEM_BE_CTRL;

     if (sdev->render_options & DSRO_MATRIX)
          prep[11] = BE_CTRL_QUADRANGLE | BE_CTRL_SCANMODE_4x4 |
                     BE_CTRL_MATRIX | BE_CTRL_FIXMODE_16_16;// | BE_CTRL_ORIGIN;
     else
          prep[11] = BE_CTRL_QUADRANGLE | BE_CTRL_SCANMODE_LINE;

     submit_buffer( sdrv, 12 );

     return true;
}

/*
 * Render a filled triangle using the current hardware state.
 */
bool
sh7722FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep;

     D_DEBUG_AT( SH7722_BLT, "%s( %d,%d - %d,%d - %d,%d )\n", __FUNCTION__,
                 tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3 );
     DUMP_INFO();


     if (sdev->render_options & DSRO_ANTIALIAS) {
          int x1, y1;
          int x2, y2;
          int x3, y3;

          if (sdev->render_options & DSRO_MATRIX) {
               x1 = ((tri->x1 * sdev->matrix[0]) +
                     (tri->y1 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y1 = ((tri->x1 * sdev->matrix[3]) +
                     (tri->y1 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;

               x2 = ((tri->x2 * sdev->matrix[0]) +
                     (tri->y2 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y2 = ((tri->x2 * sdev->matrix[3]) +
                     (tri->y2 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;

               x3 = ((tri->x3 * sdev->matrix[0]) +
                     (tri->y3 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
               y3 = ((tri->x3 * sdev->matrix[3]) +
                     (tri->y3 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
          }
          else {
               x1 = tri->x1;
               y1 = tri->y1;
               x2 = tri->x2;
               y2 = tri->y2;
               x3 = tri->x3;
               y3 = tri->y3;
          }

          prep = start_buffer( sdrv, 24 );

          prep[0] = BEM_WR_FGC;
          prep[1] = PIXEL_ARGB( (sdev->color.a * AA_COEF) >> 8,
                                (sdev->color.r * AA_COEF) >> 8,
                                (sdev->color.g * AA_COEF) >> 8,
                                (sdev->color.b * AA_COEF) >> 8 );

          prep[2] = BEM_PE_FIXEDALPHA;
          prep[3] = (sdev->color.a * AA_COEF) << 16;

          prep[4] = BEM_WR_V1;
          prep[5] = SH7722_XY( x1, y1 );

          prep[6] = BEM_WR_V2;
          prep[7] = SH7722_XY( x2, y2 );

          prep[8] = BEM_PE_OPERATION;
          prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                     sdev->ble_srcf |
                                                     BLE_SRCA_FIXED |
                                                     sdev->ble_dstf)
                                                  :
                                                    (BLE_FUNC_AxB_plus_CxD |
                                                     BLE_SRCF_ONE |
                                                     BLE_SRCA_FIXED |
                                                     BLE_DSTF_1_SRC_A);

          prep[10] = BEM_WR_CTRL;
          prep[11] = WR_CTRL_LINE | WR_CTRL_ANTIALIAS;

          prep[12] = BEM_WR_V2;
          prep[13] = SH7722_XY( x3, y3 );
          prep[14] = BEM_WR_CTRL;
          prep[15] = WR_CTRL_POLYLINE | WR_CTRL_ANTIALIAS;

          prep[16] = BEM_WR_V2;
          prep[17] = SH7722_XY( x1, y1 );
          prep[18] = BEM_WR_CTRL;
          prep[19] = WR_CTRL_POLYLINE | WR_CTRL_ANTIALIAS;

          prep[20] = BEM_WR_FGC;
          prep[21] = PIXEL_ARGB( sdev->color.a,
                                 sdev->color.r,
                                 sdev->color.g,
                                 sdev->color.b );

          prep[22] = BEM_PE_FIXEDALPHA;
          prep[23] = (sdev->color.a << 24) << (sdev->color.a << 16);

          submit_buffer( sdrv, 24 );
     }


     prep = start_buffer( sdrv, 12 );

     prep[0] = BEM_BE_V1;
     prep[1] = SH7722_XY( tri->x1, tri->y1 );

     prep[2] = BEM_BE_V2;
     prep[3] = SH7722_XY( tri->x2, tri->y2 );

     prep[4] = BEM_BE_V3;
     prep[5] = SH7722_XY( tri->x3, tri->y3 );

     prep[6] = BEM_BE_V4;
     prep[7] = SH7722_XY( tri->x3, tri->y3 );

     prep[8] = BEM_PE_OPERATION;
     prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[10] = BEM_BE_CTRL;
     prep[11] = BE_CTRL_QUADRANGLE | BE_CTRL_SCANMODE_LINE;

     if (sdev->render_options & DSRO_MATRIX)
          prep[11] |= BE_CTRL_MATRIX | BE_CTRL_FIXMODE_16_16;// | BE_CTRL_ORIGIN;

     submit_buffer( sdrv, 12 );

     return true;
}

/*
 * Render rectangle outlines using the current hardware state.
 */
static bool
sh7722DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 20 );

     int x1 = rect->x;
     int y1 = rect->y;
     int x2 = rect->x + rect->w;
     int y2 = rect->y + rect->h;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     prep[0] = BEM_WR_V1;
     prep[1] = (y1 << 16) | x1;

     prep[2] = BEM_WR_V2;
     prep[3] = (y1 << 16) | x2;

     prep[4] = BEM_PE_OPERATION;
     prep[5] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[6] = BEM_WR_CTRL;
     prep[7] = WR_CTRL_LINE;

     if (rect->h > 1 && rect->w > 1) {
          prep[8]  = BEM_WR_V2;
          prep[9]  = (y2 << 16) | x2;
          prep[10] = BEM_WR_CTRL;
          prep[11] = WR_CTRL_POLYLINE;

          prep[12] = BEM_WR_V2;
          prep[13] = (y2 << 16) | x1;
          prep[14] = BEM_WR_CTRL;
          prep[15] = WR_CTRL_POLYLINE;

          prep[16] = BEM_WR_V2;
          prep[17] = (y1 << 16) | x1;
          prep[18] = BEM_WR_CTRL;
          prep[19] = WR_CTRL_POLYLINE;

          submit_buffer( sdrv, 20 );
     }
     else {
          prep[3]  = (y2 << 16) | x2;
          prep[7] |= WR_CTRL_ENDPOINT;

          submit_buffer( sdrv, 8 );
     }

     return true;
}

static bool
sh7722DrawRectangleMatrixAA( void *drv, void *dev, DFBRectangle *rect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;

     int x1 = rect->x;
     int y1 = rect->y;
     int x2 = rect->x + rect->w;
     int y2 = rect->y;
     int x3 = rect->x + rect->w;
     int y3 = rect->y + rect->h;
     int x4 = rect->x;
     int y4 = rect->y + rect->h;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     if (sdev->render_options & DSRO_MATRIX) {
          int t;

          t  = ((x1 * sdev->matrix[0]) +
                (y1 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y1 = ((x1 * sdev->matrix[3]) +
                (y1 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
          x1 = t;

          t  = ((x2 * sdev->matrix[0]) +
                (y2 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y2 = ((x2 * sdev->matrix[3]) +
                (y2 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
          x2 = t;

          t  = ((x3 * sdev->matrix[0]) +
                (y3 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y3 = ((x3 * sdev->matrix[3]) +
                (y3 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
          x3 = t;

          t  = ((x4 * sdev->matrix[0]) +
                (y4 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y4 = ((x4 * sdev->matrix[3]) +
                (y4 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
          x4 = t;
     }

     if (sdev->render_options & DSRO_ANTIALIAS)
          draw_rectangle( sdrv, sdev, x1, y1, x2, y2, x3, y3, x4, y4, true,  rect->h > 1 && rect->w > 1 );

     draw_rectangle( sdrv, sdev, x1, y1, x2, y2, x3, y3, x4, y4, false, rect->h > 1 && rect->w > 1 );

     return true;
}

/*
 * Render a line using the current hardware state.
 */
static bool
sh7722DrawLine( void *drv, void *dev, DFBRegion *line )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 8 );

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d -> %d, %d )\n", __FUNCTION__,
                 DFB_REGION_VALS( line ) );
     DUMP_INFO();

     prep[0] = BEM_WR_V1;
     prep[1] = SH7722_XY( line->x1, line->y1 );

     prep[2] = BEM_WR_V2;
     prep[3] = SH7722_XY( line->x2, line->y2 );

     prep[4] = BEM_PE_OPERATION;
     prep[5] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[6] = BEM_WR_CTRL;
     prep[7] = WR_CTRL_LINE | WR_CTRL_ENDPOINT;

     submit_buffer( sdrv, 8 );

     return true;
}

static bool
sh7722DrawLineMatrix( void *drv, void *dev, DFBRegion *line )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 8 );

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d -> %d, %d )\n", __FUNCTION__,
                 DFB_REGION_VALS( line ) );
     DUMP_INFO();

     int x1 = ((line->x1 * sdev->matrix[0]) +
               (line->y1 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
     int y1 = ((line->x1 * sdev->matrix[3]) +
               (line->y1 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;

     int x2 = ((line->x2 * sdev->matrix[0]) +
               (line->y2 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
     int y2 = ((line->x2 * sdev->matrix[3]) +
               (line->y2 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;

     prep[0] = BEM_WR_V1;
     prep[1] = SH7722_XY( x1, y1 );

     prep[2] = BEM_WR_V2;
     prep[3] = SH7722_XY( x2, y2 );

     prep[4] = BEM_PE_OPERATION;
     prep[5] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[6] = BEM_WR_CTRL;
     prep[7] = WR_CTRL_LINE | WR_CTRL_ENDPOINT;

     submit_buffer( sdrv, 8 );

     return true;
}

static bool
sh7722DrawLineAA( void *drv, void *dev, DFBRegion *line )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;
     __u32            *prep = start_buffer( sdrv, 24 );
     int               x1, y1;
     int               x2, y2;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d -> %d, %d )\n", __FUNCTION_s_,
                 DFB_REGION_VALS( line ) );
     DUMP_INFO();

     if (sdev->render_options & DSRO_MATRIX) {
          x1 = ((line->x1 * sdev->matrix[0]) +
                (line->y1 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y1 = ((line->x1 * sdev->matrix[3]) +
                (line->y1 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;

          x2 = ((line->x2 * sdev->matrix[0]) +
                (line->y2 * sdev->matrix[1]) + sdev->matrix[2]) >> 16;
          y2 = ((line->x2 * sdev->matrix[3]) +
                (line->y2 * sdev->matrix[4]) + sdev->matrix[5]) >> 16;
     }
     else {
          x1 = line->x1;
          y1 = line->y1;
          x2 = line->x2;
          y2 = line->y2;
     }

     prep[0] = BEM_WR_FGC;
     prep[1] = PIXEL_ARGB( (sdev->color.a * AA_COEF) >> 8,
                           (sdev->color.r * AA_COEF) >> 8,
                           (sdev->color.g * AA_COEF) >> 8,
                           (sdev->color.b * AA_COEF) >> 8 );

     prep[2] = BEM_PE_FIXEDALPHA;
     prep[3] = (sdev->color.a * AA_COEF) << 16;

     prep[4] = BEM_WR_V1;
     prep[5] = SH7722_XY( x1, y1 );

     prep[6] = BEM_WR_V2;
     prep[7] = SH7722_XY( x2, y2 );

     prep[8] = BEM_PE_OPERATION;
     prep[9] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                sdev->ble_srcf |
                                                BLE_SRCA_FIXED |
                                                sdev->ble_dstf)
                                             :
                                               (BLE_FUNC_AxB_plus_CxD |
                                                BLE_SRCF_ONE |
                                                BLE_SRCA_FIXED |
                                                BLE_DSTF_1_SRC_A);

     prep[10] = BEM_WR_CTRL;
     prep[11] = WR_CTRL_LINE | WR_CTRL_ENDPOINT | WR_CTRL_ANTIALIAS;



     prep[12] = BEM_WR_FGC;
     prep[13] = PIXEL_ARGB( sdev->color.a,
                            sdev->color.r,
                            sdev->color.g,
                            sdev->color.b );

     prep[14] = BEM_PE_FIXEDALPHA;
     prep[15] = (sdev->color.a << 24) | (sdev->color.a << 16);

     prep[16] = BEM_WR_V1;
     prep[17] = SH7722_XY( x1, y1 );

     prep[18] = BEM_WR_V2;
     prep[19] = SH7722_XY( x2, y2 );

     prep[20] = BEM_PE_OPERATION;
     prep[21] = (sdev->dflags & DSDRAW_BLEND) ? (BLE_FUNC_AxB_plus_CxD |
                                                 sdev->ble_srcf |
                                                 BLE_SRCA_FIXED |
                                                 sdev->ble_dstf) : BLE_FUNC_NONE;

     prep[22] = BEM_WR_CTRL;
     prep[23] = WR_CTRL_LINE | WR_CTRL_ENDPOINT;


     submit_buffer( sdrv, 24 );

     return true;
}

/*
 * Common implementation for Blit() and StretchBlit().
 */
static inline bool
sh7722DoBlit( SH7722DriverData *sdrv, SH7722DeviceData *sdev,
              DFBRectangle *rect, int x, int y, int w, int h )
{
     __u32 *prep = start_buffer( sdrv, 12 );

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

     prep[0] = BEM_BE_SRC_LOC;
     prep[1] = SH7722_XY( rect->x, rect->y );

     prep[2] = BEM_BE_SRC_SIZE;
     prep[3] = SH7722_XY( rect->w, rect->h );

     prep[4] = BEM_BE_V1;
     prep[5] = SH7722_XY( x, y );

     prep[6] = BEM_BE_V2;
     prep[7] = SH7722_XY( w, h );

     prep[8] = BEM_PE_OPERATION;
     prep[9] = BLE_FUNC_NONE;

     if (sdev->bflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          prep[9] |= BLE_FUNC_AxB_plus_CxD | sdev->ble_srcf | sdev->ble_dstf;

          switch (sdev->bflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               case DSBLIT_BLEND_ALPHACHANNEL:
                    prep[9] |= BLE_SRCA_SOURCE_ALPHA;
                    break;

               case DSBLIT_BLEND_COLORALPHA:
                    prep[9] |= BLE_SRCA_FIXED;
                    break;

               case DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA:
                    prep[9] |= BLE_SRCA_ALPHA_CHANNEL;      /* does not work */
                    break;
          }
     }

     prep[10] = BEM_BE_CTRL;
     prep[11] = BE_CTRL_RECTANGLE | BE_CTRL_TEXTURE | BE_CTRL_SCANMODE_4x4;

     if (sdev->bflags & DSBLIT_ROTATE180)
          prep[11] |= BE_FLIP_BOTH;
     else if (rect->w == w && rect->h == h)  /* No blit direction handling for StretchBlit(). */
          prep[11] |= BE_CTRL_BLTDIR_AUTOMATIC;

     submit_buffer( sdrv, 12 );

     return true;
}

/*
 * This version sends a quadrangle to have all four edges transformed.
 */
static inline bool
sh7722DoBlitM( SH7722DriverData *sdrv, SH7722DeviceData *sdev,
               DFBRectangle *rect, int x1, int y1, int x2, int y2 )
{
     __u32 *prep = start_buffer( sdrv, 16 );

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

     prep[0]  = BEM_BE_SRC_LOC;
     prep[1]  = SH7722_XY( rect->x, rect->y );
             
     prep[2]  = BEM_BE_SRC_SIZE;
     prep[3]  = SH7722_XY( rect->w, rect->h );
             
     prep[4]  = BEM_BE_V1;
     prep[5]  = SH7722_XY( x1, y1 );
             
     prep[6]  = BEM_BE_V2;
     prep[7]  = SH7722_XY( x1, y2 );
             
     prep[8]  = BEM_BE_V3;
     prep[9]  = SH7722_XY( x2, y1 );

     prep[10] = BEM_BE_V4;
     prep[11] = SH7722_XY( x2, y2 );

     prep[12] = BEM_PE_OPERATION;
     prep[13] = BLE_FUNC_NONE;

     if (sdev->bflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          prep[13] |= BLE_FUNC_AxB_plus_CxD | sdev->ble_srcf | sdev->ble_dstf;

          switch (sdev->bflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               case DSBLIT_BLEND_ALPHACHANNEL:
                    prep[13] |= BLE_SRCA_SOURCE_ALPHA;
                    break;

               case DSBLIT_BLEND_COLORALPHA:
                    prep[13] |= BLE_SRCA_FIXED;
                    break;

               case DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA:
                    prep[13] |= BLE_SRCA_ALPHA_CHANNEL;      /* does not work */
                    break;
          }
     }

     prep[14] = BEM_BE_CTRL;
     prep[15] = BE_CTRL_QUADRANGLE | BE_CTRL_TEXTURE | BE_CTRL_SCANMODE_4x4 |
                BE_CTRL_MATRIX | BE_CTRL_FIXMODE_16_16;// | BE_CTRL_ORIGIN;

     if (sdev->bflags & DSBLIT_ROTATE180)
          prep[15] |= BE_FLIP_BOTH;

     submit_buffer( sdrv, 16 );

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
sh7722Blit( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( rect ), x, y );

     if (sdev->render_options & DSRO_MATRIX)
          return sh7722DoBlitM( sdrv, sdev, rect, DFB_REGION_VALS_FROM_RECTANGLE_VALS( x, y, rect->w, rect->h ) );

     return sh7722DoBlit( sdrv, sdev, rect, x, y, rect->w, rect->h );
}

/*
 * StretchBlit a rectangle using the current hardware state.
 */
bool
sh7722StretchBlit( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect )
{
     SH7722DriverData *sdrv = drv;
     SH7722DeviceData *sdev = dev;

     D_DEBUG_AT( SH7722_BLT, "%s( %d, %d - %dx%d  -> %d, %d - %dx%d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( srect ), DFB_RECTANGLE_VALS( drect ) );

     if (sdev->render_options & DSRO_MATRIX)
          return sh7722DoBlitM( sdrv, sdev, srect, DFB_REGION_VALS_FROM_RECTANGLE( drect ) );

     return sh7722DoBlit( sdrv, sdev, srect, drect->x, drect->y, drect->w, drect->h );
}

