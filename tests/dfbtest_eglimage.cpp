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


#include <++dfb.h>

#include "dfbapp.h"

#include <core/Util.h>

extern "C" {
#include <direct/direct.h>
}

#include <core/Debug.h>

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



static const char vertex_shader[] =
"attribute vec2 position;\n"
"attribute vec2 texcoords;\n"
"varying   vec2 varTexCoord;\n"
"\n"
"void main(void)\n"
"{\n"
"    varTexCoord.s = texcoords.x;\n"
"    varTexCoord.t = texcoords.y;\n"
"    gl_Position = vec4(position.x, position.y, 0.0, 1.0);\n"
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


     glUniform1i( glGetUniformLocation( program, "sampler" ), 0 );
}




#define EGL_CHECK(cmd)                                      \
     /*fprintf(stderr, "CALLING %s...\n", #cmd);*/              \
     if (cmd) {                                             \
          fprintf(stderr, "!!! %s failed\n", #cmd);         \
          goto quit;                                        \
     }


class DFBTestEGLImage : public DFBApp {
public:
     DFBTestEGLImage() {
     }

     virtual ~DFBTestEGLImage() {
     }

private:
     EGLDisplay display;
     EGLConfig  configs[2];
     EGLContext context;
     EGLSurface egl_surface;

     void InitEGL( IDirectFB_C        *dfb,
                   IDirectFBSurface_C *dfb_surface )
     {
          EGLint width, height;
          EGLint major, minor, nconfigs;
          EGLint attribs[] = {
               EGL_SURFACE_TYPE,        EGL_WINDOW_BIT,
               EGL_RED_SIZE,            1,
               EGL_GREEN_SIZE,          1,
               EGL_BLUE_SIZE,           1,
//               EGL_ALPHA_SIZE,          1,
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
          const char *client_apis;

          // get display
          EGL_CHECK((display = eglGetDisplay((EGLNativeDisplayType) 0)) == EGL_NO_DISPLAY)

          // init
          EGL_CHECK(!eglInitialize(display, &major, &minor))

          client_apis = eglQueryString( display, EGL_CLIENT_APIS );
          D_INFO("EGL_CLIENT_APIS: '%s'\n", client_apis);

          // get configs
          EGL_CHECK(!eglGetConfigs(display, configs, 2, &nconfigs))

          // choose config
          EGL_CHECK(!eglChooseConfig(display, attribs, configs, 2, &nconfigs))

          // create a surface
          EGL_CHECK((egl_surface = eglCreateWindowSurface(display, configs[0], (EGLNativeWindowType) dfb_surface, surface_attrs)) == EGL_NO_SURFACE)

          EGL_CHECK(eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)

          // create context
          EGL_CHECK((context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attrs)) == EGL_NO_CONTEXT)

          EGL_CHECK(eglMakeCurrent(display, egl_surface, egl_surface, context) != EGL_TRUE)

          eglQuerySurface(display, egl_surface, EGL_WIDTH, &width);
          eglQuerySurface(display, egl_surface, EGL_HEIGHT, &height);

          EGL_CHECK(!eglQuerySurface(display, egl_surface, EGL_RENDER_BUFFER, &render_buffer));


          eglSwapInterval( display, 1 );
     quit:
          return;
     }

     /* called after initialization */
     virtual bool Setup( int width, int height ) {
          InitEGL( m_dfb.iface, m_primary.iface );

          gears_init();

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

          egl_image = eglCreateImageKHR( display, EGL_NO_CONTEXT, EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB, dfb_image.iface, NULL );

          if (egl_image == NULL) {
               D_ERROR( "DFBTest/EGLImage: eglCreateImageKHR() returned 0!\n" );
               return false;
          }


          typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
          typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);

          PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress( "glEGLImageTargetTexture2DOES" );



          dfb_image.Clear( 0xff, 0xff, 0xff, 0xff );

          dfb_image.SetColor( 0x80, 0x80, 0x80, 0x80 );
          dfb_image.FillRectangle( 0, 0, 256, 256 );



          glGenTextures( 1, &texture );

          glBindTexture( GL_TEXTURE_2D, texture );

          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

          glGetError();
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
          //int x = ((int) surface.GetWidth()  - (int) dfb_image.GetWidth())  / 2;
          //int y = ((int) surface.GetHeight() - (int) dfb_image.GetHeight()) / 2;

          glClear( GL_COLOR_BUFFER_BIT );

          float pos[] = {
               -1.0f,  1.0f,
                1.0f,  1.0f,
                1.0f, -1.0f,
               -1.0f, -1.0f
          };

          float tex[] = {
                0.0f,  0.0f,
                1.0f,  0.0f,
                1.0f,  1.0f,
                0.0f,  1.0f
          };

          glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, pos );
          glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 0, tex );

          glEnableVertexAttribArray( 0 );
          glEnableVertexAttribArray( 1 );

          glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

          eglSwapBuffers( display, egl_surface );
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

     direct_initialize();

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

