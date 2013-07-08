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

#include <stdio.h>
#include <string.h>

#include <direct/hash.h>
#include <direct/messages.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>


static int m_num = 2;

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Surface Compositor Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -n, --num                         Number of surfaces to create\n");

     return -1;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     int                     i, width, height;
     DFBSurfaceDescription   desc;
     IDirectFB              *dfb;
     IDirectFBEventBuffer   *events;
     IDirectFBSurface       *primary  = NULL;
     IDirectFBSurface      **surfaces = NULL;
     bool                    update   = true;
     DirectHash              surface_map;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceCompositor: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_surface_compositor version %s\n", DIRECTFB_VERSION);
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

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceCompositor: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Create an event buffer. */
     ret = dfb->CreateEventBuffer( dfb, &events );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceCompositor: IDirectFB::CreateEventBuffer() failed!\n" );
          goto out;
     }

     surfaces = D_CALLOC( m_num, sizeof(IDirectFBSurface*) );
     if (!surfaces) {
          ret = D_OOM();
          goto out;
     }

     direct_hash_init( &surface_map, 17 );

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &primary );
     if (ret) {
          D_DERROR( ret, "DFBTest/SurfaceCompositor: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     primary->GetSize( primary, &width, &height );

     /* Fill description for a shared offscreen surface. */
     desc.flags  = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT;
     desc.caps   = DSCAPS_SHARED | DSCAPS_TRIPLE;
     desc.width  = width  - 20 * (m_num + 1);
     desc.height = height - 20 * (m_num + 1);

     for (i=0; i<m_num; i++) {
          DFBSurfaceID surface_id;

          /* Create a primary surface. */
          ret = dfb->CreateSurface( dfb, &desc, &surfaces[i] );
          if (ret) {
               D_DERROR( ret, "DFBTest/SurfaceCompositor: IDirectFB::CreateSurface() failed!\n" );
               goto out;
          }

          surfaces[i]->MakeClient( surfaces[i] );

          /* Create event buffer */
          ret = surfaces[i]->AttachEventBuffer( surfaces[i], events );
          if (ret) {
               D_DERROR( ret, "DFBTest/SurfaceCompositor: IDirectFBSurface::AttachEventBuffer() failed!\n" );
               goto out;
          }

          surfaces[i]->GetID( surfaces[i], &surface_id );

          surfaces[i]->AllowAccess( surfaces[i], "*" );

          D_INFO( "DFBTest/SurfaceCompositor: Surface %d has ID %d\n", i+1, surface_id );

          direct_hash_insert( &surface_map, surface_id, surfaces[i] );
     }

     while (true) {
          DFBEvent          event;
          IDirectFBSurface *surface;

          while (events->GetEvent( events, &event ) == DFB_OK) {
               switch (event.clazz) {
                    case DFEC_SURFACE:
                         switch (event.surface.type) {
                              case DSEVT_UPDATE:
                                   update = true;

                                   surface = direct_hash_lookup( &surface_map, event.surface.surface_id );
                                   if (surface)
                                        surface->FrameAck( surface, event.surface.flip_count );
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
               primary->Clear( primary, 0, 0, 0, 0x80 );

               for (i=0; i<m_num; i++) {
                    primary->Blit( primary, surfaces[i], NULL, 20 + 20 * i, 20 + 20 * i );
               }

               primary->Flip( primary, NULL, DSFLIP_NONE );

               update = false;
          }
     }

out:
     if (surfaces) {
          for (i=0; i<m_num; i++) {
               if (surfaces[i])
                    surfaces[i]->Release( surfaces[i] );
          }

          D_FREE( surfaces );
     }

     if (primary)
          primary->Release( primary );

     if (events)
          events->Release( events );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

