/*
   PXA3xx Graphics Controller

   (c) Copyright 2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2009  Raumfeld GmbH (raumfeld.com)

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Sven Neumann <s.neumann@raumfeld.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

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

#define DUMP_INFO() D_DEBUG_AT( PXA3XX_BLT, "  -> %srunning\n",     \
                                            pdrv->gfx_shared->hw_running ? "" : "not " );

/**********************************************************************************************************************/

static bool pxa3xxFillRectangle     ( void *drv, void *dev, DFBRectangle *rect );
static bool pxa3xxFillRectangleBlend( void *drv, void *dev, DFBRectangle *rect );

static bool pxa3xxBlit               ( void *drv, void *dev, DFBRectangle *rect, int x, int y );
static bool pxa3xxBlitBlend          ( void *drv, void *dev, DFBRectangle *rect, int x, int y );
static bool pxa3xxBlitBlendColorAlpha( void *drv, void *dev, DFBRectangle *rect, int x, int y );
static bool pxa3xxBlitGlyph          ( void *drv, void *dev, DFBRectangle *rect, int x, int y );


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

__attribute__((noinline))
static DFBResult
flush_prepared( PXA3XXDriverData *pdrv )
{
     int result;

     D_DEBUG_AT( PXA3XX_BLT, "%s()\n", __FUNCTION__ );

     DUMP_INFO();

     D_ASSERT( pdrv->prep_num < PXA3XX_GCU_BUFFER_WORDS );
     D_ASSERT( pdrv->prep_num <= D_ARRAY_SIZE(pdrv->prep_buf) );

     /* Something prepared? */
     if (pdrv->prep_num) {
          result = write( pdrv->gfx_fd, pdrv->prep_buf, pdrv->prep_num * 4 );
          if (result < 0) {
               D_PERROR( "PXA3XX/BLT: write() failed!\n" );
               return DFB_IO;
          }

          pdrv->prep_num = 0;
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
     pdev->src_alpha = DFB_PIXELFORMAT_HAS_ALPHA( buffer->format );

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
     prep[1] = dfb_pixel_from_color( state->destination->config.format, &state->color );

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

          direct_log_printf( NULL, "  -> %srunning\n",
                             pdrv->gfx_shared->hw_running ? "" : "not " );

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
          const DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~PXA3XX_SUPPORTED_BLITTINGFLAGS)
               return;

          /* Return if the source format is not supported. */
          if (!pixel_formats[DFB_PIXELFORMAT_INDEX(state->source->config.format)])
               return;

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               if (DFB_PIXELFORMAT_HAS_ALPHA( state->destination->config.format ))
                    return;

               /* Rotated blits are not supported with blending. */
               if (flags & ~(DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE))
                    return;

               /* Blending with alpha from color value is not supported
                * in combination with other blending functions.
                */
               if (flags & DSBLIT_BLEND_COLORALPHA &&
                   flags != DSBLIT_BLEND_COLORALPHA)
                 return;

               /* Check blend functions. */
               if (!check_blend_functions( state ))
                    return;
          }

          /* Colorizing is only supported for rendering ARGB glyphs. */
          if (flags & DSBLIT_COLORIZE &&
              (flags != (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE) || ! (state->source->type & CSTF_FONT)))
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
               PXA3XX_INVALIDATE( DEST | COLOR );

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

               if (state->drawingflags & DSDRAW_BLEND)
                    funcs->FillRectangle = pxa3xxFillRectangleBlend;
               else
                    funcs->FillRectangle = pxa3xxFillRectangle;

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

               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL && pdev->src_alpha) {
                    if (state->blittingflags & DSBLIT_COLORIZE)
                         funcs->Blit = pxa3xxBlitGlyph;
                    else
                         funcs->Blit = pxa3xxBlitBlend;
               }
               else {
                    if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                         funcs->Blit = pxa3xxBlitBlendColorAlpha;
                    else
                         funcs->Blit = pxa3xxBlit;
               }

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
static bool
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
 * Render a blended rectangle using the current hardware state.
 *
 * As the hardware does not directly support this, we blit a single
 * pixel with blending.
 */
static bool
pxa3xxFillRectangleBlend( void *drv, void *dev, DFBRectangle *rect )
{
     PXA3XXDriverData *pdrv   = drv;
     PXA3XXDeviceData *pdev   = dev;
     u32              *prep   = start_buffer( pdrv, 22 );
     const u32         format = pixel_formats[DFB_PIXELFORMAT_INDEX( DSPF_ARGB )];

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     /* Set fake destination. */
     prep[0]  = 0x020000A2;
     prep[1]  = pdev->fake_phys;
     prep[2]  = (format << 19) | 4;

     /* Fill rectangle. */
     prep[3]  = 0x40000014 | (format << 8);
     prep[4]  = 0;
     prep[5]  = 0;
     prep[6]  = PXA3XX_WH( rect->w, 1 );
     prep[7]  = PIXEL_ARGB( pdev->color.a, pdev->color.r, pdev->color.g, pdev->color.b );

     /* Restore destination. */
     prep[8]  = 0x020000A2;
     prep[9]  = pdev->dst_phys;
     prep[10] = (pixel_formats[pdev->dst_index] << 19) | (pdev->dst_pitch << 5) | pdev->dst_bpp;

     /* Set fake buffer as source. */
     prep[11] = 0x02000002;
     prep[12] = pdev->fake_phys;
     prep[13] = (format << 19) | 4;

     /* Blit with blending. */
     prep[14] = 0x47000107;
     prep[15] = rect->x;
     prep[16] = rect->y;
     prep[17] = 0;
     prep[18] = 0;
     prep[19] = rect->x;
     prep[20] = rect->y;
     prep[21] = PXA3XX_WH( rect->w, rect->h );

     submit_buffer( pdrv, 22 );

     /* Clear the flag. */
     PXA3XX_INVALIDATE( SOURCE );

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
static bool
pxa3xxBlit( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     PXA3XXDriverData *pdrv     = drv;
     PXA3XXDeviceData *pdev     = dev;
     u32               rotation = 0;
     u32              *prep     = start_buffer( pdrv, 6 );

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

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

/* RASTER

     prep[0] = 0x4BCC0007;
     prep[1] = x;
     prep[2] = y;
     prep[3] = rect->x;
     prep[4] = rect->y;
     prep[5] = rect->x;
     prep[6] = rect->y;
     prep[7] = PXA3XX_WH( rect->w, rect->h );

     submit_buffer( pdrv, 8 );
 */

/* PATTERN

     prep[0] = 0x4C000006;
     prep[1] = x;
     prep[2] = y;
     prep[3] = rect->x;
     prep[4] = rect->y;
     prep[5] = PXA3XX_WH( rect->w, rect->h );
     prep[6] = PXA3XX_WH( rect->w, rect->h );

     submit_buffer( pdrv, 7 );
 */

/* BIAS

     prep[0] = 0x49000016;
     prep[1] = x;
     prep[2] = y;
     prep[3] = rect->x;
     prep[4] = rect->y;
     prep[5] = PXA3XX_WH( rect->w, rect->h );
     prep[6] = 0;

     submit_buffer( pdrv, 7 );
 */

     return true;
}

/*
 * Blit a rectangle with alpha blending using the current hardware state.
 */
static bool
pxa3xxBlitBlend( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     PXA3XXDriverData *pdrv = drv;
     u32              *prep = start_buffer( pdrv, 8 );

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

     prep[0] = 0x47000107;
     prep[1] = x;
     prep[2] = y;
     prep[3] = rect->x;
     prep[4] = rect->y;
     prep[5] = x;
     prep[6] = y;
     prep[7] = PXA3XX_WH( rect->w, rect->h );

     submit_buffer( pdrv, 8 );

     return true;
}

/*
 * Blend a rectangle using the alpha value from the color using the current hardware state.
 */
static bool
pxa3xxBlitBlendColorAlpha( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     PXA3XXDriverData *pdrv = drv;
     PXA3XXDeviceData *pdev = dev;
     u32              *prep = start_buffer( pdrv, 9 );

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d  -> %d, %d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( rect ), x, y );
     DUMP_INFO();

     prep[0] = 0x47000138;
     prep[1] = x;
     prep[2] = y;
     prep[3] = rect->x;
     prep[4] = rect->y;
     prep[5] = x;
     prep[6] = y;
     prep[7] = PXA3XX_WH( rect->w, rect->h );
     prep[8] = (pdev->color.a << 24) | (pdev->color.a << 16);

     submit_buffer( pdrv, 9 );

     return true;
}

/*
 * Blit a glyph with alpha blending and colorizing using the current hardware state.
 */
static bool
pxa3xxBlitGlyph( void *drv, void *dev, DFBRectangle *rect, int x, int y )
{
     PXA3XXDriverData *pdrv   = drv;
     PXA3XXDeviceData *pdev   = dev;
     u32              *prep   = start_buffer( pdrv, 40 );
     const u32         format = pixel_formats[DFB_PIXELFORMAT_INDEX( DSPF_ARGB )];

     D_DEBUG_AT( PXA3XX_BLT, "%s( %d, %d - %dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ) );
     DUMP_INFO();

     if (rect->w * (rect->h + 1) * 4 > pdev->fake_size)
          return false;

     /* Set fake destination. */
     prep[0]  = 0x020000A2;
     prep[1]  = pdev->fake_phys;
     prep[2]  = (format << 19) | ((rect->w << 2) << 5) | 4;

     /* Fill first row of fake buffer. */
     prep[3]  = 0x40000014 | (format << 8);
     prep[4]  = 0;
     prep[5]  = 0;
     prep[6]  = PXA3XX_WH( rect->w, 1 );
     prep[7]  = PIXEL_ARGB( pdev->color.a, pdev->color.r, pdev->color.g, pdev->color.b );

     /* Set first row of fake buffer as source1. */
     prep[8]  = 0x02000012;
     prep[9]  = pdev->fake_phys;
     prep[10] = (format << 19) | 4;

     /* Blit with blending. */
     prep[11] = 0x47000118;
     prep[12] = 0;
     prep[13] = 1;
     prep[14] = rect->x;
     prep[15] = rect->y;
     prep[16] = 0;
     prep[17] = 0;
     prep[18] = PXA3XX_WH( rect->w, rect->h );
     prep[19] = 0;

     /* Restore destination. */
     prep[20] = 0x020000A2;
     prep[21] = pdev->dst_phys;
     prep[22] = (pixel_formats[pdev->dst_index] << 19) | (pdev->dst_pitch << 5) | pdev->dst_bpp;

     /* Restore source1 to destination. */
     prep[23] = 0x02000012;
     prep[24] = prep[21];
     prep[25] = prep[22];

     /* Set fake buffer as source0. */
     prep[26] = 0x02000002;
     prep[27] = pdev->fake_phys;
     prep[28] = (format << 19) | ((rect->w << 2) << 5) | 4;

     /* Blit with blending. */
     prep[29] = 0x47000107;
     prep[30] = x;
     prep[31] = y;
     prep[32] = 0;
     prep[33] = 1;
     prep[34] = x;
     prep[35] = y;
     prep[36] = PXA3XX_WH( rect->w, rect->h );

     /* Restore source0. */
     prep[37] = 0x02000002;
     prep[38] = pdev->src_phys;
     prep[39] = (pixel_formats[pdev->src_index] << 19) | (pdev->src_pitch << 5) | pdev->src_bpp;

     submit_buffer( pdrv, 40 );

     return true;
}
