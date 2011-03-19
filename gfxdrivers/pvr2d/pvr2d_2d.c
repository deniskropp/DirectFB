/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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
#include <core/system.h>

#include <gfx/convert.h>

#include "pvr2d_2d.h"
#include "pvr2d_gfxdriver.h"


D_DEBUG_DOMAIN(PVR2D__2D, "PVR2D/2D", "PVR2D Acceleration");

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DESTINATION  = 0x00000001,
     SCISSOR      = 0x00000002,
     MATRIX       = 0x00000004,
     RENDER_OPTS  = 0x00000008,

     COLOR_DRAW   = 0x00000010,
     COLORKEY     = 0x00000020,

     SOURCE       = 0x00000100,
     COLOR_BLIT   = 0x00000200,

     BLENDFUNC    = 0x00010000,

     ALL          = 0x0001033F
};

/*
 * State handling macros.  Shader uniform variables are shader program state
 * and some of them are duplicated across programs.  
 */
#define PVR2D_VALIDATE(flags)        \
     do {                            \
          gdrv->v_flags |=  (flags); \
     } while (0)

#define PVR2D_INVALIDATE(flags)      \
     do {                            \
          gdrv->v_flags &= ~(flags); \
    } while (0)

#define PVR2D_CHECK_VALIDATE(flag)                          \
     do {                                                   \
          if ((gdrv->v_flags & flag) != flag)               \
               pvr2d_validate_##flag(gdrv, gdev, state);    \
     } while (0)


/*****************************************************************************/

/*
 * Called by pvr2dSetState() to ensure that the rendering options are properly
 * set for execution of rendering functions.
 */
static inline void
pvr2d_validate_RENDER_OPTS(PVR2DDriverData *gdrv,
                           PVR2DDeviceData *gdev,
                           CardState       *state)
{
     D_DEBUG_AT(PVR2D__2D, "%s() UNIMPLEMENTED!\n", __FUNCTION__);

     // Set the flag.
     PVR2D_VALIDATE(RENDER_OPTS);
}

/*
 * Called by pvr2dSetState() to ensure that the clip is properly set
 * for execution of rendering functions.  This is part of PVR2D fixed
 * functionality.
 */
static inline void
pvr2d_validate_SCISSOR(PVR2DDriverData *gdrv,
                       PVR2DDeviceData *gdev,
                       CardState       *state)
{
//     CoreSurface *surface = state->destination;

     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);


     // Set the flag.
     PVR2D_VALIDATE(SCISSOR);
}

/*
 * Called by pvr2dSetState() to ensure that the destination parameters are
 * properly set for execution of rendering functions.  gdev->prog_index
 * must be valid.
 */
static inline void
pvr2d_validate_DESTINATION(PVR2DDriverData *gdrv,
                           PVR2DDeviceData *gdev,
                           CardState       *state)
{
//     CoreSurface *surface = state->destination;

//     D_DEBUG_AT( PVR2D__2D, "%s( color_rb %u )\n", __FUNCTION__, color_rb );

     gdrv->bltinfo.pDstMemInfo   = state->dst.handle;
     gdrv->bltinfo.DstOffset     = 0;
     gdrv->bltinfo.DstStride     = state->dst.pitch;
     gdrv->bltinfo.DstFormat     = PVR2D_RGB565;
     gdrv->bltinfo.DstSurfWidth  = state->destination->config.size.w;
     gdrv->bltinfo.DstSurfHeight = state->destination->config.size.h;


     // Set the flag.
     PVR2D_VALIDATE(DESTINATION);
}

/*
 * Called by pvr2dSetState() to ensure that the matrix is properly set
 * for execution of rendering functions.  gdev->prog_index must be valid.
 */
static inline void
pvr2d_validate_MATRIX(PVR2DDriverData *gdrv,
                      PVR2DDeviceData *gdev,
                      CardState       *state)
{
//     CoreSurface *surface = state->destination;

     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);


     // Set the flag.
     PVR2D_VALIDATE(MATRIX);
}

/*
 * Called by pvr2dSetState() to ensure that the color is properly set
 * for execution of drawing functions.  gdev->prog_index must be valid.
 */
static inline void
pvr2d_validate_COLOR_DRAW(PVR2DDriverData *gdrv,
                          PVR2DDeviceData *gdev,
                          CardState       *state)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);


     // Set the flag.
     PVR2D_VALIDATE(COLOR_DRAW);
}

/*
 * Called by pvr2dSetState() to ensure that the source parameters are properly
 * set for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
pvr2d_validate_SOURCE(PVR2DDriverData *gdrv,
                      PVR2DDeviceData *gdev,
                      CardState       *state)
{
//     CoreSurface      *surface = state->source;

//     D_DEBUG_AT(PVR2D__2D, "%s( texture %u )\n", __FUNCTION__, texture);

     gdrv->bltinfo.pSrcMemInfo   = state->src.handle;
     gdrv->bltinfo.SrcOffset     = 0;
     gdrv->bltinfo.SrcStride     = state->src.pitch;
     gdrv->bltinfo.SrcFormat     = PVR2D_RGB565;
     gdrv->bltinfo.SrcSurfWidth  = state->source->config.size.w;
     gdrv->bltinfo.SrcSurfHeight = state->source->config.size.h;


     // Set the flag.
     PVR2D_VALIDATE(SOURCE);
}

/*
 * Called by pvr2dSetState() to ensure that the color is properly set
 * for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
pvr2d_validate_COLOR_BLIT(PVR2DDriverData *gdrv,
                          PVR2DDeviceData *gdev,
                          CardState       *state)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);


     // Set the flag.
     PVR2D_VALIDATE(COLOR_BLIT);
}

/*
 * Called by pvr2dSetState() to ensure that the colorkey is properly set
 * for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
pvr2d_validate_COLORKEY(PVR2DDriverData *gdrv,
                        PVR2DDeviceData *gdev,
                        CardState       *state)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     // Set the flag.
     PVR2D_VALIDATE(COLORKEY);
}

/*
 * Called by pvr2dSetState() to ensure that the blend functions are properly
 * set for execution of drawing and blitting functions.  This is part of PVR2D
 * fixed functionality.
 */
static inline void
pvr2d_validate_BLENDFUNC(PVR2DDriverData *gdrv,
                         PVR2DDeviceData *gdev,
                         CardState       *state)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);


     // Set the flag.
     PVR2D_VALIDATE(BLENDFUNC);
}

/******************************************************************************/

/*
 * Wait for the blitter to be idle.
 *
 * This function is called before memory that has been written to by the
 * hardware is about to be accessed by the CPU (software driver) or another
 * hardware entity like video encoder (by Flip()).  It can also be called by
 * applications explicitly, e.g. at the end of a benchmark loop to include
 * execution time of queued commands in the measurement.
 */
DFBResult
pvr2dEngineSync(void *drv, void *dev)
{
     PVR2DDriverData *gdrv  = drv;
     PVR2DData       *pvr2d = gdrv->pvr2d;

     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     if (gdrv->bltinfo.pDstMemInfo) {
          PVR2DERROR ePVR2DStatus;

          ePVR2DStatus = PVR2DQueryBlitsComplete( pvr2d->hPVR2DContext, gdrv->bltinfo.pDstMemInfo, PVR2D_TRUE );
          if (ePVR2DStatus) {
               D_ERROR( "DirectFB/PVR2D: PVR2DQueryBlitsComplete() failed! (status %d)\n", ePVR2DStatus );
               return false;
          }
     }

     return DFB_OK;
}

/*
 * Reset the graphics engine.
 */
void
pvr2dEngineReset(void *drv, void *dev)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);
}

/*
 * Start processing of queued commands if required.
 *
 * This function is called before returning from the graphics core to the
 * application.  Usually that's after each rendering function. The only
 * functions causing multiple commands to be queued with a single emission at
 * the end are DrawString(), TileBlit(), BatchBlit(), DrawLines() and possibly
 * FillTriangle() which is emulated using multiple FillRectangle() calls.
 */
void
pvr2dEmitCommands(void *drv, void *dev)
{
//     PVR2DDriverData *gdrv = drv;

     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

}

/******************************************************************************/

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
pvr2dCheckState(void                *drv,
                void                *dev,
                CardState           *state,
                DFBAccelerationMask  accel)
{
     D_DEBUG_AT(PVR2D__2D, "%s(state %p, accel 0x%08x) <- dest %p [%lu]\n",
                __FUNCTION__, state, accel, state->destination,
                state->dst.offset);

     // Return if the desired function is not supported at all.
     if (accel & ~(PVR2D_SUPPORTED_DRAWINGFUNCTIONS |
                   PVR2D_SUPPORTED_BLITTINGFUNCTIONS)) {
          D_DEBUG_AT(PVR2D__2D, "  -> unsupported function\n");
          return;
     }

     // Return if the destination format is not supported.
     switch (state->destination->config.format) {
          //case DSPF_ARGB:
          //case DSPF_RGB32:
          case DSPF_RGB16:
               break;
          default:
               D_DEBUG_AT
               (PVR2D__2D, "  -> unsupported destination format %s\n",
                dfb_pixelformat_name(state->destination->config.format));
               return;
     }

     // Check if drawing or blitting is requested.
     if (DFB_DRAWING_FUNCTION(accel)) {
          // Return if unsupported drawing flags are set.
          if (state->drawingflags & ~PVR2D_SUPPORTED_DRAWINGFLAGS) {
               D_DEBUG_AT(PVR2D__2D, "  -> unsupported drawing flags 0x%08x\n",
                          state->drawingflags);
               return;
          }
     }
     else {
          // Return if the source format is not supported.
          switch (state->source->config.format) {
               //case DSPF_ARGB:
               //case DSPF_RGB32:
               case DSPF_RGB16:
                    break;
               default:
                    D_DEBUG_AT
                    (PVR2D__2D, "  -> unsupported source format %s\n",
                     dfb_pixelformat_name(state->source->config.format));
                    return;
          }

          // Return if unsupported blitting flags are set.
          if (state->blittingflags & ~PVR2D_SUPPORTED_BLITTINGFLAGS) {
               D_DEBUG_AT(PVR2D__2D, "  -> unsupported blit flags 0x%08x\n",
                          state->blittingflags);
               return;
          }
     }

     // Enable acceleration of the function.
     state->accel |= accel;

     D_DEBUG_AT(PVR2D__2D, "  => OK\n");
}

/*
 * Make sure that the hardware is programmed for execution of 'accel'
 * according to the 'state'.
 */
void
pvr2dSetState(void                *drv,
              void                *dev,
              GraphicsDeviceFuncs *funcs,
              CardState           *state,
              DFBAccelerationMask  accel)
{
     PVR2DDriverData       *gdrv     = drv;
     PVR2DDeviceData       *gdev     = dev;
     StateModificationFlags modified = state->mod_hw;
     DFBBoolean             blend    = DFB_FALSE;

     D_DEBUG_AT(PVR2D__2D,
                "%s(state %p, accel 0x%08x) <- dest %p, modified 0x%08x\n",
                __FUNCTION__, state, accel, state->destination, modified);

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more
      * hardware states.
      */
     if (modified == SMF_ALL) {
          PVR2D_INVALIDATE(ALL);
     }
     else if (modified) {
          if (modified & SMF_DESTINATION)
               PVR2D_INVALIDATE(DESTINATION);

          if (modified & SMF_CLIP)
               PVR2D_INVALIDATE(SCISSOR);

          if (modified & SMF_MATRIX)
               PVR2D_INVALIDATE(MATRIX);

          if (modified & SMF_RENDER_OPTIONS)
               PVR2D_INVALIDATE(MATRIX | RENDER_OPTS);

          if (modified & SMF_COLOR)
               PVR2D_INVALIDATE(COLOR_DRAW | COLOR_BLIT);

          if (modified & SMF_DRAWING_FLAGS)
               PVR2D_INVALIDATE(COLOR_DRAW);

          if (modified & SMF_BLITTING_FLAGS)
               PVR2D_INVALIDATE(COLOR_BLIT);

          if (modified & SMF_SOURCE)
               PVR2D_INVALIDATE(SOURCE);

          if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
               PVR2D_INVALIDATE(BLENDFUNC);

          if (modified & SMF_SRC_COLORKEY)
               PVR2D_INVALIDATE(COLORKEY);
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */
     PVR2D_CHECK_VALIDATE(SCISSOR);
     // PVR2D_CHECK_VALIDATE(RENDER_OPTS);

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               // If alpha blending is used...
               if (state->drawingflags & DSDRAW_BLEND) {
                    // ...require valid blend functions.
                    PVR2D_CHECK_VALIDATE(BLENDFUNC);
               }
               else {
               }

               // Now that we have a valid program, check destination state.
               PVR2D_CHECK_VALIDATE(DESTINATION);

               // Check if the DSRO_MATRIX needs to be loaded.
               PVR2D_CHECK_VALIDATE(MATRIX);

               // Check for valid drawing color.
               PVR2D_CHECK_VALIDATE(COLOR_DRAW);

               /*
                * 3) Tell which functions can be called without further
                * validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is
                * reset.
                */
               state->set = PVR2D_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               // If alpha blending is used...
               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                           DSBLIT_BLEND_COLORALPHA)) {
                    // ...require valid blend functions.
                    PVR2D_CHECK_VALIDATE(BLENDFUNC);
                    blend = DFB_TRUE;
               }
               else {
                    blend = DFB_FALSE;
               }

               // We should have a valid program; check destination and source.
               PVR2D_CHECK_VALIDATE(DESTINATION);
               PVR2D_CHECK_VALIDATE(SOURCE);

               // Check if DSRO_MATRIX needs to be loaded.
               PVR2D_CHECK_VALIDATE(MATRIX);

               if (!blend) {
                    // Check if colorkey needs to be loaded.
                    PVR2D_CHECK_VALIDATE(COLORKEY);
               }

               /*
             * To reduce the number of shader programs, the blit fragment
             * shader always modulates by a color.  Validate that color.
             */
               PVR2D_CHECK_VALIDATE(COLOR_BLIT);

               /*
          * 3) Tell which functions can be called without further
          * validation, i.e. SetState().
                *
                * When the hw independent state is changed, this collection is
                * reset.
                */
               state->set = PVR2D_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG("unexpected drawing/blitting function");
               break;
     }

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further
      * validation.  If the hw independent state is not modified, this
      * function won't get called for subsequent rendering functions, unless
      * they aren't defined by 3).
      */
     state->mod_hw = 0;
}

/******************************************************************************/

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
pvr2dFillRectangle(void *drv, void *dev, DFBRectangle *rect)
{
/*
     PVR2DDriverData *gdrv  = drv;
     PVR2DData       *pvr2d = gdrv->pvr2d;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(rect));

     PVR2DERROR ePVR2DStatus;

     gdrv->bltinfo.DstX          = 0;
     gdrv->bltinfo.DstY          = 0;
     gdrv->bltinfo.DstSizeX      = ;
     ePVR2DStatus = PVR2DBlt( pvr2d->hPVR2DContext, &gdrv->bltinfo );
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DBlt() failed! (status %d)\n", ePVR2DStatus );
          return false;
     }
*/
     return true;
}

/*
 * Render a rectangle outline using the current hardware state.
 */
bool
pvr2dDrawRectangle(void *drv, void *dev, DFBRectangle *rect)
{
//     PVR2DDriverData *gdrv = drv;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(rect));

     return true;
}

/*
 * Render a line using the current hardware state.
 */
bool
pvr2dDrawLine(void *drv, void *dev, DFBRegion *line)
{
//     PVR2DDriverData *gdrv = drv;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4d,%4d)\n",
                __FUNCTION__, DFB_REGION_VALS(line));

     return true;
}

/*
 * Render a triangle using the current hardware state.
 */
bool
pvr2dFillTriangle(void *drv, void *dev, DFBTriangle *tri)
{
  //   PVR2DDriverData *gdrv = drv;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4d,%4d-%4d,%4d)\n", __FUNCTION__,
                tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3);

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
pvr2dBlit(void *drv, void *dev, DFBRectangle *srect, int dx, int dy)
{
     PVR2DDriverData *gdrv  = drv;
     PVR2DData       *pvr2d = gdrv->pvr2d;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4dx%4d <- %4d,%4d)\n",
                __FUNCTION__, dx, dy, srect->w, srect->h, srect->x, srect->y);


     PVR2DERROR ePVR2DStatus;

     gdrv->bltinfo.DstX   = dx;
     gdrv->bltinfo.DstY   = dy;
     gdrv->bltinfo.DSizeX = srect->w;
     gdrv->bltinfo.DSizeY = srect->h;

     gdrv->bltinfo.SrcX   = srect->x;
     gdrv->bltinfo.SrcY   = srect->y;
     gdrv->bltinfo.SizeX  = srect->w;
     gdrv->bltinfo.SizeY  = srect->h;

     gdrv->bltinfo.CopyCode = 0xcc;

     ePVR2DStatus = PVR2DBlt( pvr2d->hPVR2DContext, &gdrv->bltinfo );
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DBlt() failed! (status %d)\n", ePVR2DStatus );
          return false;
     }

     return true;
}

/*
 * Blit a scaled rectangle using the current hardware state.
 */
bool
pvr2dStretchBlit(void *drv, void *dev,
                 DFBRectangle *srect, DFBRectangle *drect)
{
     //PVR2DDriverData *gdrv = drv;

     D_DEBUG_AT(PVR2D__2D, "%s(%4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(drect),
                DFB_RECTANGLE_VALS(srect));


     return true;
}

