/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#define VDPAU_VALIDATE(flags)        do { vdrv->v_flags |=  (flags); } while (0)
#define VDPAU_INVALIDATE(flags)      do { vdrv->v_flags &= ~(flags); } while (0)

#define VDPAU_CHECK_VALIDATE(flag)   do {                                             \
                                          if (! (vdrv->v_flags & flag))               \
                                               vdpau_validate_##flag( vdrv, state );  \
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
vdpau_validate_DESTINATION( VDPAUDriverData *vdrv,
                            CardState       *state )
{
     /* Remember destination parameters for usage in rendering functions. */
     vdrv->render_draw.destination_surface = (VdpOutputSurface) (unsigned long) state->dst.handle;
     vdrv->render_blit.destination_surface = (VdpOutputSurface) (unsigned long) state->dst.handle;

     /* Set the flag. */
     VDPAU_VALIDATE( DESTINATION );
}

/*
 * Called by vdpauSetState() to ensure that the source registers are properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_SOURCE( VDPAUDriverData *vdrv,
                       CardState       *state )
{
     /* Remember source parameters for usage in rendering functions. */
     vdrv->render_blit.source_surface = (VdpOutputSurface) (unsigned long) state->src.handle;

     /* Set the flag. */
     VDPAU_VALIDATE( SOURCE );
}

/*
 * Called by vdpauSetState() to ensure that the blend struct is properly set
 * for execution of drawing functions.
 */
static inline void
vdpau_validate_BLEND_DRAW( VDPAUDriverData *vdrv,
                           CardState       *state )
{
     if (state->drawingflags & DSDRAW_BLEND) {
          vdrv->render_draw.blend_state.blend_factor_source_alpha = blend_factors[state->src_blend];
          vdrv->render_draw.blend_state.blend_factor_source_color = blend_factors[state->src_blend];

          vdrv->render_draw.blend_state.blend_factor_destination_alpha = blend_factors[state->dst_blend];
          vdrv->render_draw.blend_state.blend_factor_destination_color = blend_factors[state->dst_blend];
     }
     else {
          vdrv->render_draw.blend_state.blend_factor_source_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
          vdrv->render_draw.blend_state.blend_factor_source_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;

          vdrv->render_draw.blend_state.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
          vdrv->render_draw.blend_state.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
     }

     vdrv->render_draw.blend_state.blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
     vdrv->render_draw.blend_state.blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

     vdrv->render_draw.blend_state.blend_constant.alpha = 1.0f;
     vdrv->render_draw.blend_state.blend_constant.red   = 1.0f;
     vdrv->render_draw.blend_state.blend_constant.green = 1.0f;
     vdrv->render_draw.blend_state.blend_constant.blue  = 1.0f;

     /* Set the flag. */
     VDPAU_VALIDATE( BLEND_DRAW );
}

/*
 * Called by vdpauSetState() to ensure that the blend struct is properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_BLEND_BLIT( VDPAUDriverData *vdrv,
                           CardState       *state )
{
     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          vdrv->render_blit.blend_state.blend_factor_source_alpha = blend_factors[state->src_blend];
          vdrv->render_blit.blend_state.blend_factor_source_color = blend_factors[state->src_blend];

          vdrv->render_blit.blend_state.blend_factor_destination_alpha = blend_factors[state->dst_blend];
          vdrv->render_blit.blend_state.blend_factor_destination_color = blend_factors[state->dst_blend];
     }
     else {
          vdrv->render_blit.blend_state.blend_factor_source_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
          vdrv->render_blit.blend_state.blend_factor_source_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;

          vdrv->render_blit.blend_state.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
          vdrv->render_blit.blend_state.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO;
     }

     vdrv->render_blit.blend_state.blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
     vdrv->render_blit.blend_state.blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

     vdrv->render_blit.blend_state.blend_constant.alpha = 1.0f;
     vdrv->render_blit.blend_state.blend_constant.red   = 1.0f;
     vdrv->render_blit.blend_state.blend_constant.green = 1.0f;
     vdrv->render_blit.blend_state.blend_constant.blue  = 1.0f;

     /* Set the flag. */
     VDPAU_VALIDATE( BLEND_BLIT );
}

/*
 * Called by vdpauSetState() to ensure that the color struct is properly set
 * for execution of drawing functions.
 */
static inline void
vdpau_validate_COLOR_DRAW( VDPAUDriverData *vdrv,
                           CardState       *state )
{
     vdrv->render_draw.color.alpha = state->color.a / 255.0f;
     vdrv->render_draw.color.red   = state->color.r / 255.0f;
     vdrv->render_draw.color.green = state->color.g / 255.0f;
     vdrv->render_draw.color.blue  = state->color.b / 255.0f;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          vdrv->render_draw.color.red   *= vdrv->render_draw.color.alpha;
          vdrv->render_draw.color.green *= vdrv->render_draw.color.alpha;
          vdrv->render_draw.color.blue  *= vdrv->render_draw.color.alpha;
     }

     /* Set the flag. */
     VDPAU_VALIDATE( COLOR_DRAW );
}

/*
 * Called by vdpauSetState() to ensure that the color struct is properly set
 * for execution of blitting functions.
 */
static inline void
vdpau_validate_COLOR_BLIT( VDPAUDriverData *vdrv,
                           CardState       *state )
{
     VdpColor color = { 1, 1, 1, 1 };

     vdrv->render_blit.color = color;

     color.alpha = state->color.a / 255.0f;
     color.red   = state->color.r / 255.0f;
     color.green = state->color.g / 255.0f;
     color.blue  = state->color.b / 255.0f;

     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
          vdrv->render_blit.color.alpha = color.alpha;

     if (state->blittingflags & DSBLIT_COLORIZE) {
          vdrv->render_blit.color.red   = color.red;
          vdrv->render_blit.color.green = color.green;
          vdrv->render_blit.color.blue  = color.blue;
     }

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          vdrv->render_blit.color.red   *= color.alpha;
          vdrv->render_blit.color.green *= color.alpha;
          vdrv->render_blit.color.blue  *= color.alpha;
     }

     /* Set the flag. */
     VDPAU_VALIDATE( COLOR_BLIT );
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

     D_DEBUG_AT( VDPAU_2D, "%s()\n", __FUNCTION__ );

     if (vdev->sync) {
#if 1
          DirectResult                         ret;
          int                                  retval;
          DFBX11CallOutputSurfaceGetBitsNative get;

          get.surface        = vdev->white;// vdrv->render_draw.destination_surface;
          get.ptr            = &vdev->pixel;
          get.pitch          = 4;

          get.source_rect.x0 = 0;
          get.source_rect.y0 = 0;
          get.source_rect.x1 = 1;
          get.source_rect.y1 = 1;


          ret = fusion_call_execute2( &vdrv->x11->shared->call, FCEF_NONE, X11_VDPAU_OUTPUT_SURFACE_GET_BITS_NATIVE, &get, sizeof(get), &retval );
          if (ret) {
               D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
               return ret;
          }

          if (retval) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative( %u ) failed (status %d, '%s'!\n",
                        get.surface, retval, vdp->GetErrorString( retval ) );
               return DFB_FAILURE;
          }
#else

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
          status = vdp->OutputSurfaceGetBitsNative( vdrv->render_draw.destination_surface, &rect, &ptr, &pitch );
          XUnlockDisplay( vdrv->display );
          if (status) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative() failed (status %d, '%s')!\n",
                        status, vdp->GetErrorString( status ) );
               return DFB_FAILURE;
          }
#endif

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
     VDPAUDriverData        *vdrv     = (VDPAUDriverData*) drv;
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
               state->set |= VDPAU_SUPPORTED_DRAWINGFUNCTIONS;
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
               state->set |= VDPAU_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     vdrv->render_draw.source_surface = vdev->white;

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

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     vdrv->render_draw.destination_rect.x0 = rect->x;
     vdrv->render_draw.destination_rect.y0 = rect->y;
     vdrv->render_draw.destination_rect.x1 = rect->x + rect->w;
     vdrv->render_draw.destination_rect.y1 = rect->y + rect->h;

     vdrv->render_draw.source_rect.x0 = 0;
     vdrv->render_draw.source_rect.y0 = 0;
     vdrv->render_draw.source_rect.x1 = 1;
     vdrv->render_draw.source_rect.y1 = 1;

     vdev->sync = true;

     fusion_call_execute2( &vdrv->x11->shared->call, FCEF_ONEWAY, X11_VDPAU_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
                           &vdrv->render_draw, sizeof(DFBX11CallOutputSurfaceRenderOutputSurface), NULL );

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

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d -> %d, %d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( srect ), dx, dy );

     vdrv->render_blit.destination_rect.x0 = dx;
     vdrv->render_blit.destination_rect.y0 = dy;
     vdrv->render_blit.destination_rect.x1 = dx + srect->w;
     vdrv->render_blit.destination_rect.y1 = dy + srect->h;

     vdrv->render_blit.source_rect.x0 = srect->x;
     vdrv->render_blit.source_rect.y0 = srect->y;
     vdrv->render_blit.source_rect.x1 = srect->x + srect->w;
     vdrv->render_blit.source_rect.y1 = srect->y + srect->h;

     vdev->sync = true;

     fusion_call_execute2( &vdrv->x11->shared->call, FCEF_ONEWAY, X11_VDPAU_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
                           &vdrv->render_blit, sizeof(DFBX11CallOutputSurfaceRenderOutputSurface), NULL );

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

     D_DEBUG_AT( VDPAU_2D, "%s( %d,%d-%dx%d -> %d,%d-%dx%d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( srect ), DFB_RECTANGLE_VALS( drect ) );

     vdrv->render_blit.destination_rect.x0 = drect->x;
     vdrv->render_blit.destination_rect.y0 = drect->y;
     vdrv->render_blit.destination_rect.x1 = drect->x + drect->w;
     vdrv->render_blit.destination_rect.y1 = drect->y + drect->h;

     vdrv->render_blit.source_rect.x0 = srect->x;
     vdrv->render_blit.source_rect.y0 = srect->y;
     vdrv->render_blit.source_rect.x1 = srect->x + srect->w;
     vdrv->render_blit.source_rect.y1 = srect->y + srect->h;

     vdev->sync = true;

     fusion_call_execute2( &vdrv->x11->shared->call, FCEF_ONEWAY, X11_VDPAU_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
                           &vdrv->render_blit, sizeof(DFBX11CallOutputSurfaceRenderOutputSurface), NULL );

     return true;
}

