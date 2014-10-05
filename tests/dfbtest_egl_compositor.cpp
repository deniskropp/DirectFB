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

#include "dfbtest_egl_utils.cpp"


static int m_num = 2;

/**********************************************************************************************************************/

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

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB EGL Compositor Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -n, --num                         Number of surfaces to create\n");

     return -1;
}

int
main( int argc, char *argv[] )
{
     DFBResult                ret;
     int                      i, width, height;
     DFBSurfaceDescription    desc;
     IDirectFB                dfb;
     IDirectFBEventBuffer     events;
     IDirectFBSurface         primary;
     Surface                **surfaces = NULL;
     bool                     update   = true;
     std::map<u32,Surface*>   surface_map;
     EGL                      egl;

     /* Initialize DirectFB. */
     DirectFB::Init( &argc, &argv );

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_egl_compositor version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else if (strcmp (arg, "-n") == 0 || strcmp (arg, "--num") == 0) {
               if (++i == argc)
                    return print_usage( argv[0] );

               sscanf( argv[i], "%d", &m_num );

               if (m_num < 1)
                    return print_usage( argv[0] );
          }
          else
               return print_usage( argv[0] );
     }

     surfaces = (Surface**) D_CALLOC( m_num, sizeof(Surface*) );
     if (!surfaces)
          return (int) D_OOM();

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

     for (i=0; i<m_num; i++) {
          surfaces[i] = new Surface( egl, dfb, width - 20 * (m_num + 1), height - 20 * (m_num + 1), events );

          surface_map[ surfaces[i]->GetID() ] = surfaces[i];
     }

     Shader shader( vertex_shader, fragment_shader );

     shader.Use();

     glUniform1i( shader.GetUniformLocation( "sampler" ), 0 );

     glDisable( GL_CULL_FACE );
     glDisable( GL_DEPTH_TEST );

     while (true) {
          DFBEvent  event;
          Surface  *surface;

          while (events.GetEvent( &event )) {
               switch (event.clazz) {
                    case DFEC_SURFACE:
                         switch (event.surface.type) {
                              case DSEVT_UPDATE:
                                   update = true;

                                   D_INFO("Surface Event ID %u count %u\n", event.surface.surface_id, event.surface.flip_count );

                                   surface = surface_map[ event.surface.surface_id ];
                                   if (surface) {
                                        surface->GetSurface().FrameAck( event.surface.flip_count );
                                        surface->UpdateImage();
                                   }
                                   else
                                        D_BUG( "Surface with ID %u not found", event.surface.surface_id );
                                   break;

                              default:
                                   break;
                         }
                         break;

                    default:
                         break;
               }
          }

          if (update) {
               float w = 2.0f / (float) m_num;

               glClear( GL_COLOR_BUFFER_BIT );

               for (i=0; i<m_num; i++) {
                    float pos[] = {
                         -1.0f + i * w/2.0f,      1.0f,     // FIXME: use better coordinates
                         -1.0f + i * w/2.0f + w,  1.0f,
                         -1.0f + i * w/2.0f + w, -1.0f,
                         -1.0f + i * w/2.0f,     -1.0f
                    };

                    float tex[] = {
                          0.0f,  0.0f,
                          1.0f,  0.0f,
                          1.0f,  1.0f,
                          0.0f,  1.0f
                    };

                    surfaces[i]->BindTexture();

                    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, pos );
                    glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 0, tex );

                    glEnableVertexAttribArray( 0 );
                    glEnableVertexAttribArray( 1 );

                    glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
               }

               update = false;

               egl.SwapBuffers();
          }
     }

     if (surfaces) {
          for (i=0; i<m_num; i++) {
               if (surfaces[i])
                    delete surfaces[i];
          }

          D_FREE( surfaces );
     }

     return ret;
}

