/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Ported to GLES2.
 * Kristian Høgsberg <krh@bitplanet.net>
 * May 3, 2010
 *
 * Improve GLES2 port:
 *   * Refactor gear drawing.
 *   * Use correct normals for surfaces.
 *   * Improve shader.
 *   * Use perspective projection transformation.
 *   * Add FPS count.
 *   * Add comments.
 * Alexandros Frantzis <alexandros.frantzis@linaro.org>
 * Jul 13, 2010
 */


//#define DFBEGL_ENABLE_MANGLE

#include <++dfb.h>

#include "dfbapp.h"

#include <core/Util.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/egldfbext.h>



static const char vertex_shader[] =
"attribute vec3 position;\n"
"attribute vec2 texcoords;\n"
"varying   vec2 varTexCoord;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_Position = vec4(position, 1.0);\n"
"    varTexCoord.s = texcoords.x;\n"
"    varTexCoord.t = texcoords.y;\n"
"}";

static const char fragment_shader[] =
"precision mediump float;\n"
"uniform sampler2D sampler;\n"
"varying vec2      varTexCoord;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = texture2D(sampler, varTexCoord);\n"
"}";


static void
gears_init(void)
{
     GLuint v, f, program;
     const char *p;

     glDisable(GL_CULL_FACE);
     glDisable(GL_DEPTH_TEST);

     program = glCreateProgram();

     /* Compile the vertex shader */
     p = vertex_shader;
     v = glCreateShader(GL_VERTEX_SHADER);
     glShaderSource(v, 1, &p, NULL);
     glCompileShader(v);


     GLint  log_length, char_count;


     GLint status;

     glGetShaderiv(v, GL_COMPILE_STATUS, &status);
     if (status) {
          glAttachShader(program, v);
          glDeleteShader(v); // mark for deletion on detach
     }
     else {
          glGetShaderiv(v, GL_INFO_LOG_LENGTH, &log_length);

          DirectFB::Util::TempArray<GLchar> log( log_length );

          glGetShaderInfoLog( v, log_length, &char_count, log );

          fprintf(stderr,"%s: vertex shader compilation failure:\n%s\n", __FUNCTION__, *log);
     }


     /* Compile the fragment shader */
     p = fragment_shader;
     f = glCreateShader(GL_FRAGMENT_SHADER);
     glShaderSource(f, 1, &p, NULL);
     glCompileShader(f);


     glGetShaderiv(f, GL_COMPILE_STATUS, &status);
     if (status) {
          glAttachShader(program, f);
          glDeleteShader(f); // mark for deletion on detach
     }
     else {
          glGetShaderiv(f, GL_INFO_LOG_LENGTH, &log_length);

          DirectFB::Util::TempArray<GLchar> log( log_length );

          glGetShaderInfoLog( f, log_length, &char_count, log );

          fprintf(stderr,"%s: fragment shader compilation failure:\n%s\n", __FUNCTION__, *log);
     }



     /* Create and link the shader program */
     glBindAttribLocation(program, 0, "position");
     glBindAttribLocation(program, 1, "texcoords");

     glLinkProgram(program);
     glValidateProgram(program);


     glGetProgramiv(program, GL_LINK_STATUS, &status);


     // Report errors.  Shader objects detached when program is deleted.
     glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

     DirectFB::Util::TempArray<GLchar> log( log_length );

     glGetProgramInfoLog(program, log_length, &char_count, log);
     fprintf(stderr,"%s: shader program link failure:\n%s\n", __FUNCTION__, *log);


     /* Enable the shaders */
     glUseProgram(program);
}




#define EGL_CHECK(cmd)                                      \
     /*fprintf(stderr, "CALLING %s...\n", #cmd);*/              \
     if (cmd) {                                             \
          fprintf(stderr, "!!! %s failed\n", #cmd);         \
          goto quit;                                        \
     }


class EGLInit
{
public:
     EGLDisplay display;
     EGLConfig  configs[2];
     EGLContext context;
     EGLSurface surface;

public:
     EGLInit( IDirectFB_C        *dfb,
              IDirectFBSurface_C *dfb_surface )
     {
          EGLint width, height;
          EGLint major, minor, nconfigs;
          EGLint attribs[] = {
               EGL_BUFFER_SIZE,    EGL_DONT_CARE,
               EGL_DEPTH_SIZE,          16,
               EGL_RED_SIZE,       5,
               EGL_GREEN_SIZE,          6,
               EGL_RED_SIZE,       5,
               EGL_RENDERABLE_TYPE,     EGL_OPENGL_ES2_BIT,
               EGL_NONE
          };
          EGLint context_attrs[] = {
               EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE
          };
          EGLint surface_attrs[] = {
               EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE
          };

          EGLint render_buffer = 0;

          sleep(4);
          // get display
          EGL_CHECK((display = eglGetDisplay((EGLNativeDisplayType) dfb)) == EGL_NO_DISPLAY)

          // init
          EGL_CHECK(!eglInitialize(display, &major, &minor))

          // get configs
          EGL_CHECK(!eglGetConfigs(display, configs, 2, &nconfigs))

          // choose config
          EGL_CHECK(!eglChooseConfig(display, attribs, configs, 2, &nconfigs))


          // create a surface
          EGL_CHECK((surface = eglCreateWindowSurface(display, configs[0], (EGLNativeWindowType) dfb_surface, surface_attrs)) == EGL_NO_SURFACE)

          EGL_CHECK(eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)

          // create context
          EGL_CHECK((context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attrs)) == EGL_NO_CONTEXT)

          EGL_CHECK(eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)

          eglQuerySurface(display, surface, EGL_WIDTH, &width);
          eglQuerySurface(display, surface, EGL_HEIGHT, &height);

          EGL_CHECK(!eglQuerySurface(display, surface, EGL_RENDER_BUFFER, &render_buffer));
     //     fprintf(stderr,"RENDER_BUFFER = 0x%04x\n", render_buffer );



          eglSwapInterval( display, 1 );
quit:
          return;
     }
};



class DFBTestEGLImage : public DFBApp {
public:
     DFBTestEGLImage() {
     }

     virtual ~DFBTestEGLImage() {
     }

private:
     /* called after initialization */
     virtual bool Setup( int width, int height ) {
          EGLInit egl( m_dfb.iface, m_primary.iface );

          /* Set the viewport */
          glViewport( 0, 0, width, height );


          DFBSurfaceDescription desc;

          desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
          desc.width       = 512;
          desc.height      = 512;
          desc.pixelformat = DSPF_ARGB;

          dfb_image = m_dfb.CreateSurface( desc );




          typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, EGLint *attr_list);
          typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);


          PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress( "eglCreateImageKHR" );

          egl_image = eglCreateImageKHR( egl.display, EGL_NO_CONTEXT, EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB, dfb_image.iface, NULL );

          if (egl_image == NULL) {
               D_ERROR( "DFBTest/EGLImage: eglCreateImageKHR() returned 0!\n" );
               return false;
          }


          typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
          typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);

          PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress( "glEGLImageTargetTexture2DOES" );


          glGenTextures( 1, &texture );

          glBindTexture( GL_TEXTURE_2D, texture );

          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

          glGetError();
          glEGLImageTargetTexture2DOES_( GL_TEXTURE_2D, egl_image );
          glEGLImageTargetTexture2DOES_( GL_TEXTURE_2D, egl_image );
          GLenum error = glGetError();

          if (error != GL_NO_ERROR) {
               D_ERROR( "DFBTest/EGLImage: glEGLImageTargetTexture2D() failed (0x%04x)!\n", error );
               return false;
          }



          return true;
     }

     /* render callback */
     virtual void Render( IDirectFBSurface &surface ) {
          int x = ((int) surface.GetWidth()  - (int) dfb_image.GetWidth())  / 2;
          int y = ((int) surface.GetHeight() - (int) dfb_image.GetHeight()) / 2;

          surface.Clear();

          

          surface.Blit( dfb_image, NULL, x, y );
     }

     bool ParseArgs( int argc, char *argv[] ) {
          return true;
     }

private:
     IDirectFBSurface dfb_image;
     EGLImageKHR      egl_image;
     GLuint           texture;
};

int
main( int argc, char *argv[] )
{
     DFBTestEGLImage app;

     try {
          /* Initialize DirectFB command line parsing. */
          DirectFB::Init( &argc, &argv );

          /* Parse remaining arguments and run. */
          if (app.Init( argc, argv ))
               app.Run();
     }
     catch (DFBException *ex) {
          /*
           * Exception has been caught, destructor of 'app' will deinitialize
           * anything at return time (below) that got initialized until now.
           */
          std::cerr << std::endl;
          std::cerr << "Caught exception!" << std::endl;
          std::cerr << "  -- " << ex << std::endl;
     }

     return 0;
}

