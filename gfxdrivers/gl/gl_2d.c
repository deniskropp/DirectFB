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
#include <core/system.h>

#include <gfx/convert.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include "gl_2d.h"
#include "gl_gfxdriver.h"


D_DEBUG_DOMAIN( GL__2D, "GL/2D", "OpenGL 2D Acceleration" );

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

     SOURCE       = 0x00000100,
     COLOR_BLIT   = 0x00000200,
     SRC_COLORKEY = 0x00000400,

     BLENDFUNC    = 0x00010000,

     ALL          = 0x0001031F
};

/*
 * State handling macros.
 */

#define GL_VALIDATE(flags)        do { gdev->v_flags |=  (flags); } while (0)
#define GL_INVALIDATE(flags)      do { gdev->v_flags &= ~(flags); } while (0)

#define GL_CHECK_VALIDATE(flag)   do {                                               \
                                       if ((gdev->v_flags & flag) != flag)           \
                                            gl_validate_##flag( gdrv, gdev, state ); \
                                  } while (0)


/**********************************************************************************************************************/

/*
 * Called by glSetState() to ensure that the destination parameters are properly set
 * for execution of rendering functions.
 */
static inline void
gl_validate_DESTINATION( GLDriverData *gdrv,
                         GLDeviceData *gdev,
                         CardState    *state )
{
     CoreSurface  *surface = state->destination;
     GLBufferData *buffer  = state->dst.handle;

     D_DEBUG_AT( GL__2D, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, GLBufferData );

     if (buffer->flags & GLBF_UPDATE_TARGET) {
          glViewport( 0, 0, surface->config.size.w, surface->config.size.h );

          glMatrixMode( GL_PROJECTION );
          glLoadIdentity();
          glOrtho( 0, surface->config.size.w, 0, surface->config.size.h, -1, 1 );

          glMatrixMode( GL_MODELVIEW );
          glLoadIdentity();
          glScalef( 1, -1, 1 );
          glTranslatef( 0, - surface->config.size.h, 0 );

          glShadeModel( GL_FLAT );
          glDisable( GL_LIGHTING );

          glDepthMask( GL_FALSE );
          glDisable( GL_DEPTH_TEST );

          glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );
          glDisable( GL_CULL_FACE );

          glEnable( GL_SCISSOR_TEST );

          GL_INVALIDATE( ALL );

          buffer->flags &= ~GLBF_UPDATE_TARGET;
     }

     /* Set the flag. */
     GL_VALIDATE( DESTINATION );
}

/*
 * Called by glSetState() to ensure that the clip is properly set
 * for execution of rendering functions.
 */
static inline void
gl_validate_SCISSOR( GLDriverData *gdrv,
                     GLDeviceData *gdev,
                     CardState    *state )
{
     CoreSurface *surface = state->destination;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     glScissor( state->clip.x1,
                surface->config.size.h - state->clip.y2 - 1,
                state->clip.x2 - state->clip.x1 + 1,
                state->clip.y2 - state->clip.y1 + 1 );

     /* Set the flag. */
     GL_VALIDATE( SCISSOR );
}

/*
 * Called by glSetState() to ensure that the matrix is properly set
 * for execution of rendering functions.
 */
static inline void
gl_validate_MATRIX( GLDriverData *gdrv,
                    GLDeviceData *gdev,
                    CardState    *state )
{
     CoreSurface *surface = state->destination;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     glMatrixMode( GL_MODELVIEW );
     glLoadIdentity();
     glScalef( 1, -1, 1 );
     glTranslatef( 0, - surface->config.size.h, 0 );

     if (state->render_options & DSRO_MATRIX) {
          float m[16] = { 0 };

#define M(n)   (state->matrix[n] / 65536.0f)

          m[0] = M(0); m[4] = M(1); m[ 8] = 0.0f; m[12] = M(2);
          m[1] = M(3); m[5] = M(4); m[ 9] = 0.0f; m[13] = M(5);
          m[2] = 0.0f; m[6] = 0.0f; m[10] = 1.0f; m[14] = 0.0f;
          m[3] = M(6); m[7] = M(7); m[11] = 0.0f; m[15] = M(8);

#undef M

          D_DEBUG_AT( GL__2D, "  -> %7.2f %7.2f %7.2f %7.2f\n", m[0], m[4], m[8], m[12] );
          D_DEBUG_AT( GL__2D, "  -> %7.2f %7.2f %7.2f %7.2f\n", m[1], m[5], m[9], m[13] );
          D_DEBUG_AT( GL__2D, "  -> %7.2f %7.2f %7.2f %7.2f\n", m[2], m[6], m[10], m[14] );
          D_DEBUG_AT( GL__2D, "  -> %7.2f %7.2f %7.2f %7.2f\n", m[3], m[7], m[11], m[15] );

          glMultMatrixf( m );
     }

     /* Set the flag. */
     GL_VALIDATE( MATRIX );
}

/*
 * Called by glSetState() to ensure that the rendering options are properly set
 * for execution of rendering functions.
 */
static inline void
gl_validate_RENDER_OPTS( GLDriverData *gdrv,
                         GLDeviceData *gdev,
                         CardState    *state )
{
     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     if (state->render_options & DSRO_ANTIALIAS) {
          glEnable( GL_LINE_SMOOTH );
          //glEnable( GL_POLYGON_SMOOTH );
     }
     else {
          glDisable( GL_LINE_SMOOTH );
          //glDisable( GL_POLYGON_SMOOTH );
     }

     /* Set the flag. */
     GL_VALIDATE( RENDER_OPTS );
}

/*
 * Called by glSetState() to ensure that the color is properly set
 * for execution of drawing functions.
 */
static inline void
gl_validate_COLOR_DRAW( GLDriverData *gdrv,
                        GLDeviceData *gdev,
                        CardState    *state )
{
     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          int A = state->color.a + 1;

          glColor4ub( (state->color.r * A) >> 8,
                      (state->color.g * A) >> 8,
                      (state->color.b * A) >> 8, state->color.a );
     }
     else
          glColor4ub( state->color.r, state->color.g, state->color.b, state->color.a );

     /* Set the flag. */
     GL_VALIDATE( COLOR_DRAW );

     /* Invalidates blitting color. */
     GL_INVALIDATE( COLOR_BLIT );
}

/*
 * Called by glSetState() to ensure that the source parameters are properly set
 * for execution of blitting functions.
 */
static inline void
gl_validate_SOURCE( GLDriverData *gdrv,
                    GLDeviceData *gdev,
                    CardState    *state )
{
     GLBufferData *buffer = state->src.handle;

     D_DEBUG_AT( GL__2D, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, GLBufferData );

     glBindTexture( GL_TEXTURE_RECTANGLE_ARB, buffer->texture );

     if (buffer->flags & GLBF_UPDATE_TEXTURE) {
          glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
          glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

          glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
          glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

          buffer->flags &= ~GLBF_UPDATE_TEXTURE;
     }

     /* Set the flag. */
     GL_VALIDATE( SOURCE );
}

/*
 * Called by glSetState() to ensure that the color is properly set
 * for execution of blitting functions.
 */
static inline void
gl_validate_COLOR_BLIT( GLDriverData *gdrv,
                        GLDeviceData *gdev,
                        CardState    *state )
{
     int r, g, b, a;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     if (state->blittingflags & DSBLIT_COLORIZE) {
          r = state->color.r;
          g = state->color.g;
          b = state->color.b;
     }
     else
          r = g = b = 0xff;

     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
          a = state->color.a;
     else
          a = 0xff;

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          int A = state->color.a + 1;

          r = (r * A) >> 8;
          g = (g * A) >> 8;
          b = (b * A) >> 8;
     }

     glColor4ub( r, g, b, a );

     /* Set the flag. */
     GL_VALIDATE( COLOR_BLIT );

     /* Invalidates drawing color. */
     GL_INVALIDATE( COLOR_DRAW );
}

/*
 * Called by glSetState() to ensure that the colorkey is properly set
 * for execution of blitting functions.
 */
static inline void
gl_validate_SRC_COLORKEY( GLDriverData *gdrv,
                          GLDeviceData *gdev,
                          CardState    *state )
{
     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     /* convert RGB32 color values to float */
     float fr = (float)((state->src_colorkey & 0x00FF0000) >> 16)/255.0f;
     float fg = (float)((state->src_colorkey & 0x0000FF00) >>  8)/255.0f;
     float fb = (float)((state->src_colorkey & 0x000000FF)      )/255.0f;
          
     /* send float converted color key to shader */
     glUniform3fARB( gdev->location_colorkey, fr, fg, fb );

     /* Set the flag. */
     GL_VALIDATE( SRC_COLORKEY );     
}

/*
 * Called by glSetState() to ensure that the blend functions are properly set
 * for execution of drawing and blitting functions.
 */
static inline void
gl_validate_BLENDFUNC( GLDriverData *gdrv,
                       GLDeviceData *gdev,
                       CardState    *state )
{
     GLenum src = GL_ZERO, dst = GL_ZERO;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

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
               D_BUG( "unexpected src blend function %d", state->src_blend );
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
               D_BUG( "unexpected dst blend function %d", state->dst_blend );
     }

     glBlendFunc( src, dst );

     /* Set the flag. */
     GL_VALIDATE( BLENDFUNC );
}

/**********************************************************************************************************************/

/*
 * Wait for the blitter to be idle.
 *
 * This function is called before memory that has been written to by the hardware is about to be
 * accessed by the CPU (software driver) or another hardware entity like video encoder (by Flip()).
 * It can also be called by applications explicitly, e.g. at the end of a benchmark loop to include
 * execution time of queued commands in the measurement.
 */
DFBResult
glEngineSync( void *drv, void *dev )
{
     GLDriverData *gdrv = drv;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

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
glEngineReset( void *drv, void *dev )
{
     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );
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
glEmitCommands( void *drv, void *dev )
{
     GLDriverData *gdrv = drv;

     D_DEBUG_AT( GL__2D, "%s()\n", __FUNCTION__ );

     if (gdrv->calls > 523) {
          glFlush();

          gdrv->calls = 1;
     }
}

/**********************************************************************************************************************/
static bool
printGLInfoLog(GLhandleARB obj)
{
     int infologLength = 0;
     int charsWritten  = 0;
     char *infoLog;

     glGetObjectParameterivARB( obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infologLength );
     if (infologLength > 1) {
          infoLog = (char *)malloc( infologLength );
          glGetInfoLogARB( obj, infologLength, &charsWritten, infoLog );
          D_WARN( "OpenGL InfoLog: %s\n",infoLog );
          free( infoLog );
          return true;
     }
     return false;
}

static bool
compile_shader( GLDeviceData *gdev )
{
     GLhandleARB f, p;

     p = glCreateProgramObjectARB();

     if (p == -1) return false;

     f = glCreateShaderObjectARB( GL_FRAGMENT_SHADER );

     static const char * ff = "#extension GL_ARB_texture_rectangle : enable\n"
                              "\n"
                              "uniform sampler2DRect myTexture;\n"
                              "uniform vec3 src_colorkey;\n"              
                              "void main (void)\n" 
                              "{\n" 
                              "   vec4 value = texture2DRect(myTexture, vec2(gl_TexCoord[0]));\n"
                              "   if (vec3(value) == src_colorkey)\n"
                              "      discard;\n"
                              "   gl_FragColor = value;\n"
                              "}\n";

     glShaderSourceARB( f, 1, &ff, NULL );
     glCompileShaderARB( f );
     glAttachObjectARB( p, f );
     glLinkProgramARB( p );

     if (printGLInfoLog( p ))
          return false;

     gdev->shader_colorkey = p;
     gdev->location_colorkey = glGetUniformLocationARB( p ,"src_colorkey" );

     return true;
}

/**********************************************************************************************************************/

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
glCheckState( void                *drv,
              void                *dev,
              CardState           *state,
              DFBAccelerationMask  accel )
{
     GLDeviceData *gdev = (GLDeviceData*) dev;

     D_DEBUG_AT( GL__2D, "%s( state %p, accel 0x%08x ) <- dest %p [%lu]\n", __FUNCTION__,
                 state, accel, state->destination, state->dst.offset );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(GL_SUPPORTED_DRAWINGFUNCTIONS | GL_SUPPORTED_BLITTINGFUNCTIONS)) {
          D_DEBUG_AT( GL__2D, "  -> unsupported function\n" );
          return;
     }

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               break;

          default:
               D_DEBUG_AT( GL__2D, "  -> unsupported destination format %s\n",
                           dfb_pixelformat_name(state->destination->config.format) );
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~GL_SUPPORTED_DRAWINGFLAGS) {
               D_DEBUG_AT( GL__2D, "  -> unsupported drawing flags 0x%08x\n", state->drawingflags );
               return;
          }
     }
     else {
          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB:
               case DSPF_RGB32:
                    break;

               default:
                    D_DEBUG_AT( GL__2D, "  -> unsupported source format %s\n",
                                dfb_pixelformat_name(state->source->config.format) );
                    return;
          }

          /* Return if unsupported blitting flags are set. */
          if (state->blittingflags & ~gdev->supported_blittingflags) {
               /* 
                * if there are unsupported blittingflags, check if DSBLIT_SRC_COLORKEY 
                * is among them. If so, try to compile a fragment shader for colorkeying, 
                * if it worked, add DSBLIT_SRC_COLORKEY to the supported blittingflags. 
                *  
                * The downside is that source colorkeying will never be reported in the 
                * CardCapabilities. So applications querieing them will get the wrong 
                * information. 
                *  
                * Compilation of shaders should be done in init_driver(), but OpenGL is not 
                * ready to compile shaders there. 
                *  
                * FIXME: find a better solution 
                */ 
               if (!gdev->glsl_probed && (state->blittingflags & DSBLIT_SRC_COLORKEY)) {
                         if (compile_shader(gdev)) {
                              gdev->has_glsl = true;
                              gdev->supported_blittingflags |= DSBLIT_SRC_COLORKEY;
                         }
                         gdev->glsl_probed = true;
                         /* recheck: return if one of the blitting flags is still unsupported */
                         if (state->blittingflags & ~gdev->supported_blittingflags)
                              return;
               }
               else {
                    D_DEBUG_AT( GL__2D, "  -> unsupported blitting flags 0x%08x\n", state->blittingflags );
     
                    return;
               }
          }
     }

     /* Enable acceleration of the function. */
     state->accel |= accel;

     D_DEBUG_AT( GL__2D, "  => OK\n" );
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
glSetState( void                *drv,
            void                *dev,
            GraphicsDeviceFuncs *funcs,
            CardState           *state,
            DFBAccelerationMask  accel )
{
     GLDriverData           *gdrv     = drv;
     GLDeviceData           *gdev     = dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( GL__2D, "%s( state %p, accel 0x%08x ) <- dest %p, modified 0x%08x\n", __FUNCTION__,
                 state, accel, state->destination, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          GL_INVALIDATE( ALL );
     }
     else if (modified) {
          if (modified & SMF_DESTINATION)
               GL_INVALIDATE( DESTINATION );

          if (modified & SMF_CLIP)
               GL_INVALIDATE( SCISSOR );

          if (modified & SMF_MATRIX)
               GL_INVALIDATE( MATRIX );

          if (modified & SMF_RENDER_OPTIONS)
               GL_INVALIDATE( MATRIX | RENDER_OPTS );

          if (modified & SMF_COLOR)
               GL_INVALIDATE( COLOR_DRAW | COLOR_BLIT );

          if (modified & SMF_DRAWING_FLAGS)
               GL_INVALIDATE( COLOR_DRAW );

          if (modified & SMF_BLITTING_FLAGS)
               GL_INVALIDATE( COLOR_BLIT );

          if (modified & SMF_SRC_COLORKEY)
               GL_INVALIDATE( SRC_COLORKEY );

          if (modified & SMF_SOURCE)
               GL_INVALIDATE( SOURCE );

          if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
               GL_INVALIDATE( BLENDFUNC );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination, clip, matrix and options... */
     GL_CHECK_VALIDATE( DESTINATION );
     GL_CHECK_VALIDATE( SCISSOR );
     GL_CHECK_VALIDATE( MATRIX );
     GL_CHECK_VALIDATE( RENDER_OPTS );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               glUseProgramObjectARB(0);
               glDisable( GL_TEXTURE_RECTANGLE_ARB );

               /* ...require valid drawing color. */
               GL_CHECK_VALIDATE( COLOR_DRAW );

               /* If alpha blending is used... */
               if (state->drawingflags & DSDRAW_BLEND) {
                    /* ...require valid blend functions. */
                    GL_CHECK_VALIDATE( BLENDFUNC );

                    glEnable( GL_BLEND );
               }
               else
                    glDisable( GL_BLEND );


               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = GL_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               glEnable( GL_TEXTURE_RECTANGLE_ARB );

               /* ...require valid source. */
               GL_CHECK_VALIDATE( SOURCE );


               /* If alpha blending is used... */
               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
                    /* ...require valid blend functions. */
                    GL_CHECK_VALIDATE( BLENDFUNC );

                    glEnable( GL_BLEND );
               }
               else
                    glDisable( GL_BLEND );

               if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                    glUseProgramObjectARB(gdev->shader_colorkey);
                    GL_CHECK_VALIDATE( SRC_COLORKEY );
               } 
               else
                    glUseProgramObjectARB(0);

               /* If colorizing or premultiplication of global alpha is used... */
               if (state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR | DSBLIT_BLEND_COLORALPHA)) {
                    /* ...require valid color. */
                    GL_CHECK_VALIDATE( COLOR_BLIT );

                    /* Enable texture modulation */
                    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
               }
               else
                    /* Disable texture modulation */
                    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );


               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = GL_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     gdrv->blittingflags = state->blittingflags;

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
glFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     GLDriverData *gdrv = (GLDriverData*)drv;

     int x1 = rect->x;
     int y1 = rect->y;
     int x2 = rect->w + x1;
     int y2 = rect->h + y1;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     glBegin( GL_QUADS );

     glVertex2i( x1, y1 );
     glVertex2i( x2, y1 );
     glVertex2i( x2, y2 );
     glVertex2i( x1, y2 );

     glEnd();


     gdrv->calls += 1 + rect->w * rect->h / (23 * 42);

     return true;
}

/*
 * Render a rectangle outline using the current hardware state.
 */
bool
glDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     GLDriverData *gdrv = drv;

     int x1 = rect->x + 1;
     int y1 = rect->y;
     int x2 = rect->x + rect->w;
     int y2 = rect->y + rect->h - 1;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     glBegin( GL_LINE_LOOP );

     glVertex2i( x1, y1 );
     glVertex2i( x2, y1 );
     glVertex2i( x2, y2 );
     glVertex2i( x1, y2 );

     glEnd();


     gdrv->calls++;

     return true;
}

/*
 * Render a line using the current hardware state.
 */
bool
glDrawLine( void *drv, void *dev, DFBRegion *line )
{
     GLDriverData *gdrv = drv;

     int x1 = line->x1;
     int y1 = line->y1;
     int x2 = line->x2;
     int y2 = line->y2;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4d,%4d )\n", __FUNCTION__, DFB_REGION_VALS( line ) );

     glBegin( GL_LINES );

     glVertex2i( x1, y1 );
     glVertex2i( x2, y2 );

     glEnd();


     gdrv->calls++;

     return true;
}

/*
 * Render a line using the current hardware state.
 */
bool
glFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     GLDriverData *gdrv = drv;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4d,%4d-%4d,%4d )\n", __FUNCTION__,
                 tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3 );

     glBegin( GL_TRIANGLES );

     glVertex2i( tri->x1, tri->y1 );
     glVertex2i( tri->x2, tri->y2 );
     glVertex2i( tri->x3, tri->y3 );

     glEnd();


     gdrv->calls += 23;

     return true;
}

/*
 * Blit a rectangle using the current hardware state.
 */
bool
glBlit( void *drv, void *dev, DFBRectangle *srect, int dx, int dy )
{
     GLDriverData *gdrv = drv;

     int x1 = dx;
     int y1 = dy;
     int x2 = srect->w + x1;
     int y2 = srect->h + y1;

     int tx1 = srect->x;
     int ty1 = srect->y;
     int tx2 = srect->w + tx1;
     int ty2 = srect->h + ty1;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4dx%4d <- %4d,%4d )\n", __FUNCTION__,
                 dx, dy, srect->w, srect->h, srect->x, srect->y );

     /* Might also use GL_TEXTURE matrix, but isn't this less overhead in state management? */
     if (gdrv->blittingflags & DSBLIT_ROTATE180) {
          int tmp;

          tmp = tx1; tx1 = tx2; tx2 = tmp;
          tmp = ty1; ty1 = ty2; ty2 = tmp;
     }

     glBegin( GL_QUADS );

     glTexCoord2i( tx1, ty1 );
     glVertex2i( x1, y1 );

     glTexCoord2i( tx2, ty1 );
     glVertex2i( x2, y1 );

     glTexCoord2i( tx2, ty2 );
     glVertex2i( x2, y2 );

     glTexCoord2i( tx1, ty2 );
     glVertex2i( x1, y2 );

     glEnd();


     gdrv->calls += 1 + srect->w * srect->h / (23 * 42);

     return true;
}

/*
 * Blit a scaled rectangle using the current hardware state.
 */
bool
glStretchBlit( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect )
{
     GLDriverData *gdrv = drv;

     int x1 = drect->x;
     int y1 = drect->y;
     int x2 = drect->w + x1;
     int y2 = drect->h + y1;

     int tx1 = srect->x;
     int ty1 = srect->y;
     int tx2 = srect->w + tx1;
     int ty2 = srect->h + ty1;

     D_DEBUG_AT( GL__2D, "%s( %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( drect ), DFB_RECTANGLE_VALS( srect ) );

     /* Might also use GL_TEXTURE matrix, but isn't this less overhead in state management? */
     if (gdrv->blittingflags & DSBLIT_ROTATE180) {
          int tmp;

          tmp = tx1; tx1 = tx2; tx2 = tmp;
          tmp = ty1; ty1 = ty2; ty2 = tmp;
     }

     glBegin( GL_QUADS );

     glTexCoord2i( tx1, ty1 );
     glVertex2i( x1, y1 );

     glTexCoord2i( tx2, ty1 );
     glVertex2i( x2, y1 );

     glTexCoord2i( tx2, ty2 );
     glVertex2i( x2, y2 );

     glTexCoord2i( tx1, ty2 );
     glVertex2i( x1, y2 );

     glEnd();


     gdrv->calls += 1 + drect->w * drect->h / (23 * 42);

     return true;
}

