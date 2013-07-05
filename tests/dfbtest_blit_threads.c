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


static int  m_num    = 2;
static bool m_render = false;
static bool m_double = false;

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Blit Threaded Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -n, --num                         Number of threads to create\n");
     fprintf (stderr, "  -r, --render                      Let threads render to source before blitting\n");
     fprintf (stderr, "  -d, --double                      Use double buffered source surface\n");

     return -1;
}

/**********************************************************************************************************************/

typedef struct {
     IDirectFBSurface *destination;
     IDirectFBSurface *source;
     DFBColor          color;
} BlittingThreadContext;

static void *
blitting_thread( DirectThread *thread,
                 void         *arg )
{
     BlittingThreadContext *ctx = arg;

     while (true) {
          ctx->destination->Blit( ctx->destination, ctx->source, NULL, 0, 0 );

          /* Flush (single buffered Flip just flushes) */
          ctx->destination->Flip( ctx->destination, NULL, DSFLIP_NONE );
     }

     return NULL;
}

static void *
blitting_thread_render( DirectThread *thread,
                        void         *arg )
{
     BlittingThreadContext *ctx = arg;

     while (true) {
//          ctx->source->Clear( ctx->source, ctx->color.r, ctx->color.g, ctx->color.b, ctx->color.a );

          ctx->source->Blit( ctx->source, ctx->source, NULL, 0, 0 );

          /* Flush (single buffered Flip just flushes) */
          ctx->source->Flip( ctx->source, NULL, DSFLIP_NONE );

          sleep(1);
     }

     return NULL;
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
     IDirectFBSurface       *primary      = NULL;
     IDirectFBSurface      **destinations = NULL;
     IDirectFBSurface      **sources      = NULL;
     DirectThread          **threads      = NULL;
     BlittingThreadContext  *contexts     = NULL;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/BlitThreads: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_blit_threads version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else if (strcmp (arg, "-n") == 0 || strcmp (arg, "--num") == 0) {
               if (++i == argc)
                    return print_usage( argv[0] );

               sscanf( argv[i], "%d", &m_num );

               if (m_num < 1)
                    return print_usage( argv[0] );
          }
          else if (strcmp (arg, "-r") == 0 || strcmp (arg, "--render") == 0) {
               m_render = true;
          }
          else if (strcmp (arg, "-d") == 0 || strcmp (arg, "--double") == 0) {
               m_double = true;
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/BlitThreads: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Create an event buffer. */
     ret = dfb->CreateEventBuffer( dfb, &events );
     if (ret) {
          D_DERROR( ret, "DFBTest/BlitThreads: IDirectFB::CreateEventBuffer() failed!\n" );
          goto out;
     }

     sources = D_CALLOC( m_num, sizeof(IDirectFBSurface*) );
     if (!sources) {
          ret = D_OOM();
          goto out;
     }

     destinations = D_CALLOC( m_num, sizeof(IDirectFBSurface*) );
     if (!destinations) {
          ret = D_OOM();
          goto out;
     }

     threads = D_CALLOC( m_num, sizeof(DirectThread*) );
     if (!threads) {
          ret = D_OOM();
          goto out;
     }

     contexts = D_CALLOC( m_num, sizeof(BlittingThreadContext) );
     if (!contexts) {
          ret = D_OOM();
          goto out;
     }

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY;

     /* Create a single buffered primary surface (to see threads' output immediately without flip). */
     ret = dfb->CreateSurface( dfb, &desc, &primary );
     if (ret) {
          D_DERROR( ret, "DFBTest/BlitThreads: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     primary->GetSize( primary, &width, &height );

     /* Fill description for a source surface. */
     desc.flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_CAPS;
     desc.width  = 64;
     desc.height = 64;
     desc.caps   = m_double ? DSCAPS_DOUBLE : DSCAPS_NONE;

     for (i=0; i<m_num; i++) {
          DFBRectangle rect;

          /* Create a source surface. */
          ret = dfb->CreateSurface( dfb, &desc, &sources[i] );
          if (ret) {
               D_DERROR( ret, "DFBTest/BlitThreads: IDirectFB::CreateSurface() failed!\n" );
               goto out;
          }

          /* Create a sub surface as destination */
          rect.x = 100 + 100 * (i % ((width - 250) / 100));
          rect.y = 100 + 100 * (i / ((width - 250) / 100));
          rect.w = 64;
          rect.h = 64;

          ret = primary->GetSubSurface( primary, &rect, &destinations[i] );
          if (ret) {
               D_DERROR( ret, "DFBTest/BlitThreads: IDirectFBSurface::GetSubSurface() failed!\n" );
               goto out;
          }

          contexts[i].destination = destinations[i];
          contexts[i].source      = sources[i];
          contexts[i].color       = (DFBColor){ 0xff, 0x55 + i * 0x22, 0x11 + i * 0x77, 0xdd + i * 0x55 };

          sources[i]->Clear( sources[i], contexts[i].color.r, contexts[i].color.g, contexts[i].color.b, contexts[i].color.a );
          sources[i]->Flip( sources[i], NULL, DSFLIP_NONE );

          threads[i] = direct_thread_create( DTT_DEFAULT,
                                             m_render ? blitting_thread_render : blitting_thread, &contexts[i], "Blit" );
     }

     while (true) {
          if (m_render) {
               for (i=0; i<m_num; i++) {
                    primary->Blit( primary, sources[i], NULL, 100 + 100 * (i % ((width - 250) / 100)), 100 + 100 * (i / ((width - 250) / 100)) );
               }

               primary->Flip( primary, NULL, DSFLIP_NONE );

               //sleep(1);
          }
          else
               sleep(1);
     }

out:
     if (sources) {
          for (i=0; i<m_num; i++) {
               if (sources[i])
                    sources[i]->Release( sources[i] );
          }

          D_FREE( sources );
     }

     if (destinations) {
          for (i=0; i<m_num; i++) {
               if (destinations[i])
                    destinations[i]->Release( destinations[i] );
          }

          D_FREE( destinations );
     }

     if (contexts)
          D_FREE( contexts );

     if (threads)
          D_FREE( threads );

     if (primary)
          primary->Release( primary );

     if (events)
          events->Release( events );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

