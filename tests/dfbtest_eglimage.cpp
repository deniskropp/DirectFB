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

#include "dfbtest_egl_utils.cpp"


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


int
main( int argc, char *argv[] )
{
     int                      width, height;
     DFBSurfaceDescription    desc;
     IDirectFB                dfb;
     IDirectFBEventBuffer     events;
     IDirectFBSurface         primary;

     EGL                      egl;

     IDirectFBSurface         dfb_image;
     EGLImageKHR              egl_image;
     GLuint                   texture;

     /* Initialize DirectFB. */
     DirectFB::Init( &argc, &argv );

     /* Create super interface. */
     dfb = DirectFB::Create();

     /* Create an event buffer. */
     events = dfb.CreateEventBuffer();

     dfb.SetCooperativeLevel( DFSCL_FULLSCREEN );

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = (DFBSurfaceCapabilities)(DSCAPS_PRIMARY | DSCAPS_FLIPPING);

     /* Create a primary surface. */
     primary = dfb.CreateSurface( desc );


     egl.Initialise( dfb, primary, true );

     primary.GetSize( &width, &height );


     Shader shader( vertex_shader, fragment_shader );

     shader.Use();

     glUniform1i( shader.GetUniformLocation( "sampler" ), 0 );

     glDisable(GL_CULL_FACE);
     glDisable(GL_DEPTH_TEST);

     /* Set the viewport */
     glViewport( 0, 0, width, height );


     desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc.width       = 512;
     desc.height      = 512;
     desc.pixelformat = DSPF_ARGB;

     dfb_image = dfb.CreateSurface( desc );



     dfb_image.Clear( 0xff, 0xff, 0xff, 0xff );

     dfb_image.SetColor( 0x80, 0x80, 0x80, 0x80 );
     dfb_image.FillRectangle( 0, 0, 256, 256 );



     typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, EGLint *attr_list);
     //typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);


     PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress( "eglCreateImageKHR" );

     egl_image = eglCreateImageKHR( egl.GetDisplay(), EGL_NO_CONTEXT, EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB, dfb_image.iface, NULL );

     if (egl_image == NULL) {
          D_ERROR( "DFBTest/EGLImage: eglCreateImageKHR() returned 0!\n" );
          return 1;
     }


     typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
     //typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);

     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress( "glEGLImageTargetTexture2DOES" );


     glGenTextures( 1, &texture );

     glBindTexture( GL_TEXTURE_2D, texture );

     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

     glGetError();
     glEGLImageTargetTexture2DOES_( GL_TEXTURE_2D, egl_image );
     GLenum error = glGetError();

     if (error != GL_NO_ERROR) {
          D_ERROR( "DFBTest/EGLImage: glEGLImageTargetTexture2D() failed (0x%04x)!\n", error );
          return 1;
     }


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

     egl.SwapBuffers();

     pause();

     return 0;
}

