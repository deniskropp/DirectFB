/*
   (c) Copyright 2001-2009  The DirectFB Organization (directfb.org)
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

#include <stdarg.h>
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
static const DirectFBWindowCapabilitiesNames( caps_names );
static const DirectFBWindowOptionsNames( options_names );

/**********************************************************************************************************************/

static DFBWindowDescription m_desc_top = {
     .flags         = DWDESC_CAPS | DWDESC_POSX | DWDESC_POSY |
                      DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT | DWDESC_OPTIONS,
     .posx          = 100,
     .posy          = 100,
     .width         = 200,
     .height        = 200,
};

static DFBWindowDescription m_desc_sub = {
     .flags         = DWDESC_CAPS | DWDESC_POSX | DWDESC_POSY |
                      DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT | DWDESC_OPTIONS | DWDESC_TOPLEVEL_ID,
     .posx          = 40,
     .posy          = 40,
     .width         = 120,
     .height        = 120,
};

static DFBColor         m_topcolor;
static DFBColor         m_subcolor;

static IDirectFBWindow *m_toplevel     = NULL;
static DFBWindowID      m_toplevel_id  = 0;

static IDirectFBWindow *m_subwindow    = NULL;
static DFBWindowID      m_subwindow_id = 0;

static DFBBoolean       m_wait_at_end  = DFB_FALSE;

/**********************************************************************************************************************/

typedef DFBResult (*TestFunc)( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static DFBResult Test_CreateWindow( IDirectFBDisplayLayer *layer, void *arg );
static DFBResult Test_CreateSubWindow( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static DFBResult Test_MoveWindow( IDirectFBDisplayLayer *layer, void *arg );
static DFBResult Test_ScaleWindow( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static DFBResult Test_RestackWindow( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static DFBResult Test_SrcGeometry( IDirectFBDisplayLayer *layer, void *arg );
static DFBResult Test_DstGeometry( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static DFBResult Test_HideWindow( IDirectFBDisplayLayer *layer, void *arg );
static DFBResult Test_DestroyWindow( IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

typedef struct {
     const char *name;
     TestFunc    func;

     bool        run_top;
     bool        run_sub;
} Test;

static Test m_tests[] = {
     { "Restack",        Test_RestackWindow },
     { "Move",           Test_MoveWindow },
     { "Scale",          Test_ScaleWindow },
     { "SrcGeometry",    Test_SrcGeometry },
     { "DstGeometry",    Test_DstGeometry },
     { "Hide",           Test_HideWindow },
     { "Destroy",        Test_DestroyWindow },
};

/**********************************************************************************************************************/

static DFBResult RunTest( TestFunc func, const char *func_name, IDirectFBDisplayLayer *layer, void *arg );

/**********************************************************************************************************************/

static void ShowMessage( unsigned int ms, const char *name,
                         const char *prefix, const char *format, ... ) D_FORMAT_PRINTF(4);

#define SHOW_TEST(msg...)    ShowMessage( 2000, __FUNCTION__, \
                                          "===============================================================\n\n", msg )
#define SHOW_INFO(msg...)    ShowMessage(  500, __FUNCTION__, "", msg )
#define SHOW_RESULT(msg...)  ShowMessage( 3000, __FUNCTION__, "", msg )

/**********************************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     int                    i;
     IDirectFB             *dfb;
     IDirectFBDisplayLayer *layer;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     SHOW_INFO( "Starting up..." );

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


     if (!m_toplevel_id)
          RunTest( Test_CreateWindow, "CreateWindow", layer, NULL );

     RunTest( Test_CreateSubWindow, "CreateSubWindow", layer, NULL );


     for (i=0; i<D_ARRAY_SIZE(m_tests); i++) {
          if (m_tests[i].run_top)
               RunTest( m_tests[i].func, m_tests[i].name, layer, NULL );

          if (m_tests[i].run_sub)
               RunTest( m_tests[i].func, m_tests[i].name, layer, (void*) (unsigned long) m_subwindow_id );
     }

     if (m_wait_at_end) {
          sigset_t block;

          sigemptyset( &block );

          sigsuspend( &block );
     }

     SHOW_INFO( "Shutting down..." );

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
     fprintf (stderr, "Known window capabilities:\n");

     for (i=0; caps_names[i].capability != DWCAPS_NONE; i++)
          fprintf (stderr, "   %s\n", caps_names[i].name);

     fprintf (stderr, "\n");
     fprintf (stderr, "Known window options:\n");

     for (i=0; options_names[i].option != DWOP_NONE; i++)
          fprintf (stderr, "   %s\n", options_names[i].name);

     fprintf (stderr, "\n");

     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg_name);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                            Show this help message\n");
     fprintf (stderr, "  -v, --version                         Print version information\n");
     fprintf (stderr, "  -T, --top-level     <toplevel_id>     WindowID (skips top creation)\n");
     fprintf (stderr, "  -W, --wait-at-end                     Wait at the end (don't exit)\n");
     fprintf (stderr, "\n");
     fprintf (stderr, "Top window:\n");
     fprintf (stderr, "  -r, --run           <test>            Run test (see list below)\n");
     fprintf (stderr, "  -p, --pos           <posx>,<posy>     Position     (%d,%d)\n", m_desc_top.posx, m_desc_top.posy);
     fprintf (stderr, "  -s, --size          <width>x<height>  Size         (%dx%d)\n", m_desc_top.width, m_desc_top.height);
     fprintf (stderr, "  -f, --format        <pixelformat>     Pixel Format (%s)\n",    dfb_pixelformat_name(m_desc_top.pixelformat));
     fprintf (stderr, "  -c, --caps          <window_caps>     Capabilities (NONE)\n");
     fprintf (stderr, "  -l, --color         <aarrggbb>        Fixed Color  (NONE)\n");
     fprintf (stderr, "  -o, --option        <window_option>   Options      (NONE)\n");
     fprintf (stderr, "  -a, --associate     <parent_id>       Association  (N/A)\n");
     fprintf (stderr, "\n");
     fprintf (stderr, "Sub window:\n");
     fprintf (stderr, "  -R, --sub-run       <test>            Run test (see list below)\n");
     fprintf (stderr, "  -P, --sub-pos       <posx>,<posy>     Position     (%d,%d)\n", m_desc_sub.posx, m_desc_sub.posy);
     fprintf (stderr, "  -S, --sub-size      <width>x<height>  Size         (%dx%d)\n", m_desc_sub.width, m_desc_sub.height);
     fprintf (stderr, "  -F, --sub-format    <pixelformat>     Format       (%s)\n",    dfb_pixelformat_name(m_desc_sub.pixelformat));
     fprintf (stderr, "  -C, --sub-caps      <window_caps>     Capabilities (NONE)\n");
     fprintf (stderr, "  -L, --sub-color     <aarrggbb>        Fixed Color  (NONE)\n");
     fprintf (stderr, "  -O, --sub-option    <window_option>   Options      (NONE)\n");
     fprintf (stderr, "  -A, --sub-associate <parent_id>       Association  (N/A)\n");
     fprintf (stderr, "\n");

     fprintf (stderr, "Available tests:\n");

     for (i=0; i<D_ARRAY_SIZE(m_tests); i++)
          fprintf (stderr, "   %s\n", m_tests[i].name);

     fprintf (stderr, "\n");
}

static DFBBoolean
parse_test( const char *arg, bool sub )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(m_tests); i++) {
          if (!strncasecmp( arg, m_tests[i].name, strlen(arg) )) {
               if (sub)
                    m_tests[i].run_sub = true;
               else
                    m_tests[i].run_top = true;

               return DFB_TRUE;
          }
     }

     fprintf (stderr, "\nInvalid test specified!\n\n" );

     return DFB_FALSE;
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
parse_caps( const char *arg, DFBWindowCapabilities *_c )
{
     int i = 0;

     while (caps_names[i].capability != DWCAPS_NONE) {
          if (!strncasecmp( arg, caps_names[i].name, strlen(arg) )) {
               *_c |= caps_names[i].capability;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid caps specified!\n\n" );

     return DFB_FALSE;
}

static DFBBoolean
parse_color( const char *arg, DFBColor *_c )
{
     long int  l   = 0;
     char     *end = 0;
     DFBColor  c;

     l = strtol( arg, &end, 16 );

     if( strlen(arg)>8 || (end && *end!=0) ) {
          fprintf (stderr, "\nInvalid color specified!\n\n" );
          return DFB_FALSE;
     }

     c.a = (l >> 24)       ;
     c.r = (l >> 16) & 0xff;
     c.g = (l >>  8) & 0xff;
     c.b = (l      ) & 0xff;
     
     *_c = c;

     return DFB_TRUE;
}

static DFBBoolean
parse_option( const char *arg, DFBWindowOptions *_o )
{
     int i = 0;

     while (options_names[i].option != DWOP_NONE) {
          if (!strncasecmp( arg, options_names[i].name, strlen(arg) )) {
               *_o |= options_names[i].option;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid options specified!\n\n" );

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

          if (strcmp (arg, "-T") == 0 || strcmp (arg, "--top-level") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_id( argv[n], &m_toplevel_id ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-W") == 0 || strcmp (arg, "--wait-at-end") == 0) {
               m_wait_at_end = DFB_TRUE;
               continue;
          }

          if (strcmp (arg, "-r") == 0 || strcmp (arg, "--run") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_test( argv[n], false ))
                    return false;

               continue;
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

          if (strcmp (arg, "-c") == 0 || strcmp (arg, "--caps") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_caps( argv[n], &m_desc_top.caps ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-l") == 0 || strcmp (arg, "--color") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_color( argv[n], &m_topcolor ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-o") == 0 || strcmp (arg, "--option") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_option( argv[n], &m_desc_top.options ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-a") == 0 || strcmp (arg, "--associate") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_id( argv[n], &m_desc_top.parent_id ))
                    return false;

//               m_desc_top.flags   |= DWDESC_PARENT;
               m_desc_top.options |= DWOP_FOLLOW_BOUNDS;

               continue;
          }

          if (strcmp (arg, "-R") == 0 || strcmp (arg, "--sub-run") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_test( argv[n], true ))
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

          if (strcmp (arg, "-C") == 0 || strcmp (arg, "--sub-caps") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_caps( argv[n], &m_desc_sub.caps ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-L") == 0 || strcmp (arg, "--sub-color") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_color( argv[n], &m_subcolor ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-O") == 0 || strcmp (arg, "--sub-option") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_option( argv[n], &m_desc_sub.options ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-A") == 0 || strcmp (arg, "--sub-associate") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_id( argv[n], &m_desc_sub.parent_id ))
                    return false;

//               m_desc_sub.flags   |= DWDESC_PARENT;
               m_desc_sub.options |= DWOP_FOLLOW_BOUNDS;

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
         const char            *test_name,
         IDirectFBDisplayLayer *layer,
         void                  *arg )
{
     DFBResult ret;

     /* Run the actual test... */
     ret = func( layer, arg );
     if (ret)
          D_DERROR( ret, "RunTest: '%s' failed!\n", test_name );

     return ret;
}

/**********************************************************************************************************************/

static void
ShowMessage( unsigned int ms, const char *name, const char *prefix, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "%s [[ %-30s ]]  %s\n", prefix, name, buf );

     usleep( ms * 1000 );
}

/**********************************************************************************************************************/

#define _T(x...)  \
     do {                                                                       \
          DFBResult ret = x;                                                    \
                                                                                \
          if (ret) {                                                            \
               D_DERROR( ret, "Tests/Window: '%s' failed!\n", #x );             \
               return ret;                                                      \
          }                                                                     \
     } while (0)

/**********************************************************************************************************************/

static DFBResult
Test_CreateWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     IDirectFBSurface *surface = NULL;
     IDirectFBWindow  *window;
     DFBWindowID       window_id;
     DFBDimension      size = { m_desc_top.width, m_desc_top.height };

     D_ASSERT( m_toplevel_id == 0 );

     /*
      * Create a new top level window
      */
     SHOW_TEST( "CreateWindow( %d,%d - %dx%d %s, options 0x%08x )...",
                m_desc_top.posx, m_desc_top.posy, m_desc_top.width, m_desc_top.height,
                dfb_pixelformat_name( m_desc_top.pixelformat ), m_desc_top.options );

     _T( layer->CreateWindow( layer, &m_desc_top, &window ) );

     if (m_desc_top.caps & DWCAPS_COLOR) {
          DFBColor c = m_topcolor;

          SHOW_INFO( "  - SetColor( 0x%02x, 0x%02x, 0x%02x, 0x%02x )...", c.r, c.g, c.b, c.a );

          _T( window->SetColor( window, c.r, c.g, c.b, c.a ) );
     }

     /*
      * Query its surface and clear it with light blue (if not input or color only)
      */
     if (!(m_desc_top.caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR) )) {
          SHOW_INFO( "  - GetSurface()..." );

          _T( window->GetSurface( window, &surface ) );

          SHOW_INFO( "  - Clear( 0x20, 0x50, 0xC0, 0xFF )..." );

          _T( surface->Clear( surface, 0x20, 0x50, 0xC0, 0xFF ) );

          _T( surface->SetColor( surface, 0x90, 0xF0, 0xC0, 0xFF ) );

          _T( surface->DrawRectangle( surface, 0, 0, size.w, size.h ) );

          _T( surface->FillRectangle( surface, size.w / 2,          1,          1, size.h - 2 ) );
          _T( surface->FillRectangle( surface,          1, size.h / 2, size.w - 2,          1 ) );
     }

     /*
      * Show the window
      */
     SHOW_INFO( "  - SetOpacity( 255 )..." );

     _T( window->SetOpacity( window, 0xff ) );

     /*
      * Query and print ID of new window
      */
     SHOW_INFO( "  - GetID()..." );

     _T( window->GetID( window, &window_id ) );

     /*
      * Set association of new window
      */
     if (m_desc_top.parent_id) {
          SHOW_INFO( "  - SetAssociation( %u )...", m_desc_top.parent_id );

          _T( window->SetAssociation( window, m_desc_top.parent_id ) );
     }

     /*
      * Set top level window ID (user hasn't specified one)
      */
     m_toplevel_id = window_id;
     m_toplevel    = window;

     SHOW_RESULT( "...CreateWindow( %d,%d - %dx%d %s ) done. => Top Window ID %u",
                  m_desc_top.posx, m_desc_top.posy, m_desc_top.width, m_desc_top.height,
                  dfb_pixelformat_name( m_desc_top.pixelformat ), window_id );

     if (surface)
          surface->Release( surface );

     return DFB_OK;
}

static DFBResult
Test_CreateSubWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     IDirectFBWindow      *window;
     DFBWindowID           window_id;
     DFBDimension          size = { m_desc_sub.width, m_desc_sub.height };

     D_ASSERT( m_toplevel_id != 0 );

     /* Write window ID of top level into description */
     m_desc_sub.toplevel_id = m_toplevel_id;

     /*
      * Create a new sub window with 75% width/height and positioned at 20,20 within top level window
      */
     SHOW_TEST( "CreateWindow( %d,%d - %dx%d %s + toplevel ID %u, options 0x%08x )...",
                m_desc_sub.posx, m_desc_sub.posy, m_desc_sub.width, m_desc_sub.height,
                dfb_pixelformat_name( m_desc_sub.pixelformat ), m_desc_sub.toplevel_id, m_desc_top.options );

     _T( layer->CreateWindow( layer, &m_desc_sub, &window ) );

     if (m_desc_sub.caps & DWCAPS_COLOR) {
          DFBColor c = m_subcolor;

          SHOW_INFO( "  - SetColor( 0x%02x, 0x%02x, 0x%02x, 0x%02x )...", c.r, c.g, c.b, c.a );

          _T( window->SetColor( window, c.r, c.g, c.b, c.a ) );
     }

     /*
      * Query its surface and clear it with light gray (if not input or color only)
      */
     if (!(m_desc_sub.caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR) )) {
          IDirectFBSurface     *surface;

          SHOW_INFO( "  - GetSurface()..." );

          _T( window->GetSurface( window, &surface ) );

          SHOW_INFO( "  - Clear( 0xC0, 0xC0, 0xC0, 0xFF )..." );

          _T( surface->Clear( surface, 0xC0, 0xC0, 0xC0, 0xFF ) );

          _T( surface->DrawRectangle( surface, 0, 0, size.w, size.h ) );

          _T( surface->FillRectangle( surface, size.w / 2,          1,          1, size.h - 2 ) );
          _T( surface->FillRectangle( surface,          1, size.h / 2, size.w - 2,          1 ) );
          
          surface->Release( surface );
     }

     /*
      * Show the window
      */
     SHOW_INFO( "  - SetOpacity( 255 )..." );

     _T( window->SetOpacity( window, 0xff ) );

     /*
      * Query and print ID of new window
      */
     SHOW_INFO( "  - GetID()..." );

     _T( window->GetID( window, &window_id ) );

     /*
      * Set association of new window
      */
     if (m_desc_sub.parent_id) {
          SHOW_INFO( "  - SetAssociation( %u )...", m_desc_sub.parent_id );

          _T( window->SetAssociation( window, m_desc_sub.parent_id ) );
     }

     /*
      * Set top level window ID (user hasn't specified one)
      */
     m_subwindow_id = window_id;
     m_subwindow    = window;

     SHOW_RESULT( "...CreateWindow( %d,%d - %dx%d %s + toplevel ID %u ) done. => Sub Window ID %u",
                  m_desc_sub.posx, m_desc_sub.posy, m_desc_sub.width, m_desc_sub.height,
                  dfb_pixelformat_name( m_desc_sub.pixelformat ), m_desc_sub.toplevel_id, window_id );

     return DFB_OK;
}

static DFBResult
Test_MoveWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     int              i;
     DFBPoint         pos;
     IDirectFBWindow *window;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     window->GetPosition( window, &pos.x, &pos.y );

     /*
      * Move the window
      */
     {
          DFBPoint poss[] = { { pos.x - 40, pos.y - 40 },
                              { pos.x + 40, pos.y - 40 },
                              { pos.x + 40, pos.y + 40 },
                              { pos.x - 40, pos.y + 40 },
                              { pos.x     , pos.y      } };

          for (i=0; i<D_ARRAY_SIZE(poss); i++) {
               SHOW_TEST( "MoveTo( %4d,%4d - [%02d] )...", poss[i].x, poss[i].y, i );

               _T( window->MoveTo( window, poss[i].x, poss[i].y ) );

               SHOW_RESULT( "...MoveTo( %4d,%4d - [%02d] ) done.", poss[i].x, poss[i].y, i );
          }
     }

     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_ScaleWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     int              i;
     IDirectFBWindow *window;
     DFBWindowOptions opts;
     DFBDimension     size;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     window->GetSize( window, &size.w, &size.h );

     /*
      * Enable scaling
      */
     _T( window->GetOptions( window, &opts ) );
     _T( window->SetOptions( window, opts | DWOP_SCALE ) );

     /*
      * Scale the window
      */
     {
          DFBDimension sizes[] = { { size.w + 40, size.h      },
                                   { size.w + 40, size.h + 40 },
                                   { size.w,      size.h + 40 },
                                   { size.w + 40, size.h - 40 },
                                   { size.w - 40, size.h + 40 },
                                   { size.w,      size.h      } };

          for (i=0; i<D_ARRAY_SIZE(sizes); i++) {
               SHOW_TEST( "Resize( %4d,%4d - [%02d] )...", sizes[i].w, sizes[i].h, i );

               _T( window->Resize( window, sizes[i].w, sizes[i].h ) );

               SHOW_RESULT( "...Resize( %4d,%4d - [%02d] ) done.", sizes[i].w, sizes[i].h, i );
          }
     }

     /*
      * Restore options
      */
     _T( window->SetOptions( window, opts ) );

     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_RestackWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     int              i;
     IDirectFBWindow *window;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     /*
      * Lower it a few times
      */
     for (i=0; i<2; i++) {
          SHOW_TEST( "Lower() #%d...", i+1 );

          _T( window->Lower( window ) );

          SHOW_RESULT( "...Lower() #%d done.", i+1 );
     }

     /*
      * Raise it a few times
      */
     for (i=0; i<2; i++) {
          SHOW_TEST( "Raise() #%d...", i+1 );

          _T( window->Raise( window ) );

          SHOW_RESULT( "...Raise() #%d done.", i+1 );
     }

     /*
      * Lower it to the bottom
      */
     SHOW_TEST( "LowerToBottom()..." );

     _T( window->LowerToBottom( window ) );

     SHOW_RESULT( "...LowerToBottom() done." );

     /*
      * Raise it to the top
      */
     SHOW_TEST( "RaiseToTop()..." );

     _T( window->RaiseToTop( window ) );

     SHOW_RESULT( "...RaiseToTop() done." );


     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_SrcGeometry( IDirectFBDisplayLayer *layer, void *arg )
{
     int                i;
     IDirectFBWindow   *window;
     DFBWindowGeometry  geometry;
     DFBDimension       size;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     window->GetSize( window, &size.w, &size.h );

     /*
      * Change source geometry
      */
     {
          DFBRectangle rects[] = { {          0,          0, size.w / 2, size.h / 2 },
                                   { size.w / 2,          0, size.w / 2, size.h / 2 },
                                   { size.w / 2, size.h / 2, size.w / 2, size.h / 2 },
                                   {          0, size.h / 2, size.w / 2, size.h / 2 } };

          for (i=0; i<D_ARRAY_SIZE(rects); i++) {
               SHOW_TEST( "SetSrcGeometry( %4d,%4d-%4dx%4d - [%02d] )...", DFB_RECTANGLE_VALS(&rects[i]), i );

               geometry.mode      = DWGM_RECTANGLE;
               geometry.rectangle = rects[i];

               _T( window->SetSrcGeometry( window, &geometry ) );

               SHOW_RESULT( "...SetSrcGeometry( %4d,%4d-%4dx%4d - [%02d] ) done.", DFB_RECTANGLE_VALS(&rects[i]), i );
          }
     }


     SHOW_TEST( "SetSrcGeometry( DEFAULT )..." );

     geometry.mode = DWGM_DEFAULT;

     _T( window->SetSrcGeometry( window, &geometry ) );

     SHOW_RESULT( "...SetSrcGeometry( DEFAULT ) done." );


     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_DstGeometry( IDirectFBDisplayLayer *layer, void *arg )
{
     int                i;
     IDirectFBWindow   *window;
     DFBWindowGeometry  geometry;
     DFBDimension       size;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     window->GetSize( window, &size.w, &size.h );

     /*
      * Change destination geometry
      */
     {
          DFBRectangle rects[] = { {          0,          0, size.w / 2, size.h / 2 },
                                   { size.w / 2,          0, size.w / 2, size.h / 2 },
                                   { size.w / 2, size.h / 2, size.w / 2, size.h / 2 },
                                   {          0, size.h / 2, size.w / 2, size.h / 2 } };

          for (i=0; i<D_ARRAY_SIZE(rects); i++) {
               SHOW_TEST( "SetDstGeometry( %4d,%4d-%4dx%4d - [%02d] )...", DFB_RECTANGLE_VALS(&rects[i]), i );

               geometry.mode      = DWGM_RECTANGLE;
               geometry.rectangle = rects[i];

               _T( window->SetDstGeometry( window, &geometry ) );

               SHOW_RESULT( "...SetDstGeometry( %4d,%4d-%4dx%4d - [%02d] ) done.", DFB_RECTANGLE_VALS(&rects[i]), i );
          }
     }


     SHOW_TEST( "SetDstGeometry( DEFAULT )..." );

     geometry.mode = DWGM_DEFAULT;

     _T( window->SetDstGeometry( window, &geometry ) );

     SHOW_RESULT( "...SetDstGeometry( DEFAULT ) done." );


     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_HideWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     IDirectFBWindow *window;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     /*
      * Hide it
      */
     SHOW_TEST( "SetOpacity( 0 )..." );

     _T( window->SetOpacity( window, 0 ) );

     SHOW_RESULT( "...SetOpacity( 0 ) done." );

     /*
      * Show it again
      */
     SHOW_TEST( "SetOpacity( 0xff )..." );

     _T( window->SetOpacity( window, 0xff ) );

     SHOW_RESULT( "...SetOpacity( 0xff ) done." );

     window->Release( window );

     return DFB_OK;
}

static DFBResult
Test_DestroyWindow( IDirectFBDisplayLayer *layer, void *arg )
{
     IDirectFBWindow *window;

     D_ASSERT( m_toplevel_id != 0 );

     /*
      * Get the top level window
      */
     _T( layer->GetWindow( layer, arg ? (unsigned long) arg : m_toplevel_id, &window ) );

     /*
      * Destroy it
      */
     SHOW_TEST( "Destroy()..." );

     _T( window->Destroy( window ) );

     SHOW_RESULT( "...Destroy() done." );

     window->Release( window );

     return DFB_OK;
}

