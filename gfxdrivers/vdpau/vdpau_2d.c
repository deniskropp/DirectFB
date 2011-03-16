/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/state.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include "vdpau_2d.h"
#include "vdpau_gfxdriver.h"


D_DEBUG_DOMAIN( VDPAU_2D, "VDPAU/2D", "VDPAU 2D Acceleration" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DESTINATION  = 0x00000001,
     SOURCE       = 0x00000002,

     BLEND_DRAW   = 0x00000010,
     BLEND_BLIT   = 0x00000020,

     COLOR_DRAW   = 0x00000100,
     COLOR_BLIT   = 0x00000200,

     ALL          = 0x00000333
};

/*
 * State handling macros.
 */

#define VDPAU_VALIDATE(flags)        do { vdev->v_flags |=  (flags); } while (0)
#define VDPAU_INVALIDATE(flags)      do { vdev->v_flags &= ~(flags); } while (0)

#define VDPAU_CHECK_VALIDATE(flag)   do {                                             \
                                          if (! (vdev->v_flags & flag))               \
                                               vdpau_validate_##flag( vdev, state );  \
                                     } while (0)

/**********************************************************************************************************************/

static const VdpOutputSurfaceRenderBlendFactor blend_factors[] = {
     [DSBF_ZERO]         = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
     [DSBF_ONE]          = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
     [DSBF_SRCCOLOR]     = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_COLOR,
     [DSBF_INVSRCCOLOR]  = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
     [DSBF_SRCALPHA]     = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
     [DSBF_INVSRCALPHA]  = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
     [DSBF_DESTALPHA]    = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_DST_ALPHA,
     [DSBF_INVDESTALPHA] = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
     [DSBF_DESTCOLOR]    = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_DST_COLOR,
     [DSBF_INVDESTCOLOR] = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
     [DSBF_SRCALPHASAT]  = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA_SATURATE,
};

/**********************************************************************************************************************/

/*
 * Called by vdpauSetState() to ensure that the destination registers are properly set
 * for execution of rendering functions.
 */
static inline void
vdpau_validate_DESTINATION( VDPAUDeviceData *vdev,
                            CardState       *state )
{
     /* Remember destination parameters for usage in rendering functions. */
     vdev->dst        = (VdpOutputSurface) (unsigned long) state->dst.handle;
     vdev->dst_pitch  = state->dst.pitch;
     vdev->dst_format = state->dst.buffer->format;
     vdev->dst_bpp    = DFB_BYTES_PER_PIXEL( vdev->dst_format );

     /* Set the flag. */
     VDPAU_VALIDATE( DESTINATION );
}

/*
 * Called by vdpauSetState() to ensure that the source registers are properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_SOURCE( VDPAUDeviceData *vdev,
                       CardState       *state )
{
     /* Remember source parameters for usage in rendering functions. */
     vdev->src        = (VdpOutputSurface) (unsigned long) state->src.handle;
     vdev->src_pitch  = state->src.pitch;
     vdev->src_format = state->src.buffer->format;
     vdev->src_bpp    = DFB_BYTES_PER_PIXEL( vdev->src_format );

     /* Set the flag. */
     VDPAU_VALIDATE( SOURCE );
}

/*
 * Called by vdpauSetState() to ensure that the blend struct is properly set
 * for execution of drawing functions.
 */
static inline void
vdpau_validate_BLEND_DRAW( VDPAUDeviceData *vdev,
                           CardState       *state )
{
     if (state->drawingflags & DSDRAW_BLEND) {
          vdev->blend.blend_factor_source_alpha = blend_factors[state->src_blend];
          vdev->blend.blend_factor_source_color = blend_factors[state->src_blend];

          vdev->blend.blend_factor_destination_alpha = blend_factors[state->dst_blend];
          vdev->blend.blend_factor_destination_color = blend_factors[state->dst_blend];
     }
     else {
          vdev->blend.blend_factor_source_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
          vdev->blend.blend_factor_source_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;

          vdev->blend.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
          vdev->blend.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
     }

     vdev->blend.blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
     vdev->blend.blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

     vdev->blend.blend_constant.alpha = 1.0f;
     vdev->blend.blend_constant.red   = 1.0f;
     vdev->blend.blend_constant.green = 1.0f;
     vdev->blend.blend_constant.blue  = 1.0f;

     /* Set the flag. */
     VDPAU_VALIDATE( BLEND_DRAW );
     VDPAU_INVALIDATE( BLEND_BLIT );
}

/*
 * Called by vdpauSetState() to ensure that the blend struct is properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_BLEND_BLIT( VDPAUDeviceData *vdev,
                           CardState       *state )
{
     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          vdev->blend.blend_factor_source_alpha = blend_factors[state->src_blend];
          vdev->blend.blend_factor_source_color = blend_factors[state->src_blend];

          vdev->blend.blend_factor_destination_alpha = blend_factors[state->dst_blend];
          vdev->blend.blend_factor_destination_color = blend_factors[state->dst_blend];
     }
     else {
          vdev->blend.blend_factor_source_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
          vdev->blend.blend_factor_source_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;

          vdev->blend.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
          vdev->blend.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
     }

     vdev->blend.blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
     vdev->blend.blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

     vdev->blend.blend_constant.alpha = 1.0f;
     vdev->blend.blend_constant.red   = 1.0f;
     vdev->blend.blend_constant.green = 1.0f;
     vdev->blend.blend_constant.blue  = 1.0f;

     /* Set the flag. */
     VDPAU_VALIDATE( BLEND_BLIT );
     VDPAU_INVALIDATE( BLEND_DRAW );
}

/*
 * Called by vdpauSetState() to ensure that the color struct is properly set
 * for execution of drawing functions.
 */
static inline void
vdpau_validate_COLOR_DRAW( VDPAUDeviceData *vdev,
                           CardState       *state )
{
     vdev->color.alpha = state->color.a / 255.0f;
     vdev->color.red   = state->color.r / 255.0f;
     vdev->color.green = state->color.g / 255.0f;
     vdev->color.blue  = state->color.b / 255.0f;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          vdev->color.red   *= vdev->color.alpha;
          vdev->color.green *= vdev->color.alpha;
          vdev->color.blue  *= vdev->color.alpha;
     }

     /* Set the flag. */
     VDPAU_VALIDATE( COLOR_DRAW );
     VDPAU_INVALIDATE( COLOR_BLIT );
}

/*
 * Called by vdpauSetState() to ensure that the color struct is properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_COLOR_BLIT( VDPAUDeviceData *vdev,
                           CardState       *state )
{
     VdpColor color = { 1, 1, 1, 1 };

     vdev->color = color;

     color.alpha = state->color.a / 255.0f;
     color.red   = state->color.r / 255.0f;
     color.green = state->color.g / 255.0f;
     color.blue  = state->color.b / 255.0f;

     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
          vdev->color.alpha = color.alpha;

     if (state->blittingflags & DSBLIT_COLORIZE) {
          vdev->color.red   = color.red;
          vdev->color.green = color.green;
          vdev->color.blue  = color.blue;
     }

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          vdev->color.red   *= color.alpha;
          vdev->color.green *= color.alpha;
          vdev->color.blue  *= color.alpha;
     }

     /* Set the flag. */
     VDPAU_VALIDATE( COLOR_BLIT );
     VDPAU_INVALIDATE( COLOR_DRAW );
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
vdpauEngineSync( void *drv, void *dev )
{
     VDPAUDriverData *vdrv = (VDPAUDriverData*) drv;
     VDPAUDeviceData *vdev = (VDPAUDeviceData*) dev;
     DFBX11VDPAU     *vdp  = vdrv->vdp;

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     if (vdev->sync) {
          VdpStatus  status;
          VdpRect    rect;
          u32        pixel;
          void      *ptr   = &pixel;
          uint32_t   pitch = 4;

          rect.x0 = 0;
          rect.y0 = 0;
          rect.x1 = 1;
          rect.y1 = 1;

          /*
           * Pseudo GetBits call, no other way found
           */
          XLockDisplay( vdrv->display );
          status = vdp->OutputSurfaceGetBitsNative( vdev->dst, &rect, &ptr, &pitch );
          XUnlockDisplay( vdrv->display );
          if (status) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative() failed (status %d, '%s')!\n",
                        status, vdp->GetErrorString( status ) );
               return DFB_FAILURE;
          }

          vdev->sync = false;
     }

     return DFB_OK;
}

/*
 * Reset the graphics engine.
 */
void
vdpauEngineReset( void *drv, void *dev )
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
vdpauEmitCommands( void *drv, void *dev )
{
}

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
vdpauCheckState( void                *drv,
                 void                *dev,
                 CardState           *state,
                 DFBAccelerationMask  accel )
{
     D_DEBUG_AT( VDPAU_2D, "vdpauCheckState (state %p, accel 0x%08x) <- dest %p\n",
                 state, accel, state->destination );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(VDPAU_SUPPORTED_DRAWINGFUNCTIONS | VDPAU_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_ARGB:
               break;

          default:
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~VDPAU_SUPPORTED_DRAWINGFLAGS)
               return;
     }
     else {
          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB:
                    break;

               default:
                    return;
          }

          /* Return if unsupported blitting flags are set. */
          if (state->blittingflags & ~VDPAU_SUPPORTED_BLITTINGFLAGS)
               return;
     }

     /* Enable acceleration of the function. */
     state->accel |= accel;
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
vdpauSetState( void                *drv,
               void                *dev,
               GraphicsDeviceFuncs *funcs,
               CardState           *state,
               DFBAccelerationMask  accel )
{
     VDPAUDeviceData        *vdev     = (VDPAUDeviceData*) dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( VDPAU_2D, "vdpauSetState (state %p, accel 0x%08x) <- dest %p, modified 0x%08x\n",
                 state, accel, state->destination, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          VDPAU_INVALIDATE( ALL );
     }
     else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               VDPAU_INVALIDATE( DESTINATION );
          else if (modified & SMF_COLOR)
               VDPAU_INVALIDATE( COLOR_BLIT | COLOR_DRAW );

          if (modified & SMF_SOURCE)
               VDPAU_INVALIDATE( SOURCE );

          if (modified & (SMF_BLITTING_FLAGS | SMF_SRC_BLEND | SMF_DST_BLEND))
               VDPAU_INVALIDATE( BLEND_BLIT );

          if (modified & (SMF_DRAWING_FLAGS | SMF_SRC_BLEND | SMF_DST_BLEND))
               VDPAU_INVALIDATE( BLEND_DRAW );

          if (modified & SMF_BLITTING_FLAGS)
               VDPAU_INVALIDATE( COLOR_BLIT );

          if (modified & SMF_DRAWING_FLAGS)
               VDPAU_INVALIDATE( COLOR_DRAW );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination... */
     VDPAU_CHECK_VALIDATE( DESTINATION );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               /* ...require valid drawing color/blend. */
               VDPAU_CHECK_VALIDATE( BLEND_DRAW );
               VDPAU_CHECK_VALIDATE( COLOR_DRAW );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VDPAU_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               /* ...require valid source and blitting color/blend. */
               VDPAU_CHECK_VALIDATE( SOURCE );
               VDPAU_CHECK_VALIDATE( BLEND_BLIT );
               VDPAU_CHECK_VALIDATE( COLOR_BLIT );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VDPAU_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

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
bool
vdpauFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     VDPAUDriverData *vdrv = (VDPAUDriverData*) drv;
     VDPAUDeviceData *vdev = (VDPAUDeviceData*) dev;
     DFBX11VDPAU     *vdp  = vdrv->vdp;

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     VdpStatus status;
     VdpRect   dst_rect;
     VdpRect   src_rect;

     dst_rect.x0 = rect->x;
     dst_rect.y0 = rect->y;
     dst_rect.x1 = rect->x + rect->w;
     dst_rect.y1 = rect->y + rect->h;

     src_rect.x0 = 0;
     src_rect.y0 = 0;
     src_rect.x1 = 1;
     src_rect.y1 = 1;

     vdev->sync = true;

     XLockDisplay( vdrv->display );
     status = vdp->OutputSurfaceRenderOutputSurface( vdev->dst, &dst_rect, vdev->white, &src_rect,
                                                     &vdev->color, &vdev->blend, vdev->flags );
     XUnlockDisplay( vdrv->display );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceRenderOutputSurface() failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return false;
     }

     return true;
}

/*
 * Blit a surface using the current hardware state.
 */
bool
vdpauBlit( void *drv, void *dev, DFBRectangle *srect, int dx, int dy )
{
     VDPAUDriverData *vdrv = (VDPAUDriverData*) drv;
     VDPAUDeviceData *vdev = (VDPAUDeviceData*) dev;
     DFBX11VDPAU     *vdp  = vdrv->vdp;

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d -> %d, %d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( srect ), dx, dy );

     VdpStatus status;
     VdpRect   dst_rect;
     VdpRect   src_rect;

     dst_rect.x0 = dx;
     dst_rect.y0 = dy;
     dst_rect.x1 = dx + srect->w;
     dst_rect.y1 = dy + srect->h;

     src_rect.x0 = srect->x;
     src_rect.y0 = srect->y;
     src_rect.x1 = srect->x + srect->w;
     src_rect.y1 = srect->y + srect->h;

     vdev->sync = true;

     XLockDisplay( vdrv->display );
     status = vdp->OutputSurfaceRenderOutputSurface( vdev->dst, &dst_rect, vdev->src, &src_rect,
                                                     &vdev->color, &vdev->blend, vdev->flags );
     XUnlockDisplay( vdrv->display );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceRenderOutputSurface() failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return false;
     }

     return true;
}

/*
 * StretchBlit a surface using the current hardware state.
 */
bool
vdpauStretchBlit( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect )
{
     VDPAUDriverData *vdrv = (VDPAUDriverData*) drv;
     VDPAUDeviceData *vdev = (VDPAUDeviceData*) dev;
     DFBX11VDPAU     *vdp  = vdrv->vdp;

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d -> %d,%d-%dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( srect ), DFB_RECTANGLE_VALS( drect ) );

     VdpStatus status;
     VdpRect   dst_rect;
     VdpRect   src_rect;

     dst_rect.x0 = drect->x;
     dst_rect.y0 = drect->y;
     dst_rect.x1 = drect->x + drect->w;
     dst_rect.y1 = drect->y + drect->h;

     src_rect.x0 = srect->x;
     src_rect.y0 = srect->y;
     src_rect.x1 = srect->x + srect->w;
     src_rect.y1 = srect->y + srect->h;

     vdev->sync = true;

     XLockDisplay( vdrv->display );
     status = vdp->OutputSurfaceRenderOutputSurface( vdev->dst, &dst_rect, vdev->src, &src_rect,
                                                     &vdev->color, &vdev->blend, vdev->flags );
     XUnlockDisplay( vdrv->display );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceRenderOutputSurface() failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return false;
     }

     return true;
}

