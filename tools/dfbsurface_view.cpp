/*
   (c) Copyright 2012-2014  DirectFB integrated media GmbH
   (c) Copyright 2001-2014  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdexcept>

#include <++dfb.h>

extern "C" {
#include <direct/memcpy.h>
}

#include <SDL.h>


D_LOG_DOMAIN( DFBTool_SurfaceView, "DFBTool/SurfaceView", "DFB Surface View" );

/**********************************************************************************************************************/

static DFBSurfaceID g_surface_id;

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Surface View (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options] <surface-id>\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");

     return -1;
}

/**********************************************************************************************************************/

class TestDisplay
{
public:
     virtual void Init() = 0;
     virtual void Show( void               *data,
                        int                 pitch,
                        const DFBDimension &size,
                        const DFBRegion    &update ) = 0;
};

/**********************************************************************************************************************/

class SDLDisplay : public TestDisplay
{
public:
     SDLDisplay();

     virtual void Init();
     virtual void Show( void               *data,
                        int                 pitch,
                        const DFBDimension &size,
                        const DFBRegion    &update );

private:
     SDL_Surface  *screen;
     DFBDimension  size;
};

SDLDisplay::SDLDisplay()
     :
     screen( NULL )
{
}

void
SDLDisplay::Init()
{
     char *driver = getenv( "SDL_VIDEODRIVER" );

     if (driver && !strcasecmp( driver, "directfb" )) {
          D_LOG( DFBTool_SurfaceView, INFO, "SDL_VIDEODRIVER is 'directfb', unsetting it.\n" );
          unsetenv( "SDL_VIDEODRIVER" );
     }

     /* Initialize SDL */
     if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
          D_ERROR_AT( DFBTool_SurfaceView, "Couldn't initialize SDL: %s\n", SDL_GetError() );
          throw std::runtime_error( "" );
     }
}

void
SDLDisplay::Show( void               *data,
                  int                 pitch,
                  const DFBDimension &size,
                  const DFBRegion    &update )
{
     if (this->size != size) {
          screen = SDL_SetVideoMode( size.w, size.h, 32, SDL_HWSURFACE | SDL_RESIZABLE );
          if (!screen) {
               D_ERROR_AT( DFBTool_SurfaceView, "Couldn't set %dx%dx%d video mode: %s\n",
                           size.w, size.h, 32, SDL_GetError());
               throw std::runtime_error( "" );
          }

          this->size = size;
     }

     if (SDL_LockSurface( screen ) < 0) {
          D_ERROR_AT( DFBTool_SurfaceView, "Couldn't lock the display surface: %s\n", SDL_GetError() );
          throw std::runtime_error( "" );
     }

     for (int y=update.y1; y<=update.y2; y++) {
          D_ASSERT( y < size.h );
          D_ASSERT( update.x1 < size.w );
          D_ASSERT( update.x2 < size.w );
          D_ASSERT( update.x1 <= update.x2 );

          direct_memcpy( (u8*) screen->pixels + screen->pitch * y + update.x1 * 4,
                         (u8*) data + pitch * y + update.x1 * 4,
                         (update.x2 - update.x1 + 1) * 4 );
     }

     SDL_UnlockSurface( screen );

     SDL_UpdateRect( screen, DFB_RECTANGLE_VALS_FROM_REGION(&update) );
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     SDLDisplay           display;

     IDirectFB            dfb;
     IDirectFBEventBuffer events;
     IDirectFBSurface     surface;
     DFBDimension         size;
     DFBUpdates           updates;

     /* Initialize DirectFB. */
     DirectFB::Init( &argc, &argv );

     /* Parse arguments. */
     for (int i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_surface_compositor version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else if (strcmp (arg, "-q") == 0 || strcmp (arg, "--quiet") == 0) {
               direct_log_domain_config_level( "DFBTool/SurfaceView", DIRECT_LOG_ERROR );
          }
          else {
               if (sscanf( argv[i], "%u", &g_surface_id ) != 1)
                    return print_usage( argv[0] );
          }
     }

     if (!g_surface_id)
          return print_usage( argv[0] );

     /* Create super interface */
     dfb = DirectFB::Create();

     /* Create an event buffer */
     events = dfb.CreateEventBuffer();

     /* Get the surface to view */
     surface = dfb.GetSurface( g_surface_id );

     /* Attach the event buffer */
     surface.AttachEventBuffer( events );

     surface.iface->MakeClient( surface.iface );


     size = surface.GetSize();

     D_LOG( DFBTool_SurfaceView, INFO, "Initial config %dx%d\n", size.w, size.h );


     display.Init();

     D_LOG( DFBTool_SurfaceView, INFO, "Display initialised\n" );


     updates |= size;


     while (true) {
          /* Process events */
          DFBEvent evt;
          while (events.GetEvent( DFB_EVENT(&evt) )) {
               switch (evt.clazz) {
                    case DFEC_SURFACE:
                         switch (evt.surface.type) {
                              case DSEVT_UPDATE:
                                   D_LOG( DFBTool_SurfaceView, INFO, "UPDATE event %d,%d-%dx%d (flips %u)\n",
                                          DFB_RECTANGLE_VALS_FROM_REGION(&evt.surface.update), evt.surface.flip_count );
                                   updates |= evt.surface.update;
                                   surface.iface->FrameAck( surface.iface, evt.surface.flip_count );
                                   break;

                              case DSEVT_FRAME:
                                   D_LOG( DFBTool_SurfaceView, INFO, "FRAME event %d,%d-%dx%d (buffer id 0x%x, serial %u)\n",
                                          DFB_RECTANGLE_VALS_FROM_REGION(&evt.surface.update), evt.surface.left_id, evt.surface.left_serial );
                                   updates |= evt.surface.update;
                                   break;

                              case DSEVT_CONFIG:
                                   size = evt.surface.size;
                                   D_LOG( DFBTool_SurfaceView, INFO, "CONFIG event %dx%d\n", evt.surface.size.w, evt.surface.size.h );
                                   break;

                              default:
                                   break;
                         }
                         break;

                    default:
                         break;
               }
          }

          if (updates.num_regions) {
               void *data;
               int   pitch;

               D_LOG( DFBTool_SurfaceView, INFO, "Updating content %d,%d-%dx%d\n",
                      DFB_RECTANGLE_VALS_FROM_REGION(&updates.bounding) );

               surface.Lock( DSLF_READ, &data, &pitch );

               display.Show( data, pitch, size, updates.bounding );

               surface.Unlock();

               updates.Reset();
          }

          events.WaitForEventWithTimeout( 3, 0 );
     }

     return 0;
}

