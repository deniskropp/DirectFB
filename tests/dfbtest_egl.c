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


//#define DFBEGL_ENABLE_MANGLE

#include <directfb.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>



static const char vertex_shader[] =
"attribute mediump vec2 position;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_Position = vec4(position.x, position.y, 0.0, 1.0);\n"
"}";

static const char fragment_shader[] =
"void main(void)\n"
"{\n"
"    gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );\n"
"}";

static void
shader_init(void)
{
     GLuint v, f, program;
     const char *p;

     glEnable(GL_CULL_FACE);
     glEnable(GL_DEPTH_TEST);

     program = glCreateProgram();

     /* Compile the vertex shader */
     p = vertex_shader;
     v = glCreateShader(GL_VERTEX_SHADER);
     glShaderSource(v, 1, &p, NULL);
     glCompileShader(v);


     GLint  log_length, char_count;
     char *log;


     GLint status;

     glGetShaderiv(v, GL_COMPILE_STATUS, &status);
     if (status) {
          glAttachShader(program, v);
          glDeleteShader(v); // mark for deletion on detach
     }
     else {
          glGetShaderiv(v, GL_INFO_LOG_LENGTH, &log_length);

          log = malloc(log_length);

          glGetShaderInfoLog(v, log_length, &char_count, log);

          fprintf(stderr,"%s: vertex shader compilation failure:\n%s\n", __FUNCTION__, log);
          free(log);
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

          log = malloc(log_length);

          glGetShaderInfoLog(f, log_length, &char_count, log);

          fprintf(stderr,"%s: fragment shader compilation failure:\n%s\n", __FUNCTION__, log);
          free(log);
     }



     /* Create and link the shader program */
     glBindAttribLocation(program, 0, "position");

     glLinkProgram(program);
     glValidateProgram(program);


     glGetProgramiv(program, GL_LINK_STATUS, &status);

     if (status) {
          // Don't need the shader objects anymore.
          GLuint  shaders[2];
          GLsizei shader_count;

          glGetAttachedShaders(program, 2, &shader_count, shaders);

          glDetachShader(program, shaders[0]);
          glDetachShader(program, shaders[1]);
     }
     else {
          // Report errors.  Shader objects detached when program is deleted.
          glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

          log = malloc(log_length);

          glGetProgramInfoLog(program, log_length, &char_count, log);
          fprintf(stderr,"%s: shader program link failure:\n%s\n", __FUNCTION__, log);
          free(log);
     }


     int l1 = glGetAttribLocation(program, "position");

     printf("location: %d\n",l1);

     /* Enable the shaders */
     glUseProgram(program);
}









typedef struct {
     IDirectFB             *dfb;
     IDirectFBSurface      *primary;
     IDirectFBEventBuffer  *events;
     DFBDimension           size;
} Test;


static DFBResult
Initialize( Test   *test,
            int    *argc,
            char ***argv )
{
     DFBResult             ret;
     DFBSurfaceDescription dsc;

     /*
      * Initialize DirectFB options
      */
     ret = DirectFBInit( argc, argv );
     if (ret) {
          D_DERROR( ret, "DirectFBInit() failed!\n" );
          return ret;
     }

     /*
      * Create the super interface
      */
     ret = DirectFBCreate( &test->dfb );
     if (ret) {
          D_DERROR( ret, "DirectFBCreate() failed!\n" );
          return ret;
     }

     /*
      * Create an event buffer for all devices with these caps
      */
     ret = test->dfb->CreateInputEventBuffer( test->dfb, DICAPS_KEYS | DICAPS_AXES, DFB_FALSE, &test->events );
     if (ret) {
          D_DERROR( ret, "IDirectFB::CreateInputEventBuffer( DICAPS_KEYS | DICAPS_AXES ) failed!\n" );
          return ret;
     }

     /*
      * Try to set our cooperative level to DFSCL_FULLSCREEN for exclusive access to the primary layer
      */
     test->dfb->SetCooperativeLevel( test->dfb, DFSCL_FULLSCREEN );

     /*
      * Create the primary surface
      */
     dsc.flags = DSDESC_CAPS;
     dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     ret = test->dfb->CreateSurface( test->dfb, &dsc, &test->primary );
     if (ret) {
          D_DERROR( ret, "IDirectFB::CreateSurface( DSCAPS_PRIMARY | DSCAPS_FLIPPING ) failed!\n" );
          return ret;
     }

     /*
      * Get the size of the surface, clear and show it
      */
     test->primary->GetSize( test->primary, &test->size.w, &test->size.h );

     test->primary->Clear( test->primary, 0, 0, 0, 0 );
     test->primary->Flip( test->primary, NULL, 0 );


     return DFB_OK;
}

static void
Shutdown( Test *test )
{
     if (test->primary)
          test->primary->Release( test->primary );

     if (test->events)
          test->events->Release( test->events );

     if (test->dfb)
          test->dfb->Release( test->dfb );
}







static EGLDisplay display;
static EGLConfig configs[2];
static EGLContext context;
static EGLSurface surface;


static DFBResult
InitGL( Test *test )
{
     EGLint major, minor, nconfigs;
     EGLint attribs[] = {
          EGL_SURFACE_TYPE,        EGL_WINDOW_BIT,
          EGL_RED_SIZE,            1,
          EGL_GREEN_SIZE,          1,
          EGL_BLUE_SIZE,           1,
          EGL_RENDERABLE_TYPE,     EGL_OPENGL_ES2_BIT,
          EGL_NONE
     };
     EGLint context_attrs[] = {
          EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE
     };
     EGLint surface_attrs[] = {
          EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE
     };
     EGLNativeDisplayType     disp = EGL_DEFAULT_DISPLAY;

#define EGL_CHECK(cmd)                                      \
     /*fprintf(stderr, "CALLING %s...\n", #cmd);*/              \
     if (cmd) {                                             \
          fprintf(stderr, "!!! %s failed\n", #cmd);         \
          goto quit;                                        \
     }

     // get display
     EGL_CHECK((display = eglGetDisplay(disp)) == EGL_NO_DISPLAY)

     // init
     EGL_CHECK(!eglInitialize(display, &major, &minor))

     // get configs
     EGL_CHECK(!eglGetConfigs(display, configs, 2, &nconfigs))

     // choose config
     EGL_CHECK(!eglChooseConfig(display, attribs, configs, 2, &nconfigs))


     // create a surface
     EGL_CHECK((surface = eglCreateWindowSurface(display, configs[0], test->primary, surface_attrs)) == EGL_NO_SURFACE)

     EGL_CHECK(eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)

     // create context
     EGL_CHECK((context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attrs)) == EGL_NO_CONTEXT)

     EGL_CHECK(eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)


     eglSwapInterval( display, 1 );


     /* Setup the viewport */
     glViewport( 0, 0, (GLint) test->size.w, (GLint) test->size.h );


     return DFB_OK;

quit:
     return DFB_FAILURE;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     bool      quit = false;
     Test      test;

     memset( &test, 0, sizeof(test) );


     ret = Initialize( &test, &argc, &argv );
     if (ret)
          goto error;

     ret = InitGL( &test );
     if (ret)
          goto error;

     shader_init();

     /*
      * Main Loop
      */
     while (!quit) {
          DFBInputEvent evt;

          const static GLfloat v[6] = { -1.0, -1.0,  1.0, 0.0,  -1.0, 1.0 };

          glClearColor(1.0, 1.0, 1.0, 1.0);
          glClear(GL_COLOR_BUFFER_BIT);

          glDisable( GL_CULL_FACE );
          glDisable( GL_DEPTH_TEST );

          glEnableVertexAttribArray( 0 );
          glVertexAttribPointer( 0, 2, GL_FLOAT, GL_TRUE, 0, v );

          glDrawArrays( GL_TRIANGLE_FAN, 0, 3 );

          eglSwapBuffers( display, surface );

          sleep(3);

          /*
           * Process events
           */
          while (test.events->GetEvent( test.events, DFB_EVENT(&evt) ) == DFB_OK) {
               switch (evt.type) {
                    case DIET_KEYPRESS:
                         switch (evt.key_symbol) {
                              case DIKS_CURSOR_LEFT:
                                   //view_rot[1] += 5.0;
                                   break;
                              case DIKS_CURSOR_RIGHT:
                                   //view_rot[1] -= 5.0;
                                   break;
                              case DIKS_CURSOR_UP:
                                   //view_rot[0] += 5.0;
                                   break;
                              case DIKS_CURSOR_DOWN:
                                   //view_rot[0] -= 5.0;
                                   break;
                              case DIKS_ESCAPE:
                                   quit = true;
                                   break;
                              default:
                                   ;
                         }
                         break;
                    case DIET_KEYRELEASE:
                         switch (evt.key_symbol) {
                              default:
                                   ;
                         }
                         break;
                    case DIET_AXISMOTION:
                         if (evt.flags & DIEF_AXISREL) {
                              switch (evt.axis) {
                                   default:
                                        ;
                              }
                         }
                         break;
                    default:
                         ;
               }
          }
     }


error:
     Shutdown( &test );

     return ret;
}

