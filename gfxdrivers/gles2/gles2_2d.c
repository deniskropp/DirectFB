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

#include "gles2_2d.h"
#include "gles2_gfxdriver.h"


D_DEBUG_DOMAIN(GLES2__2D, "GLES2/2D", "OpenGL ES2 2D Acceleration");

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
#define GLES2_VALIDATE(flags)        \
     do {			     \
	  gdev->progs[gdev->prog_index].v_flags |=  (flags); \
     } while (0)

#define GLES2_INVALIDATE(flags)      \
     do {                            \
	  int i;		     \
	  for (i = 0; i < GLES2_NUM_PROGRAMS; i++) {	\
	       gdev->progs[i].v_flags &= ~(flags);	\
	  }						\
     } while (0)

#define GLES2_CHECK_VALIDATE(flag)   \
     do {                            \
	  if ((gdev->progs[gdev->prog_index].v_flags & flag) != flag)	\
	       gles2_validate_##flag(gdrv, gdev, state);		\
     } while (0)


/*****************************************************************************/

/*
 * Called by gles2SetState() to ensure that the rendering options are properly
 * set for execution of rendering functions.
 *
 * XXX hood - TODO 
 * Line anti-aliasing is not directly supported in GLES2.  A shader for it
 * needs to be written.  Could use multisample buffers as well to cover
 * all point, line, and triangle primitives.
 */
static inline void
gles2_validate_RENDER_OPTS(GLES2DriverData *gdrv,
                           GLES2DeviceData *gdev,
                           CardState       *state)
{
     D_DEBUG_AT(GLES2__2D, "%s() UNIMPLEMENTED!\n", __FUNCTION__);
#if 0
     if (state->render_options & DSRO_ANTIALIAS) {
          glEnable(GL_LINE_SMOOTH);
          //glEnable(GL_POLYGON_SMOOTH);
     }
     else {
          glDisable(GL_LINE_SMOOTH);
          //glDisable(GL_POLYGON_SMOOTH);
     }
#endif
     // Set the flag.
     GLES2_VALIDATE(RENDER_OPTS);
}

/*
 * Called by gles2SetState() to ensure that the clip is properly set
 * for execution of rendering functions.  This is part of GLES2 fixed
 * functionality.
 */
static inline void
gles2_validate_SCISSOR(GLES2DriverData *gdrv,
                       GLES2DeviceData *gdev,
                       CardState       *state)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     glEnable(GL_SCISSOR_TEST);
     glScissor(state->clip.x1,
               state->clip.y1,
               state->clip.x2 - state->clip.x1 + 1,
               state->clip.y2 - state->clip.y1 + 1);

     // Set the flag.
     GLES2_VALIDATE(SCISSOR);
}

/*
 * Called by gles2SetState() to ensure that the destination parameters are
 * properly set for execution of rendering functions.  gdev->prog_index
 * must be valid.
 */
static inline void
gles2_validate_DESTINATION(GLES2DriverData *gdrv,
                           GLES2DeviceData *gdev,
                           CardState       *state)
{
     CoreSurface *surface  = state->destination;
     GLuint       color_rb = (GLuint)(long)state->dst.handle;


     D_DEBUG_AT( GLES2__2D, "%s( color_rb %u )\n", __FUNCTION__, color_rb );
//     D_MAGIC_ASSERT(buffer, GLES2BufferData);
#ifdef GLES2_USE_FBO
     glFramebufferRenderbuffer( GL_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER,
                                color_rb );

     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
          D_ERROR( "DirectFB/GLES2: Framebuffer not complete\n" );
     }
#endif

     if (1/*(buffer->flags & GLES2BF_UPDATE_TARGET)*/ ||
         (gdev->prog_index != gdev->prog_last)) {

          int w = surface->config.size.w;
          int h = surface->config.size.h;
          GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

          D_DEBUG_AT(GLES2__2D, "  -> got new prog or UPDATE_TARGET\n");
          D_DEBUG_AT(GLES2__2D, "  -> width %d height %d\n", w, h);

          glViewport(0, 0, w, h);
          glDisable(GL_CULL_FACE);

          if (state->render_options & DSRO_MATRIX) {
               /*
             * We need a 3x3 matrix multiplication in the vertex shader to
             * support the non-affine elements of the DSRO matrix.  Load
             * the ortho 2D model-view-projection matrix component here.
             */
               float m[9];

               m[0] = 2.0f/w; m[3] =     0.0; m[6] = -1.0f;
               m[1] =    0.0; m[4] = -2.0f/h; m[7] =  1.0f;
               m[2] =    0.0; m[5] =     0.0; m[8] =  1.0f;

               // Load it in the proper location for this program object.
               glUniformMatrix3fv(prog->dfbMVPMatrix, 1, GL_FALSE, m);

               D_DEBUG_AT(GLES2__2D,
                          "  -> loaded matrix %f %f %f %f %f %f %f %f %f\n",
                          m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
          }
          else {
               // Just need the X & Y scale factors and constant offsets.
#ifdef GLES2_PVR2D
               glUniform2f(prog->dfbScale, 2.0f/w, -2.0f/h);
               glUniform2f(prog->dfbScale, 2.0f/w, -2.0f/h);
#else
               glUniform2f(prog->dfbScale, 2.0f/w, 2.0f/h);
               glUniform2f(prog->dfbScale, 2.0f/w, 2.0f/h);
#endif

               D_DEBUG_AT(GLES2__2D, "  -> loaded scale factors %f %f\n",
                          2.0f/w, 2.0f/h);
          }

          GLES2_INVALIDATE(ALL);
//          buffer->flags &= ~GLES2BF_UPDATE_TARGET;
     }

     // Set the flag.
     GLES2_VALIDATE(DESTINATION);
}

/*
 * Called by gles2SetState() to ensure that the matrix is properly set
 * for execution of rendering functions.  gdev->prog_index must be valid.
 */
static inline void
gles2_validate_MATRIX(GLES2DriverData *gdrv,
                      GLES2DeviceData *gdev,
                      CardState       *state)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     if (state->render_options & DSRO_MATRIX) {
          /*
           * We have to load a new surface render options matrix.  The matrix
           * is 3x3 with row major ordering, and it can encode non-affine 2D
           * transforms.  The elements are s32, 16.16 fixed point format.
           */ 
          float m[9];
          GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

#define M(n) (state->matrix[n] / 65536.0f)
          m[0] = M(0); m[3] = M(1); m[6] = M(2);
          m[1] = M(3); m[4] = M(4); m[7] = M(5);
          m[2] = M(6); m[5] = M(7); m[8] = M(8);
#undef M
          // Load it in the proper location for this program object.
          glUniformMatrix3fv(prog->dfbROMatrix, 1, GL_FALSE, m);

          D_DEBUG_AT(GLES2__2D,
                     "  -> loaded DSRO_MATRIX %f %f %f %f %f %f %f %f %f\n",
                     m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
     }

     // Set the flag.
     GLES2_VALIDATE(MATRIX);
}

/*
 * Called by gles2SetState() to ensure that the color is properly set
 * for execution of drawing functions.  gdev->prog_index must be valid.
 */
static inline void
gles2_validate_COLOR_DRAW(GLES2DriverData *gdrv,
                          GLES2DeviceData *gdev,
                          CardState       *state)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);
     GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

     /*
      * DFB's global RGBA color has fixed 8-bit components (u8).  The dfbColor
      * uniform shader variable is a lowp vec4 and expects components in the
      * floating point range [0.0 - 1.0].
      */
     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          /*
           * Pre-multiply source color by alpha:
           * c/255.0 * a/255.0 = c * a/65025f;
           */
          float a = state->color.a/65025.0f;

          glUniform4f(prog->dfbColor, state->color.r * a, state->color.g * a,
                      state->color.b * a, state->color.a/255.0f);

          D_DEBUG_AT(GLES2__2D, "  -> PREMULTIPLY, loaded color %f %f %f %f\n",
                     state->color.r * a, state->color.g * a,
                     state->color.b * a, state->color.a/255.0f);
     }
     else {
          float s = 1.0f/255.0f;

          glUniform4f(prog->dfbColor, state->color.r * s, state->color.g * s,
                      state->color.b * s, state->color.a * s);

          D_DEBUG_AT(GLES2__2D, "  -> loaded color %f %f %f %f\n",
                     state->color.r * s, state->color.g * s,
                     state->color.b * s, state->color.a * s);
     }

     // Set the flag.
     GLES2_VALIDATE(COLOR_DRAW);
}

/*
 * Called by gles2SetState() to ensure that the source parameters are properly
 * set for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
gles2_validate_SOURCE(GLES2DriverData *gdrv,
                      GLES2DeviceData *gdev,
                      CardState       *state)
{
     CoreSurface      *surface = state->source;
//   GLES2BufferData  *buffer = state->src.handle;
     GLuint            texture = (GLuint)(long)state->src.handle;
     GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

     D_DEBUG_AT(GLES2__2D, "%s( texture %u )\n", __FUNCTION__, texture);
//   D_MAGIC_ASSERT(buffer, GLES2BufferData);

     glBindTexture( GL_TEXTURE_2D, texture );

     if (1/*(buffer->flags & GLES2BF_UPDATE_TEXTURE)*/ ||
         (gdev->prog_index != gdev->prog_last)) {

          int w = surface->config.size.w;
          int h = surface->config.size.h;

          D_DEBUG_AT(GLES2__2D, "  -> got new prog or texture %d\n", texture);
          D_DEBUG_AT(GLES2__2D, "  -> old prog \"%s\" new prog \"%s\"\n",
                     gdev->progs[gdev->prog_last].name,
                     gdev->progs[gdev->prog_index].name);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(prog->dfbSampler, 0);

          /*
           * The equivalent of ARB_texture_rectangle isn't supported by GLES2,
           * so blit source coordinates have to be normalized to tex coords in
           * the range [0..1].
           */
          glUniform2f(prog->dfbTexScale, 1.0f/w, 1.0f/h);

          D_DEBUG_AT(GLES2__2D, "  -> w %d h %d, scale x %f scale y %f\n",
                     w, h, 1.0f/w, 1.0f/h);

          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//          buffer->flags &= ~GLES2BF_UPDATE_TEXTURE;
     }

     // Set the flag.
     GLES2_VALIDATE(SOURCE);
}

/*
 * Called by gles2SetState() to ensure that the color is properly set
 * for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
gles2_validate_COLOR_BLIT(GLES2DriverData *gdrv,
                          GLES2DeviceData *gdev,
                          CardState       *state)
{
     /*
      * DFB's global RGBA color has fixed 8-bit components (u8).  The dfbColor
      * uniform shader variable is a lowp vec4 and expects components in the
      * floating point range [0.0 - 1.0].
      */
     float r, g, b, a;
     float s = 1.0f/255.0f;
     GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     if (state->blittingflags & DSBLIT_COLORIZE) {
          D_DEBUG_AT(GLES2__2D, "  -> DSBLIT_COLORIZE\n");

          r = state->color.r * s;
          g = state->color.g * s;
          b = state->color.b * s;
     }
     else {
          r = g = b = 1.0f;
     }

     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
          D_DEBUG_AT(GLES2__2D, "  -> DSBLIT_BLEND_COLORALPHA\n");

          a = state->color.a * s;
     }
     else {
          a = 1.0f;
     }

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          D_DEBUG_AT(GLES2__2D, "  -> DSBLIT_SRC_PREMULTCOLOR\n");

          r *= a;
          g *= a;
          b *= a;
     }

     glUniform4f(prog->dfbColor, r, g, b, a);
     D_DEBUG_AT(GLES2__2D, "  -> loaded color %f %f %f %f\n", r, g, b, a);

     // Set the flag.
     GLES2_VALIDATE(COLOR_BLIT);
}

/*
 * Called by gles2SetState() to ensure that the colorkey is properly set
 * for execution of blitting functions.  gdev->prog_index must be valid.
 */
static inline void
gles2_validate_COLORKEY(GLES2DriverData *gdrv,
                        GLES2DeviceData *gdev,
                        CardState       *state)
{
     GLES2ProgramInfo *prog = &gdev->progs[gdev->prog_index];

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     /* convert RGB32 color values to int */
     int r = (state->src_colorkey & 0x00FF0000) >> 16;
     int g = (state->src_colorkey & 0x0000FF00) >>  8;
     int b = (state->src_colorkey & 0x000000FF)      ;

     /* send converted color key to shader */
     glUniform3i( prog->dfbColorkey, r, g, b );

     D_DEBUG_AT(GLES2__2D, "  -> loaded colorkey %d %d %d\n", r, g, b);

     // Set the flag.
     GLES2_VALIDATE(COLORKEY);
}

/*
 * Called by gles2SetState() to ensure that the blend functions are properly
 * set for execution of drawing and blitting functions.  This is part of GLES2
 * fixed functionality.
 */
static inline void
gles2_validate_BLENDFUNC(GLES2DriverData *gdrv,
                         GLES2DeviceData *gdev,
                         CardState       *state)
{
     GLenum src = GL_ZERO, dst = GL_ZERO;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     switch (state->src_blend) {
          case DSBF_ZERO:
               break;

          case DSBF_ONE:
               src = GL_ONE;
               break;

          case DSBF_SRCCOLOR:
               src = GL_SRC_COLOR;
               break;

          case DSBF_INVSRCCOLOR:
               src = GL_ONE_MINUS_SRC_COLOR;
               break;

          case DSBF_SRCALPHA:
               src = GL_SRC_ALPHA;
               break;

          case DSBF_INVSRCALPHA:
               src = GL_ONE_MINUS_SRC_ALPHA;
               break;

          case DSBF_DESTALPHA:
               src = GL_DST_ALPHA;
               break;

          case DSBF_INVDESTALPHA:
               src = GL_ONE_MINUS_DST_ALPHA;
               break;

          case DSBF_DESTCOLOR:
               src = GL_DST_COLOR;
               break;

          case DSBF_INVDESTCOLOR:
               src = GL_ONE_MINUS_DST_COLOR;
               break;

          case DSBF_SRCALPHASAT:
               src = GL_SRC_ALPHA_SATURATE;
               break;

          default:
               D_BUG("unexpected src blend function %d", state->src_blend);
     }

     switch (state->dst_blend) {
          case DSBF_ZERO:
               break;

          case DSBF_ONE:
               dst = GL_ONE;
               break;

          case DSBF_SRCCOLOR:
               dst = GL_SRC_COLOR;
               break;

          case DSBF_INVSRCCOLOR:
               dst = GL_ONE_MINUS_SRC_COLOR;
               break;

          case DSBF_SRCALPHA:
               dst = GL_SRC_ALPHA;
               break;

          case DSBF_INVSRCALPHA:
               dst = GL_ONE_MINUS_SRC_ALPHA;
               break;

          case DSBF_DESTALPHA:
               dst = GL_DST_ALPHA;
               break;

          case DSBF_INVDESTALPHA:
               dst = GL_ONE_MINUS_DST_ALPHA;
               break;

          case DSBF_DESTCOLOR:
               dst = GL_DST_COLOR;
               break;

          case DSBF_INVDESTCOLOR:
               dst = GL_ONE_MINUS_DST_COLOR;
               break;

          case DSBF_SRCALPHASAT:
               dst = GL_SRC_ALPHA_SATURATE;
               break;

          default:
               D_BUG("unexpected dst blend function %d", state->dst_blend);
     }

     glBlendFunc(src, dst);

     // Set the flag.
     GLES2_VALIDATE(BLENDFUNC);
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
gles2EngineSync(void *drv, void *dev)
{
     GLES2DriverData *gdrv = drv;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     if (gdrv->calls > 0) {
          glFinish();

          gdrv->calls = 0;
     }

     return DFB_OK;
}

/*
 * Reset the graphics engine.
 */
void
gles2EngineReset(void *drv, void *dev)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);
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
gles2EmitCommands(void *drv, void *dev)
{
     GLES2DriverData *gdrv = drv;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     if (gdrv->calls > 523) {
          glFlush();

          gdrv->calls = 1;
     }
}

/******************************************************************************/

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
gles2CheckState(void                *drv,
                void                *dev,
                CardState           *state,
                DFBAccelerationMask  accel)
{
     D_DEBUG_AT(GLES2__2D, "%s(state %p, accel 0x%08x) <- dest %p [%lu]\n",
                __FUNCTION__, state, accel, state->destination,
                state->dst.offset);

     // Return if the desired function is not supported at all.
     if (accel & ~(GLES2_SUPPORTED_DRAWINGFUNCTIONS |
                   GLES2_SUPPORTED_BLITTINGFUNCTIONS)) {
          D_DEBUG_AT(GLES2__2D, "  -> unsupported function\n");
          return;
     }

     // Check if drawing or blitting is requested.
     if (DFB_DRAWING_FUNCTION(accel)) {
          // Return if unsupported drawing flags are set.
          if (state->drawingflags & ~GLES2_SUPPORTED_DRAWINGFLAGS) {
               D_DEBUG_AT(GLES2__2D, "  -> unsupported drawing flags 0x%08x\n",
                          state->drawingflags);
               return;
          }
     }
     else {
          // Return if unsupported blitting flags are set.
          if (state->blittingflags & ~GLES2_SUPPORTED_BLITTINGFLAGS) {
               D_DEBUG_AT(GLES2__2D, "  -> unsupported blit flags 0x%08x\n",
                          state->blittingflags);
               return;
          }
     }

     // Enable acceleration of the function.
     state->accel |= accel;

     D_DEBUG_AT(GLES2__2D, "  => OK\n");
}

/*
 * Make sure that the hardware is programmed for execution of 'accel'
 * according to the 'state'.
 */
void
gles2SetState(void                *drv,
              void                *dev,
              GraphicsDeviceFuncs *funcs,
              CardState           *state,
              DFBAccelerationMask  accel)
{
     GLES2DriverData       *gdrv     = drv;
     GLES2DeviceData       *gdev     = dev;
     StateModificationFlags modified = state->mod_hw;
     DFBBoolean             blend    = DFB_FALSE;

     D_DEBUG_AT(GLES2__2D,
                "%s(state %p, accel 0x%08x) <- dest %p, modified 0x%08x\n",
                __FUNCTION__, state, accel, state->destination, modified);

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more
      * hardware states.
      */
     if (modified == SMF_ALL) {
          GLES2_INVALIDATE(ALL);
     }
     else if (modified) {
          if (modified & SMF_DESTINATION)
               GLES2_INVALIDATE(DESTINATION);

          if (modified & SMF_CLIP)
               GLES2_INVALIDATE(SCISSOR);

          if (modified & SMF_MATRIX)
               GLES2_INVALIDATE(MATRIX);

          if (modified & SMF_RENDER_OPTIONS)
               GLES2_INVALIDATE(MATRIX | RENDER_OPTS);

          if (modified & SMF_COLOR)
               GLES2_INVALIDATE(COLOR_DRAW | COLOR_BLIT);

          if (modified & SMF_DRAWING_FLAGS)
               GLES2_INVALIDATE(COLOR_DRAW);

          if (modified & SMF_BLITTING_FLAGS)
               GLES2_INVALIDATE(COLOR_BLIT);

          if (modified & SMF_SOURCE)
               GLES2_INVALIDATE(SOURCE);

          if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
               GLES2_INVALIDATE(BLENDFUNC);

          if (modified & SMF_SRC_COLORKEY)
               GLES2_INVALIDATE(COLORKEY);
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */
     GLES2_CHECK_VALIDATE(SCISSOR);
     // GLES2_CHECK_VALIDATE(RENDER_OPTS);

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               // FIXME: workaround state issue in some drivers?
               glBlendFunc(GL_ZERO, GL_ZERO);
               GLES2_INVALIDATE(BLENDFUNC);
               // If alpha blending is used...
               if (state->drawingflags & DSDRAW_BLEND) {
                    // ...require valid blend functions.
                    GLES2_CHECK_VALIDATE(BLENDFUNC);
                    glEnable(GL_BLEND);
               }
               else {
                    glBlendFunc(GL_ONE, GL_ZERO);
                    glDisable(GL_BLEND);
               }

               /*
             * Validate the current shader program.  This can't use the the
             * GLES2_CHECK_VALIDATE macro since the needed state isn't
             * tracked by DFB.
             */
               if (state->render_options & DSRO_MATRIX) {
                    if (gdev->prog_index != GLES2_DRAW_MAT) {
                         // switch to program that uses 3x3 2D matrices
                         gdev->prog_index = GLES2_DRAW_MAT;
                         glUseProgram(gdev->progs[gdev->prog_index].obj);
                    }
               }
               else {
                    if (gdev->prog_index != GLES2_DRAW) {
                         // switch to program that uses scales and offsets
                         gdev->prog_index = GLES2_DRAW;
                         glUseProgram(gdev->progs[gdev->prog_index].obj);
                    }
               }

               // Now that we have a valid program, check destination state.
               GLES2_CHECK_VALIDATE(DESTINATION);

               // Check if the DSRO_MATRIX needs to be loaded.
               GLES2_CHECK_VALIDATE(MATRIX);

               // Check for valid drawing color.
               GLES2_CHECK_VALIDATE(COLOR_DRAW);

               // Enable vertex positions and disable texture coordinates.
               glEnableVertexAttribArray(GLES2VA_POSITIONS);
               glDisableVertexAttribArray(GLES2VA_TEXCOORDS);

               /*
                * 3) Tell which functions can be called without further
                * validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is
                * reset.
                */
               state->set = GLES2_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               // FIXME: workaround state issue in some drivers?
               glBlendFunc(GL_ZERO, GL_ZERO);
               GLES2_INVALIDATE(BLENDFUNC);

               // If alpha blending is used...
               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                           DSBLIT_BLEND_COLORALPHA)) {
                    // ...require valid blend functions.
                    GLES2_CHECK_VALIDATE(BLENDFUNC);
                    glEnable(GL_BLEND);
                    blend = DFB_TRUE;
               }
               else {
                    glBlendFunc(GL_ONE, GL_ZERO);
                    glDisable(GL_BLEND);
                    blend = DFB_FALSE;
               }
               // If normal blitting or color keying is used...
               if (accel == DFXL_BLIT || (state->blittingflags & DSBLIT_SRC_COLORKEY)) {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
               }
               else {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
               }

            /*
             * Validate the current shader program.  This can't use the the
             * GLES2_CHECK_VALIDATE macro since the needed state isn't
             * tracked by DFB.
             */
               if (state->render_options & DSRO_MATRIX) {
                    if (state->blittingflags & DSBLIT_SRC_COLORKEY && !blend) {
                         if (gdev->prog_index != GLES2_BLIT_COLORKEY_MAT) {

                              gdev->prog_index = GLES2_BLIT_COLORKEY_MAT;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);

                              glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                              glEnable(GL_BLEND);
                         }
                    }
                    else if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         if (gdev->prog_index != GLES2_BLIT_PREMULTIPLY_MAT) {

                              gdev->prog_index = GLES2_BLIT_PREMULTIPLY_MAT;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);
                         }
                    }
                    else {
                         if (gdev->prog_index != GLES2_BLIT_MAT) {

                              gdev->prog_index = GLES2_BLIT_MAT;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);
                         }
                    }
               }
               else {
                    if (state->blittingflags & DSBLIT_SRC_COLORKEY && !blend) {
                         if (gdev->prog_index != GLES2_BLIT_COLORKEY) {

                              gdev->prog_index = GLES2_BLIT_COLORKEY;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);

                              glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                              glEnable(GL_BLEND);
                         }
                    }
                    else if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         if (gdev->prog_index != GLES2_BLIT_PREMULTIPLY) {

                              gdev->prog_index = GLES2_BLIT_PREMULTIPLY;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);
                         }
                    }
                    else {
                         if (gdev->prog_index != GLES2_BLIT) {

                              gdev->prog_index = GLES2_BLIT;
                              glUseProgram(gdev->progs[gdev->prog_index].obj);
                         }
                    }
               }

               // We should have a valid program; check destination and source.
               GLES2_CHECK_VALIDATE(DESTINATION);
               GLES2_CHECK_VALIDATE(SOURCE);

               // Check if DSRO_MATRIX needs to be loaded.
               GLES2_CHECK_VALIDATE(MATRIX);

               if (!blend) {
                    // Check if colorkey needs to be loaded.
                    GLES2_CHECK_VALIDATE(COLORKEY);
               }

               /*
             * To reduce the number of shader programs, the blit fragment
             * shader always modulates by a color.  Validate that color.
             */
               GLES2_CHECK_VALIDATE(COLOR_BLIT);

               // Enable vertex positions and texture coordinates.
               glEnableVertexAttribArray(GLES2VA_POSITIONS);
               glEnableVertexAttribArray(GLES2VA_TEXCOORDS);

               /*
          * 3) Tell which functions can be called without further
          * validation, i.e. SetState().
                *
                * When the hw independent state is changed, this collection is
                * reset.
                */
               state->set = GLES2_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG("unexpected drawing/blitting function");
               break;
     }

     gdrv->blittingflags = state->blittingflags;

     /*
      * prog_last is used by some state setting functions to determine if a
      * new program has been loaded; reset it now that state has been updated.
      */
     gdev->prog_last = gdev->prog_index;

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further
      * validation.  If the hw independent state is not modified, this
      * function won't get called for subsequent rendering functions, unless
      * they aren't defined by 3).
      */
     state->mod_hw = 0;

     D_DEBUG_AT(GLES2__2D, "%s(): using shader program \"%s\"\n",
                __FUNCTION__, gdev->progs[gdev->prog_index].name);
}

/******************************************************************************/

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
gles2FillRectangle(void *drv, void *dev, DFBRectangle *rect)
{
     GLES2DriverData *gdrv = drv;

     float x1 = rect->x;
     float y1 = rect->y;
     float x2 = rect->w + x1;
     float y2 = rect->h + y1;

     GLfloat pos[] = {
          x1, y1,   x2, y1,   x2, y2,   x1, y2
     };

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(rect));

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

     // XXX hood - how are these magic numbers determined?
     gdrv->calls += 1 + rect->w * rect->h / (23 * 42);

     return true;
}

/*
 * Render a rectangle outline using the current hardware state.
 */
bool
gles2DrawRectangle(void *drv, void *dev, DFBRectangle *rect)
{
     GLES2DriverData *gdrv = drv;

     float x1 = rect->x + 1;
     float y1 = rect->y + 1;
     float x2 = rect->x + rect->w;
     float y2 = rect->y + rect->h;

     GLfloat pos[] = {
          x1, y1,   x2, y1,   x2, y2,   x1, y2
     };

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(rect));

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glDrawArrays(GL_LINE_LOOP, 0, 4);

     gdrv->calls++;

     return true;
}

/*
 * Render a line using the current hardware state.
 */
bool
gles2DrawLine(void *drv, void *dev, DFBRegion *line)
{
     GLES2DriverData *gdrv = drv;

     float x1 = line->x1;
     float y1 = line->y1;
     float x2 = line->x2;
     float y2 = line->y2;

     GLfloat pos[] = {
          x1, y1,   x2, y2
     };

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4d,%4d)\n",
                __FUNCTION__, DFB_REGION_VALS(line));

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glDrawArrays(GL_LINES, 0, 2);

     gdrv->calls++;

     return true;
}

/*
 * Render a triangle using the current hardware state.
 */
bool
gles2FillTriangle(void *drv, void *dev, DFBTriangle *tri)
{
     GLES2DriverData *gdrv = drv;
     GLfloat pos[] = {
          tri->x1, tri->y1,
          tri->x2, tri->y2,
          tri->x3, tri->y3
     };

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4d,%4d-%4d,%4d)\n", __FUNCTION__,
                tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3);

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glDrawArrays(GL_TRIANGLES, 0, 3);

     gdrv->calls += 23;

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
gles2Blit(void *drv, void *dev, DFBRectangle *srect, int dx, int dy)
{
     GLES2DriverData *gdrv = drv;

     float x1 = dx;
     float y1 = dy;
     float x2 = srect->w + x1;
     float y2 = srect->h + y1;

     float tx1 = srect->x;
     float ty1 = srect->y;
     float tx2 = srect->w + tx1;
     float ty2 = srect->h + ty1;

     GLfloat pos[] = {
          x1, y1,   x2, y1,   x2, y2,   x1, y2
     };

     GLfloat tex[8];

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4dx%4d <- %4d,%4d)\n",
                __FUNCTION__, dx, dy, srect->w, srect->h, srect->x, srect->y);

     if (gdrv->blittingflags & DSBLIT_ROTATE180) {
          tex[0] = tx2; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty1;
     }
     else {
          tex[0] = tx1; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty2;
     }

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glVertexAttribPointer(GLES2VA_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, tex);

     glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

     // XXX hood - how are these magic numbers determined?
     gdrv->calls += 1 + srect->w * srect->h / (23 * 42);

     return true;
}

/*
 * Blit a scaled rectangle using the current hardware state.
 */
bool
gles2StretchBlit(void *drv, void *dev,
                 DFBRectangle *srect, DFBRectangle *drect)
{
     GLES2DriverData *gdrv = drv;

     float x1 = drect->x;
     float y1 = drect->y;
     float x2 = drect->w + x1;
     float y2 = drect->h + y1;

     float tx1 = srect->x;
     float ty1 = srect->y;
     float tx2 = srect->w + tx1;
     float ty2 = srect->h + ty1;

     GLfloat pos[] = {
          x1, y1,   x2, y1,   x2, y2,   x1, y2
     };

     GLfloat tex[8];

     D_DEBUG_AT(GLES2__2D, "%s(%4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALS(drect),
                DFB_RECTANGLE_VALS(srect));

     if (gdrv->blittingflags & DSBLIT_ROTATE180) {
          tex[0] = tx2; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty1;
     }
     else {
          tex[0] = tx1; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty2;
     }

     glVertexAttribPointer(GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos);
     glVertexAttribPointer(GLES2VA_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, tex);

     glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

     // XXX hood - how are these magic numbers determined?
     gdrv->calls += 1 + drect->w * drect->h / (23 * 42);

     return true;
}

