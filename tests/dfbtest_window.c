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

static DFBWindowDescription m_desc_top = {
     .flags         = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT,
     .posx          = 100,
     .posy          = 100,
     .width         = 200,
     .height        = 200,
     .pixelformat   = DSPF_UNKNOWN,
};

static DFBWindowDescription m_desc_sub = {
     .flags         = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT | DWDESC_TOPLEVEL_ID,
     .posx          = 40,
     .posy          = 40,
     .width         = 120,
     .height        = 120,
     .pixelformat   = DSPF_UNKNOWN,
};

static IDirectFBWindow *m_toplevel    = NULL;
static DFBWindowID      m_toplevel_id = 0;

static IDirectFBWindow *m_subwindow   = NULL;

/**********************************************************************************************************************/

typedef DFBResult (*TestFunc)( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult Test_CreateWindow( IDirectFBDisplayLayer *layer );
static DFBResult Test_CreateSubWindow( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult Test_MoveWindow( IDirectFBDisplayLayer *layer );
static DFBResult Test_ScaleWindow( IDirectFBDisplayLayer *layer );

/**********************************************************************************************************************/

static DFBResult Test_DestroyWindow( IDirectFBDisplayLayer *layer );

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

     if (!m_toplevel_id)
          RUN_TEST( Test_CreateWindow, layer );

     RUN_TEST( Test_CreateSubWindow, layer );

     RUN_TEST( Test_MoveWindow, layer );

     RUN_TEST( Test_ScaleWindow, layer );


     RUN_TEST( Test_DestroyWindow, layer );

     D_INFO( "Tests/Window: Shutting down...\n" );


     /* Release the sub window. */
     if (m_subwindow)
          m_subwindow->Release( m_subwindow );

     /* Release the top level. */
     if (m_toplevel)
          m_toplevel->Release( m_toplevel );

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

     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Window Test (version %s) ==\n", DIRECTFB_VERSION);
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

     fprintf (stderr, "Usage: %s [options]\n", prg_name);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                     Show this help message\n");
     fprintf (stderr, "  -v, --version                  Print version information\n");
     fprintf (stderr, "\n");
     fprintf (stderr, "Top window:\n");
     fprintf (stderr, "  -p, --pos        <posx>,<posy>     Position (%d,%d)\n", m_desc_top.posx, m_desc_top.posy);
     fprintf (stderr, "  -s, --size       <width>x<height>  Size     (%dx%d)\n", m_desc_top.width, m_desc_top.height);
     fprintf (stderr, "  -f, --format     <pixelformat>     Format   (%s)\n",    dfb_pixelformat_name(m_desc_top.pixelformat));
     fprintf (stderr, "\n");
     fprintf (stderr, "Sub window:\n");
     fprintf (stderr, "  -P, --sub-pos    <posx>,<posy>     Position (%d,%d)\n", m_desc_sub.posx, m_desc_sub.posy);
     fprintf (stderr, "  -S, --sub-size   <width>x<height>  Size     (%dx%d)\n", m_desc_sub.width, m_desc_sub.height);
     fprintf (stderr, "  -F, --sub-format <pixelformat>     Format   (%s)\n",    dfb_pixelformat_name(m_desc_sub.pixelformat));
     fprintf (stderr, "  -T, --top-level  <toplevel_id>     WindowID (skips top creation)\n");
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_position( const char *arg, int *_x, int *_y )
{
     if (sscanf( arg, "%d,%d", _x, _y ) != 2) {
          fprintf (stderr, "\nInvalid position specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_size( const char *arg, int *_w, int *_h )
{
     if (sscanf( arg, "%dx%d", _w, _h ) != 2 || *_w < 1 || *_h < 1) {
          fprintf (stderr, "\nInvalid size specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_format( const char *arg, DFBSurfacePixelFormat *_f )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               *_f = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static DFBBoolean
parse_id( const char *arg, unsigned int *_id )
{
     if (sscanf( arg, "%u", _id ) != 1) {
          fprintf (stderr, "\nInvalid ID specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
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

          if (strcmp (arg, "-p") == 0 || strcmp (arg, "--pos") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_position( argv[n], &m_desc_top.posx, &m_desc_top.posy ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-s") == 0 || strcmp (arg, "--size") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_size( argv[n], &m_desc_top.width, &m_desc_top.height ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[n], &m_desc_top.pixelformat ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-P") == 0 || strcmp (arg, "--sub-pos") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_position( argv[n], &m_desc_sub.posx, &m_desc_sub.posy ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-S") == 0 || strcmp (arg, "--sub-size") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_size( argv[n], &m_desc_sub.width, &m_desc_sub.height ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-F") == 0 || strcmp (arg, "--sub-format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[n], &m_desc_sub.pixelformat ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-T") == 0 || strcmp (arg, "--top-level") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_id( argv[n], &m_toplevel_id ))
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
     IDirectFBSurface *surface;
     IDirectFBWindow  *window;
     DFBWindowID       window_id;
     DFBDimension      size = { m_desc_top.width, m_desc_top.height };

     D_ASSERT( m_toplevel_id == 0 );

     /*
      * Create a new top level window
      */
     D_INFO( "Tests/Window:   -> CreateWindow( %d,%d - %dx%d %s )...\n",
             m_desc_top.posx, m_desc_top.posy, m_desc_top.width, m_desc_top.height,
             dfb_pixelformat_name( m_desc_top.pixelformat ) );

     _T( layer->CreateWindow( layer, &m_desc_top, &window ) );

     /*
      * Query its surface and clear it with light blue
      */
     D_INFO( "Tests/Window:   -> GetSurface()...\n" );

     _T( window->GetSurface( window, &surface ) );

     D_INFO( "Tests/Window:   -> Clear( 0x20, 0x50, 0xC0, 0xFF )...\n" );

     _T( surface->Clear( surface, 0x20, 0x50, 0xC0, 0xFF ) );

     _T( surface->SetColor( surface, 0x90, 0xF0, 0xC0, 0xFF ) );

     _T( surface->DrawRectangle( surface, 0, 0, size.w, size.h ) );

     _T( surface->FillRectangle( surface, size.w / 2,          1,          1, size.h - 2 ) );
     _T( surface->FillRectangle( surface,          1, size.h / 2, size.w - 2,          1 ) );

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
      * Set top level window ID (user hasn't specified one)
      */
     m_toplevel_id = window_id;
     m_toplevel    = window;

     LookAtResult();

     surface->Release( surface );

     return DFB_OK;
}

static DFBResult
Test_CreateSubWindow( IDirectFBDisplayLayer *layer )
{
     IDirectFBSurface     *surface;
     IDirectFBWindow      *window;
     DFBWindowID           window_id;
     DFBDimension          size = { m_desc_sub.width, m_desc_sub.height };

     D_ASSERT( m_toplevel_id != 0 );

     /* Write window ID of top level into description */
     m_desc_sub.toplevel_id = m_toplevel_id;

     /*
      * Create a new sub window with 75% width/height and positioned at 20,20 within top level window
      */
     D_INFO( "Tests/Window:   -> CreateWindow( %d,%d - %dx%d %s + toplevel ID %u )...\n",
             m_desc_sub.posx, m_desc_sub.posy, m_desc_sub.width, m_desc_sub.height,
             dfb_pixelformat_name( m_desc_sub.pixelformat ), m_desc_sub.toplevel_id );

     _T( layer->CreateWindow( layer, &m_desc_sub, &window ) );

     /*
      * Query its surface and clear it with light gray
      */
     D_INFO( "Tests/Window:   -> GetSurface()...\n" );

     _T( window->GetSurface( window, &surface ) );

     D_INFO( "Tests/Window:   -> Clear( 0xC0, 0xC0, 0xC0, 0xFF )...\n" );

     _T( surface->Clear( surface, 0xC0, 0xC0, 0xC0, 0xFF ) );

     _T( surface->DrawRectangle( surface, 0, 0, size.w, size.h ) );

     _T( surface->FillRectangle( surface, size.w / 2,          1,          1, size.h - 2 ) );
     _T( surface->FillRectangle( surface,          1, size.h / 2, size.w - 2,          1 ) );

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

     m_subwindow = window;

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

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     D_INFO( "Tests/Window:   -> GetWindow( %u )...\n", m_toplevel_id );

     _T( layer->GetWindow( layer, m_toplevel_id, &window ) );

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
     DFBDimension     size    = { m_desc_top.width, m_desc_top.height };
     DFBDimension     sizes[] = { { size.w + 40, size.h      },
                                  { size.w + 40, size.h + 40 },
                                  { size.w,      size.h + 40 },
                                  { size.w + 40, size.h - 40 },
                                  { size.w - 40, size.h + 40 },
                                  { size.w,      size.h      } };

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     D_INFO( "Tests/Window:   -> GetWindow( %u )...\n", m_toplevel_id );

     _T( layer->GetWindow( layer, m_toplevel_id, &window ) );

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
     for (i=0; i<D_ARRAY_SIZE(sizes); i++) {
          D_INFO( "Tests/Window:   -> Resize( %4d,%4d - [%02d] )...\n", sizes[i].w, sizes[i].h, i );

          _T( window->Resize( window, sizes[i].w, sizes[i].h ) );

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

static DFBResult
Test_DestroyWindow( IDirectFBDisplayLayer *layer )
{
     IDirectFBWindow *window;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     D_INFO( "Tests/Window:   -> GetWindow( %u )...\n", m_toplevel_id );

     _T( layer->GetWindow( layer, m_toplevel_id, &window ) );

     /*
      * Destroy it
      */
     D_INFO( "Tests/Window:   -> Destroy()...\n" );

     _T( window->Destroy( window ) );

     LookAtResult();

     window->Release( window );

     return DFB_OK;
}

