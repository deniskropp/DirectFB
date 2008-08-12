/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>

static const DirectFBPixelFormatNames( format_names );

/**********************************************************************************************************************/

static int                   m_width    = 200;
static int                   m_height   = 200;
static DFBSurfacePixelFormat m_format   = DSPF_UNKNOWN;
static DFBWindowID           m_toplevel = 0;

/**********************************************************************************************************************/

typedef DFBResult (*TestFunc)( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult Test_CreateWindow( IDirectFBDisplayLayer *layer );
static DFBResult Test_CreateSubWindow( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult Test_MoveWindow( IDirectFBDisplayLayer *layer );
static DFBResult Test_ScaleWindow( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult RunTest( TestFunc func, const char *func_name, IDirectFBDisplayLayer *layer );

#define RUN_TEST(func,layer)  RunTest( func, #func, layer )

/**********************************************************************************************************************/

static void LookAtResult( void );

/**********************************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     IDirectFB             *dfb;
     IDirectFBDisplayLayer *layer;

     D_INFO( "Tests/Window: Starting up...\n" );

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return -3;
     }

     /* Get the primary layer interface. */
     ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
     if (ret) {
          D_DERROR( ret, "IDirectFB::GetDisplayLayer() failed!\n" );
          dfb->Release( dfb );
          return -4;
     }


     D_INFO( "Tests/Window: Got layer interface, running tests...\n" );

     RUN_TEST( Test_CreateWindow, layer );

     RUN_TEST( Test_CreateSubWindow, layer );

     RUN_TEST( Test_MoveWindow, layer );

     RUN_TEST( Test_ScaleWindow, layer );


     D_INFO( "Tests/Window: Shutting down...\n" );

     /* Release the layer. */
     layer->Release( layer );

     /* Release the super interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     int i = 0;

     fprintf (stderr, "\nDirectFB Window Test (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -s, --size    <width>x<height>  Set window size (default 200x200)\n");
     fprintf (stderr, "   -f, --format  <pixelformat>     Set the pixel format\n");
     fprintf (stderr, "   -h, --help                      Show this help message\n");
     fprintf (stderr, "   -v, --version                   Print version information\n");
     fprintf (stderr, "\n");

     fprintf (stderr, "Known pixel formats:\n");

     while (format_names[i].format != DSPF_UNKNOWN) {
          DFBSurfacePixelFormat format = format_names[i].format;

          fprintf (stderr, "   %-10s %2d bits, %d bytes",
                   format_names[i].name, DFB_BITS_PER_PIXEL(format),
                   DFB_BYTES_PER_PIXEL(format));

          if (DFB_PIXELFORMAT_HAS_ALPHA(format))
               fprintf (stderr, "   ALPHA");

          if (DFB_PIXELFORMAT_IS_INDEXED(format))
               fprintf (stderr, "   INDEXED");

          if (DFB_PLANAR_PIXELFORMAT(format)) {
               int planes = DFB_PLANE_MULTIPLY(format, 1000);

               fprintf (stderr, "   PLANAR (x%d.%03d)",
                        planes / 1000, planes % 1000);
          }

          fprintf (stderr, "\n");

          ++i;
     }
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_size( const char *arg )
{
     if (sscanf( arg, "%dx%d", &m_width, &m_height ) != 2 || m_width < 1 || m_height < 1) {
          fprintf (stderr, "\nInvalid size specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_format( const char *arg )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               m_format = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static bool
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return false;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return false;
          }

          if (strcmp (arg, "-s") == 0 || strcmp (arg, "--size") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_size( argv[n] ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[n] ))
                    return false;

               continue;
          }

          print_usage (argv[0]);

          return false;
     }

     return true;
}

/**********************************************************************************************************************/

static DFBResult
RunTest( TestFunc               func,
         const char            *func_name,
         IDirectFBDisplayLayer *layer )
{
     DFBResult ret;

     D_INFO( "Tests/Window: Running %s()...\n", func_name );

     /* Run the actual test... */
     ret = func( layer );
     if (ret)
          D_DERROR( ret, "RunTest: '%s' failed!\n", func_name );

     return ret;
}

/**********************************************************************************************************************/

static void
LookAtResult()
{
     sleep( 3 );
}

/**********************************************************************************************************************/

#define _T(x...)  \
     do {                                                                       \
          DFBResult ret = x;                                                    \
                                                                                \
          if (ret) {                                                            \
               D_DERROR( ret, "Tests/Window: '%s' failed!\n", #x );          \
               return ret;                                                      \
          }                                                                     \
     } while (0)

/**********************************************************************************************************************/

static DFBResult
Test_CreateWindow( IDirectFBDisplayLayer *layer )
{
     DFBWindowDescription  desc;
     IDirectFBSurface     *surface;
     IDirectFBWindow      *window;
     DFBWindowID           window_id;

     /*
      * Create a new top level window
      */
     desc.flags       = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT;
     desc.posx        = 100;
     desc.posy        = 100;
     desc.width       = m_width;
     desc.height      = m_height;
     desc.pixelformat = m_format;

     D_INFO( "Tests/Window:   -> CreateWindow( %d,%d - %dx%d %s )...\n",
             desc.posx, desc.posy, desc.width, desc.height, dfb_pixelformat_name( desc.pixelformat ) );

     _T( layer->CreateWindow( layer, &desc, &window ) );

     /*
      * Query its surface and clear it with light blue
      */
     D_INFO( "Tests/Window:   -> GetSurface()...\n" );

     _T( window->GetSurface( window, &surface ) );

     D_INFO( "Tests/Window:   -> Clear( 0x20, 0x50, 0xC0, 0xFF )...\n" );

     _T( surface->Clear( surface, 0x20, 0x50, 0xC0, 0xFF ) );

     /*
      * Show the window
      */
     D_INFO( "Tests/Window:   -> SetOpacity( 255 )...\n" );

     _T( window->SetOpacity( window, 0xff ) );

     /*
      * Query and print ID of new window
      */
     _T( window->GetID( window, &window_id ) );

     D_INFO( "Tests/Window:   => ID %u\n", window_id );

     /*
      * Set top level window ID if user hasn't specified
      */
     if (!m_toplevel)
          m_toplevel = window_id;

     LookAtResult();

     surface->Release( surface );

     return DFB_OK;
}

static DFBResult
Test_CreateSubWindow( IDirectFBDisplayLayer *layer )
{
     DFBWindowDescription  desc;
     IDirectFBSurface     *surface;
     IDirectFBWindow      *window;
     DFBWindowID           window_id;

     /*
      * Create a new sub window with 75% width/height and positioned at 20,20 within top level window
      */
     desc.flags       = DWDESC_CAPS | DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                        DWDESC_PIXELFORMAT | DWDESC_TOPLEVEL_ID;
     desc.caps        = DWCAPS_SUBWINDOW;
     desc.posx        = 20;
     desc.posy        = 20;
     desc.width       = m_width  * 3 / 4;
     desc.height      = m_height * 3 / 4;
     desc.pixelformat = m_format;
     desc.toplevel_id = m_toplevel;

     D_INFO( "Tests/Window:   -> CreateWindow( %d,%d - %dx%d %s + toplevel ID %u )...\n", desc.posx, desc.posy,
             desc.width, desc.height, dfb_pixelformat_name( desc.pixelformat ), desc.toplevel_id );

     _T( layer->CreateWindow( layer, &desc, &window ) );

     /*
      * Query its surface and clear it with light gray
      */
     D_INFO( "Tests/Window:   -> GetSurface()...\n" );

     _T( window->GetSurface( window, &surface ) );

     D_INFO( "Tests/Window:   -> Clear( 0xC0, 0xC0, 0xC0, 0xFF )...\n" );

     _T( surface->Clear( surface, 0xC0, 0xC0, 0xC0, 0xFF ) );

     /*
      * Show the window
      */
     D_INFO( "Tests/Window:   -> SetOpacity( 255 )...\n" );

     _T( window->SetOpacity( window, 0xff ) );

     /*
      * Query and print ID of new window
      */
     _T( window->GetID( window, &window_id ) );

     D_INFO( "Tests/Window:   => ID %u\n", window_id );

     LookAtResult();

     surface->Release( surface );

     return DFB_OK;
}

static DFBResult
Test_MoveWindow( IDirectFBDisplayLayer *layer )
{
     int              i;
     IDirectFBWindow *window;
     DFBPoint         pos[] = { {  60,  60 },
                                { 140,  60 },
                                { 140, 140 },
                                {  60, 140 },
                                { 100, 100 } };

     /*
      * Get the top level window
      */
     D_INFO( "Tests/Window:   -> GetWindow( %u )...\n", m_toplevel );

     _T( layer->GetWindow( layer, m_toplevel, &window ) );

     /*
      * Move the window
      */
     for (i=0; i<D_ARRAY_SIZE(pos); i++) {
          D_INFO( "Tests/Window:   -> MoveTo( %4d,%4d - [%02d] )...\n", pos[i].x, pos[i].y, i );

          _T( window->MoveTo( window, pos[i].x, pos[i].y ) );

          LookAtResult();
     }

     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_ScaleWindow( IDirectFBDisplayLayer *layer )
{
     int              i;
     IDirectFBWindow *window;
     DFBWindowOptions opts;
     DFBDimension     size[] = { { m_width + 40, m_height      },
                                 { m_width + 40, m_height + 40 },
                                 { m_width,      m_height + 40 },
                                 { m_width + 40, m_height - 40 },
                                 { m_width - 40, m_height + 40 },
                                 { m_width,      m_height      } };

     /*
      * Get the top level window
      */
     D_INFO( "Tests/Window:   -> GetWindow( %u )...\n", m_toplevel );

     _T( layer->GetWindow( layer, m_toplevel, &window ) );

     /*
      * Enable scaling
      */
     D_INFO( "Tests/Window:   -> GetOptions()...\n" );

     _T( window->GetOptions( window, &opts ) );

     D_INFO( "Tests/Window:   -> SetOptions( 0x%08x )... <- DWOP_SCALE\n", opts | DWOP_SCALE );

     _T( window->SetOptions( window, opts | DWOP_SCALE ) );

     /*
      * Move the window
      */
     for (i=0; i<D_ARRAY_SIZE(size); i++) {
          D_INFO( "Tests/Window:   -> Resize( %4d,%4d - [%02d] )...\n", size[i].w, size[i].h, i );

          _T( window->Resize( window, size[i].w, size[i].h ) );

          LookAtResult();
     }

     /*
      * Restore options
      */
     D_INFO( "Tests/Window:   -> SetOptions( 0x%08x )... <- (restore)\n", opts );

     _T( window->SetOptions( window, opts ) );

     window->Release( window );

     return DFB_OK;
}

