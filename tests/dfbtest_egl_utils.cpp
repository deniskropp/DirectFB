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

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <map>

#include <++dfb.h>

#include <core/Util.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


/**********************************************************************************************************************/

#define EGL_CHECK(cmd)                                      \
     if (cmd) {                                             \
          fprintf(stderr, "!!! %s failed\n", #cmd);         \
          throw std::runtime_error( #cmd );                 \
     }

/**********************************************************************************************************************/

class Shader
{
public:
     Shader( const char *vertex_source,
             const char *fragment_source )
          :
          compiled( false ),
          vertex_source( vertex_source ),
          fragment_source( fragment_source )
     {
     }

     ~Shader()
     {
          if (compiled)
               glDeleteProgram( program );
     }

     int GetAttribLocation( const char *name )
     {
          compile();

          std::map<std::string,int>::const_iterator it = attribs.find( name );

          if (it != attribs.end())
               return (*it).second;

          attribs[name] = glGetAttribLocation( program, name );
     }

     int GetUniformLocation( const char *name )
     {
          compile();

          std::map<std::string,int>::const_iterator it = uniforms.find( name );

          if (it != uniforms.end())
               return (*it).second;

          int location = glGetUniformLocation( program, name );

          uniforms[name] = location;

          return location;
     }

     void Use()
     {
          compile();

          glUseProgram( program );
     }

private:
     bool                          compiled;
     const char                   *vertex_source;
     const char                   *fragment_source;
     GLuint                        program;
     std::map<std::string,int>     attribs;
     std::map<std::string,int>     uniforms;

     void
     compile()
     {
          if (compiled)
               return;

          compiled = true;

          GLuint      v, f;
          const char *p;

          program = glCreateProgram();

          /* Compile the vertex shader */
          p = vertex_source;
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
          p = fragment_source;
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


          glLinkProgram(program);
          glValidateProgram(program);


          glGetProgramiv(program, GL_LINK_STATUS, &status);


          // Report errors.  Shader objects detached when program is deleted.
          glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

          DirectFB::Util::TempArray<GLchar> log( log_length );

          glGetProgramInfoLog(program, log_length, &char_count, log);
          fprintf(stderr,"%s: shader program link log:\n%s\n", __FUNCTION__, *log);
     }
};

/**********************************************************************************************************************/

class EGL
{
public:
     EGL()
          :
          egl_display(0),
          egl_config(0),
          egl_context(0),
          egl_surface(0)
     {
     }

     ~EGL()
     {
     }

     void Initialise( IDirectFB        dfb,
                      IDirectFBSurface surface,
                      bool             depth )
     {
          EGLint major, minor, nconfigs;

          EGLint attribs[] = {
               EGL_BUFFER_SIZE,     EGL_DONT_CARE,
               EGL_ALPHA_SIZE,      0,
               EGL_RED_SIZE,        0,
               EGL_GREEN_SIZE,      0,
               EGL_BLUE_SIZE,       0,
               EGL_DEPTH_SIZE,      depth ? 16 : 0,
               EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
               EGL_SURFACE_TYPE,    0,
               EGL_NONE
          };

          EGLint context_attrs[] = {
               EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE
          };

          EGLint surface_attrs[] = {
               EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE
          };


          // get display
          EGL_CHECK((egl_display = eglGetDisplay( (EGLNativeDisplayType) dfb.iface )) == EGL_NO_DISPLAY)

          CreateImageKHR             = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress( "eglCreateImageKHR" );
          DestroyImageKHR            = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress( "eglDestroyImageKHR" );
          GetConfigAttribsDIRECTFB   = (PFNEGLGETCONFIGATTRIBSDIRECTFB) eglGetProcAddress( "eglGetConfigAttribsDIRECTFB" );
          EGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress( "glEGLImageTargetTexture2DOES" );

          // init
          EGL_CHECK(!eglInitialize( egl_display, &major, &minor ))

          GetConfigAttribsDIRECTFB( egl_display, (EGLNativePixmapType) surface.iface, attribs, 0 );

          // choose config
          EGL_CHECK(!eglChooseConfig( egl_display, attribs, &egl_config, 1, &nconfigs ))

          // create a surface
          EGL_CHECK((egl_surface = eglCreateWindowSurface( egl_display, egl_config, (EGLNativeWindowType) surface.iface, surface_attrs )) == EGL_NO_SURFACE)

          EGL_CHECK(eglBindAPI( EGL_OPENGL_ES_API ) != EGL_TRUE)

          // create context
          EGL_CHECK((egl_context = eglCreateContext( egl_display, egl_config, EGL_NO_CONTEXT, context_attrs )) == EGL_NO_CONTEXT)

          EGL_CHECK(eglMakeCurrent( egl_display, egl_surface, egl_surface, egl_context ) != EGL_TRUE)

          eglSwapInterval( egl_display, 1 );

          size = surface.GetSize();

          /* Setup the viewport */
          glViewport( 0, 0, size.w, size.h );
     }

     void SwapBuffers()
     {
          eglSwapBuffers( egl_display, egl_surface );
     }

     EGLImageKHR CreateImage( EGLContext context, EGLenum target, EGLClientBuffer buffer, const EGLint *attr_list )
     {
          return CreateImageKHR( egl_display, context, target, buffer, attr_list );
     }

     void DestroyImage( EGLImageKHR image )
     {
          DestroyImageKHR( egl_display, image );
     }

     void EGLImageTargetTexture2D( GLenum target, GLeglImageOES image )
     {
          EGLImageTargetTexture2DOES( target, image );
     }

     EGLDisplay GetDisplay() const { return egl_display; }

private:
     EGLDisplay                              egl_display;
     EGLConfig                               egl_config;
     EGLContext                              egl_context;
     EGLSurface                              egl_surface;
     DFBDimension                            size;
     PFNEGLGETCONFIGATTRIBSDIRECTFB          GetConfigAttribsDIRECTFB;
     PFNEGLCREATEIMAGEKHRPROC                CreateImageKHR;
     PFNEGLDESTROYIMAGEKHRPROC               DestroyImageKHR;
     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC     EGLImageTargetTexture2DOES;
};


class Surface
{
public:
     Surface( EGL                  &egl,
              IDirectFB             dfb,
              int                   width,
              int                   height,
              IDirectFBEventBuffer  events )
          :
          egl( egl )
     {
          DFBSurfaceDescription desc;

          /* Fill description for a shared offscreen surface. */
          desc.flags  = (DFBSurfaceDescriptionFlags)(DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT);
          desc.caps   = (DFBSurfaceCapabilities)(DSCAPS_SHARED | DSCAPS_TRIPLE);
          desc.width  = width;
          desc.height = height;

          /* Create a primary surface. */
          surface = dfb.CreateSurface( desc );

          surface.MakeClient();

          /* Create event buffer */
          surface.AttachEventBuffer( events );

          surface_id = surface.GetID();

          surface.AllowAccess( "*" );

          surface.Clear( 0xff, 0xff, 0xff, 0xff );

          D_INFO( "DFBTest/EGLCompositor: Created new surface with ID %u\n", surface_id );

          image = egl.CreateImage( EGL_NO_CONTEXT, EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB, (EGLClientBuffer) surface.iface, NULL );

          glGenTextures( 1, &tex );

          glBindTexture( GL_TEXTURE_2D, tex );

          egl.EGLImageTargetTexture2D( GL_TEXTURE_2D, image );

          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );


          glDisable( GL_CULL_FACE );
          glDisable( GL_DEPTH_TEST );
     }

     ~Surface()
     {
          glDeleteTextures( 1, &tex );
          egl.DestroyImage( image );
     }

     void UpdateImage()
     {
          egl.DestroyImage( image );

          image = egl.CreateImage( EGL_NO_CONTEXT, EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB, (EGLClientBuffer) surface.iface, NULL );

          glBindTexture( GL_TEXTURE_2D, tex );

          egl.EGLImageTargetTexture2D( GL_TEXTURE_2D, image );
     }

     void BindTexture()
     {
          glBindTexture( GL_TEXTURE_2D, tex );
     }

     IDirectFBSurface GetSurface() const { return surface; }
     DFBSurfaceID     GetID() const { return surface_id; }

private:
     EGL              &egl;
     IDirectFBSurface  surface;
     DFBSurfaceID      surface_id;
     EGLImageKHR       image;
     GLuint            tex;
};

