/*
   TI Davinci driver - 2D Acceleration

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/state.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include "davinci_2d.h"
#include "davinci_gfxdriver.h"


D_DEBUG_DOMAIN( Davinci_2D, "Davinci/2D", "Davinci 2D Acceleration" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DESTINATION  = 0x00000001,
     COLOR        = 0x00000002,

     SOURCE       = 0x00000010,

     BLEND_SUB    = 0x00010000,

     ALL          = 0x00010013
};

/*
 * State handling macros.
 */

#define DAVINCI_VALIDATE(flags)        do { ddev->v_flags |=  (flags); } while (0)
#define DAVINCI_INVALIDATE(flags)      do { ddev->v_flags &= ~(flags); } while (0)

#define DAVINCI_CHECK_VALIDATE(flag)   do {                                               \
                                            if (! (ddev->v_flags & flag))                 \
                                                davinci_validate_##flag( ddev, state );   \
                                       } while (0)

/**************************************************************************************************/

static bool davinciFillRectangle16( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *rect );

static bool davinciFillRectangle32( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *rect );

static bool davinciBlit16         ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciBlit32to16     ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciBlit32         ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciBlitKeyed16    ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciBlitKeyed32    ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciBlitBlend32    ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    int                  dx,
                                    int                  dy );

static bool davinciStretchBlit32  ( void                *drv,
                                    void                *dev,
                                    DFBRectangle        *srect,
                                    DFBRectangle        *drect );

/**************************************************************************************************/

static inline int
get_blend_sub_function( const CardState *state )
{
     DFBSurfaceBlittingFlags flags = state->blittingflags & ~DSBLIT_COLORIZE;

     if (state->dst_blend == DSBF_INVSRCALPHA) {
          switch (state->src_blend) {
               case DSBF_SRCALPHA:
                    if (flags == DSBLIT_BLEND_ALPHACHANNEL)
                         return 2;
                    break;

               case DSBF_ONE:
                    switch (flags) {
                         case DSBLIT_BLEND_ALPHACHANNEL:
                              return 1;

                         case DSBLIT_BLEND_ALPHACHANNEL |
                              DSBLIT_SRC_PREMULTIPLY:
                              return 0;

                         case DSBLIT_BLEND_ALPHACHANNEL |
                              DSBLIT_BLEND_COLORALPHA |
                              DSBLIT_SRC_PREMULTCOLOR:
                              return 3;

                         default:
                              break;
                    }
                    break;

               default:
                    break;
          }
     }

     return -1;
}

/**************************************************************************************************/

/*
 * Called by davinciSetState() to ensure that the destination registers are properly set
 * for execution of rendering functions.
 */
static inline void
davinci_validate_DESTINATION( DavinciDeviceData *ddev,
                              CardState         *state )
{
     /* Remember destination parameters for usage in rendering functions. */
     ddev->dst_addr   = state->dst.addr;
     ddev->dst_phys   = state->dst.phys;
     ddev->dst_pitch  = state->dst.pitch;
     ddev->dst_format = state->dst.buffer->format;
     ddev->dst_bpp    = DFB_BYTES_PER_PIXEL( ddev->dst_format );

     /* Set the flag. */
     DAVINCI_VALIDATE( DESTINATION );
}

/*
 * Called by davinciSetState() to ensure that the color register is properly set
 * for execution of rendering functions.
 */
static inline void
davinci_validate_COLOR( DavinciDeviceData *ddev,
                        CardState         *state )
{
     switch (ddev->dst_format) {
          case DSPF_ARGB:
               ddev->color_pixel = PIXEL_ARGB( state->color.a,
                                               state->color.r,
                                               state->color.g,
                                               state->color.b );
               break;

          case DSPF_RGB32:
               ddev->color_pixel = PIXEL_RGB32( state->color.r,
                                                state->color.g,
                                                state->color.b );
               break;

          case DSPF_RGB16:
               ddev->color_pixel = PIXEL_RGB16( state->color.r,
                                                state->color.g,
                                                state->color.b );
               break;

          case DSPF_UYVY: {
               int y, u, v;

               RGB_TO_YCBCR( state->color.r, state->color.g, state->color.b, y, u, v );

               ddev->color_pixel = PIXEL_UYVY( y, u, v );
               break;
          }

          default:
               D_BUG( "unexpected format %s", dfb_pixelformat_name(ddev->dst_format) );
               return;
     }

     if (DFB_BYTES_PER_PIXEL(ddev->dst_format) < 4)
          ddev->color_pixel |= ddev->color_pixel << 16;

     if (DFB_BYTES_PER_PIXEL(ddev->dst_format) < 2)
          ddev->color_pixel |= ddev->color_pixel << 8;

     /* Set the flag. */
     DAVINCI_VALIDATE( COLOR );
}

/*
 * Called by davinciSetState() to ensure that the source registers are properly set
 * for execution of blitting functions.
 */
static inline void
davinci_validate_SOURCE( DavinciDeviceData *ddev,
                         CardState         *state )
{
     /* Remember source parameters for usage in rendering functions. */
     ddev->src_addr   = state->src.addr;
     ddev->src_phys   = state->src.phys;
     ddev->src_pitch  = state->src.pitch;
     ddev->src_format = state->src.buffer->format;
     ddev->src_bpp    = DFB_BYTES_PER_PIXEL( ddev->src_format );

     /* Set the flag. */
     DAVINCI_VALIDATE( SOURCE );
}

/*
 * Called by davinciSetState() to ensure that the blend sub function index is valid
 * for execution of blitting functions.
 */
static inline void
davinci_validate_BLEND_SUB( DavinciDeviceData *ddev,
                            CardState         *state )
{
     int index = get_blend_sub_function( state );

     if (index < 0) {
          D_BUG( "unexpected state" );
          return;
     }

     /* Set blend sub function index. */
     ddev->blend_sub_function = index;

     /* Set the flag. */
     DAVINCI_VALIDATE( BLEND_SUB );
}

/**************************************************************************************************/

/*
 * Wait for the blitter to be idle.
 *
 * This function is called before memory that has been written to by the hardware is about to be
 * accessed by the CPU (software driver) or another hardware entity like video encoder (by Flip()).
 * It can also be called by applications explicitly, e.g. at the end of a benchmark loop to include
 * execution time of queued commands in the measurement.
 */
DFBResult
davinciEngineSync( void *drv, void *dev )
{
     DFBResult          ret;
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     if (!ddev->synced) {
          ret = davinci_c64x_wait_low( &ddrv->c64x );
          if (ret)
               return ret;

          ddev->synced = true;
     }

     return DFB_OK;
}

/*
 * Reset the graphics engine.
 */
void
davinciEngineReset( void *drv, void *dev )
{
}

/*
 * Start processing of queued commands if required.
 *
 * This function is called before returning from the graphics core to the application.
 * Usually that's after each rendering function. The only functions causing multiple commands
 * to be queued with a single emition at the end are DrawString(), TileBlit(), BatchBlit(),
 * DrawLines() and possibly FillTriangle() which is emulated using multiple FillRectangle() calls.
 */
void
davinciEmitCommands( void *drv, void *dev )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_write_back_all( &ddrv->c64x );

     ddev->synced = false;
}

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
davinciCheckState( void                *drv,
                   void                *dev,
                   CardState           *state,
                   DFBAccelerationMask  accel )
{
     D_DEBUG_AT( Davinci_2D, "davinciCheckState (state %p, accel 0x%08x) <- dest %p\n",
                 state, accel, state->destination );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(DAVINCI_SUPPORTED_DRAWINGFUNCTIONS | DAVINCI_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_UYVY:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;

          default:
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~DAVINCI_SUPPORTED_DRAWINGFLAGS)
               return;
     }
     else {
          /* Return if unsupported blitting flags are set. */
          if (state->blittingflags & ~DAVINCI_SUPPORTED_BLITTINGFLAGS)
               return;

          /* No other flags supported when color keying is used. */
          if ((state->blittingflags & DSBLIT_SRC_COLORKEY) && state->blittingflags != DSBLIT_SRC_COLORKEY)
               return;

          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_UYVY:
               case DSPF_RGB16:
                    /* Only color keying for these formats. */
                    if (state->blittingflags & ~DSBLIT_SRC_COLORKEY)
                         return;
                    /* No format conversion supported. */
                    if (state->source->config.format != state->destination->config.format)
                         return;
                    break;

               case DSPF_RGB32:
                    /* Only color keying for these formats. */
                    if (state->blittingflags & ~DSBLIT_SRC_COLORKEY)
                         return;
                    /* fall through */
               case DSPF_ARGB:
                    /* Only few blending combinations are valid. */
                    if ((state->blittingflags & ~DSBLIT_SRC_COLORKEY) && get_blend_sub_function( state ) < 0)
                         return;
                    /* Only ARGB/RGB32 -> RGB16 conversion (without any flag). */
                    if (state->source->config.format != state->destination->config.format &&
                        (state->destination->config.format != DSPF_RGB16 || state->blittingflags))
                         return;
                    break;

               default:
                    return;
          }

          /* Checks per function. */
          switch (accel) {
               case DFXL_STRETCHBLIT:
                    /* No flags supported with StretchBlit(). */
                    if (state->blittingflags)
                         return;

                    /* Only (A)RGB at 32 bit supported. */
                    if (state->source->config.format != DSPF_ARGB && state->source->config.format != DSPF_RGB32)
                         return;

                    break;

               default:
                    break;
          }
     }

     /* Enable acceleration of the function. */
     state->accel |= accel;
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
davinciSetState( void                *drv,
                 void                *dev,
                 GraphicsDeviceFuncs *funcs,
                 CardState           *state,
                 DFBAccelerationMask  accel )
{
     DavinciDeviceData      *ddev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( Davinci_2D, "davinciSetState (state %p, accel 0x%08x) <- dest %p, modified 0x%08x\n",
                 state, accel, state->destination, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          DAVINCI_INVALIDATE( ALL );
     }
     else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               DAVINCI_INVALIDATE( DESTINATION | COLOR );
          else if (modified & SMF_COLOR)
               DAVINCI_INVALIDATE( COLOR );

          if (modified & SMF_SOURCE)
               DAVINCI_INVALIDATE( SOURCE );

          if (modified & (SMF_BLITTING_FLAGS | SMF_SRC_BLEND | SMF_DST_BLEND))
               DAVINCI_INVALIDATE( BLEND_SUB );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination... */
     DAVINCI_CHECK_VALIDATE( DESTINATION );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               /* ...require valid drawing color. */
               DAVINCI_CHECK_VALIDATE( COLOR );

               /* Choose function. */
               switch (DFB_BYTES_PER_PIXEL( state->destination->config.format )) {
                    case 2:
                         funcs->FillRectangle = davinciFillRectangle16;
                         break;

                    case 4:
                         funcs->FillRectangle = davinciFillRectangle32;
                         break;

                    default:
                         D_BUG( "unexpected destination bpp %d",
                                DFB_BYTES_PER_PIXEL( state->destination->config.format ) );
                         break;
               }

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set |= DAVINCI_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
               /* ...require valid source. */
               DAVINCI_CHECK_VALIDATE( SOURCE );

               /* Validate blend sub function index for blending. */
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    DAVINCI_CHECK_VALIDATE( BLEND_SUB );

               /* Choose function. */
               switch (DFB_BYTES_PER_PIXEL( state->destination->config.format )) {
                    case 2:
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                              funcs->Blit = davinciBlitKeyed16;
                         else if (state->source->config.format == DSPF_ARGB ||
                                  state->source->config.format == DSPF_RGB32)
                              funcs->Blit = davinciBlit32to16;
                         else
                              funcs->Blit = davinciBlit16;
                         break;

                    case 4:
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                              funcs->Blit = davinciBlitKeyed32;
                         else if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                              funcs->Blit = davinciBlitBlend32;
                         else
                              funcs->Blit = davinciBlit32;
                         break;

                    default:
                         D_BUG( "unexpected destination bpp %d",
                                DFB_BYTES_PER_PIXEL( state->destination->config.format ) );
                         break;
               }

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set |= DFXL_BLIT;
               break;

          case DFXL_STRETCHBLIT:
               /* ...require valid source. */
               DAVINCI_CHECK_VALIDATE( SOURCE );

               /* Choose function. */
               switch (state->destination->config.format) {
                    case DSPF_ARGB:
                    case DSPF_RGB32:
                         funcs->StretchBlit = davinciStretchBlit32;
                         break;

                    default:
                         D_BUG( "unexpected destination format %s",
                                dfb_pixelformat_name( state->destination->config.format ) );
                         break;
               }

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set |= DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     ddev->blitting_flags = state->blittingflags;
     ddev->colorkey       = state->src_colorkey;
     ddev->color          = state->color;
     ddev->color_argb     = PIXEL_ARGB( state->color.a,
                                        state->color.r,
                                        state->color.g,
                                        state->color.b );
     ddev->clip           = state->clip;

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further validation.
      * If the hw independent state is not modified, this function won't get called
      * for subsequent rendering functions, unless they aren't defined by 3).
      */
     state->mod_hw = 0;
}

/*
 * Render a filled rectangle using the current hardware state.
 */
static bool
davinciFillRectangle16( void *drv, void *dev, DFBRectangle *rect )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     /* FIXME: Optimize in DSP. */
     if ((rect->x | rect->w) & 1)
          davinci_c64x_fill_16( &ddrv->c64x,
                                ddev->dst_phys + ddev->dst_pitch * rect->y + ddev->dst_bpp * rect->x,
                                ddev->dst_pitch,
                                rect->w, rect->h,
                                ddev->color_pixel );
     else
          davinci_c64x_fill_32( &ddrv->c64x,
                                ddev->dst_phys + ddev->dst_pitch * rect->y + ddev->dst_bpp * rect->x,
                                ddev->dst_pitch,
                                rect->w/2, rect->h,
                                ddev->color_pixel );

     return true;
}

static bool
davinciFillRectangle32( void *drv, void *dev, DFBRectangle *rect )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_fill_32( &ddrv->c64x,
                           ddev->dst_phys + ddev->dst_pitch * rect->y + ddev->dst_bpp * rect->x,
                           ddev->dst_pitch,
                           rect->w, rect->h,
                           ddev->color_pixel );

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
static bool
davinciBlit16( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     /* FIXME: Optimize in DSP. */
     if ((dx | rect->x | rect->w) & 1)
          davinci_c64x_blit_16( &ddrv->c64x,
                                ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                                ddev->dst_pitch,
                                ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                                ddev->src_pitch,
                                rect->w, rect->h );
     else
          davinci_c64x_blit_32( &ddrv->c64x,
                                ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                                ddev->dst_pitch,
                                ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                                ddev->src_pitch,
                                rect->w/2, rect->h );

     return true;
}

static bool
davinciBlit32to16( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_dither_argb( &ddrv->c64x,
                               ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                               0x8e000000,
                               ddev->dst_pitch,
                               ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                               ddev->src_pitch,
                               rect->w, rect->h );

     return true;
}

static bool
davinciBlit32( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_blit_32( &ddrv->c64x,
                           ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                           ddev->dst_pitch,
                           ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                           ddev->src_pitch,
                           rect->w, rect->h );

     return true;
}

static bool
davinciBlitKeyed16( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_blit_keyed_16( &ddrv->c64x,
                                 ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                                 ddev->dst_pitch,
                                 ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                                 ddev->src_pitch,
                                 rect->w, rect->h,
                                 ddev->colorkey,
                                 (1 << DFB_COLOR_BITS_PER_PIXEL( ddev->dst_format )) - 1 );

     return true;
}

static bool
davinciBlitKeyed32( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_blit_keyed_32( &ddrv->c64x,
                                 ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                                 ddev->dst_pitch,
                                 ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                                 ddev->src_pitch,
                                 rect->w, rect->h,
                                 ddev->colorkey,
                                 (1 << DFB_COLOR_BITS_PER_PIXEL( ddev->dst_format )) - 1 );

     return true;
}

static bool
davinciBlitBlend32( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_blit_blend_32( &ddrv->c64x,
                                 ddev->dst_phys + ddev->dst_pitch * dy      + ddev->dst_bpp * dx,
                                 ddev->dst_pitch,
                                 ddev->src_phys + ddev->src_pitch * rect->y + ddev->src_bpp * rect->x,
                                 ddev->src_pitch,
                                 rect->w, rect->h,
                                 ddev->blend_sub_function,
                                 (ddev->blitting_flags & DSBLIT_COLORIZE) ?
                                   ddev->color_argb : (ddev->color_argb | 0xffffff) );

     return true;
}

static bool
davinciStretchBlit32( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect )
{
     DavinciDriverData *ddrv = drv;
     DavinciDeviceData *ddev = dev;

     davinci_c64x_stretch_32( &ddrv->c64x,
                              ddev->dst_phys + ddev->dst_pitch * drect->y + ddev->dst_bpp * drect->x,
                              ddev->dst_pitch,
                              ddev->src_phys + ddev->src_pitch * srect->y + ddev->src_bpp * srect->x,
                              ddev->src_pitch,
                              drect->w, drect->h,
                              srect->w, srect->h,
                              &ddev->clip );

     return true;
}

