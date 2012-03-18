/*
   (c) Copyright 2011  Denis Oliver Kropp

   All rights reserved.

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

#include <stdio.h>
#include <string.h>

#include <direct/messages.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>


/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Surface Updates Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");

     return -1;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     int                     i;
     DFBSurfaceDescription   desc;
     IDirectFB              *dfb;
     IDirectFBEventBuffer   *events;
     IDirectFBSurface       *surface = NULL;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceUpdates: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_surface_updates version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceUpdates: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Fill description for a primary surface. */
     desc.flags  = DSDESC_WIDTH | DSDESC_HEIGHT;
     desc.width  = 320;
     desc.height = 240;

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &surface );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceUpdates: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     /* Create event buffer */
     ret = surface->CreateEventBuffer( surface, &events );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceUpdates: IDirectFBSurface::CreateEventBuffer() failed!\n" );
          goto out;
     }


     while (true) {
          DFBEvent event;

          surface->Flip( surface, NULL, DSFLIP_NONE );

          while (events->GetEvent( events, &event ) == DFB_OK) {
               switch (event.clazz) {
                    case DFEC_SURFACE:
                         switch (event.surface.type) {
                              case DSEVT_UPDATE:
                                   printf( "Update %4d,%4d-%4d,%4d\n",
                                           event.surface.update.x1,
                                           event.surface.update.y1,
                                           event.surface.update.x2,
                                           event.surface.update.y2 );
                                   break;

                              default:
                                   break;
                         }
                         break;

                    default:
                         break;
               }
          }
     }

out:
     if (surface)
          surface->Release( surface );

     if (events)
          events->Release( events );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

