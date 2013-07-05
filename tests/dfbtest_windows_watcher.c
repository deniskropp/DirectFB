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

//#define DIRECT_ENABLE_DEBUG

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
#include <directfb_windows.h>

static const DirectFBPixelFormatNames( format_names );
static const DirectFBWindowCapabilitiesNames( caps_names );
static const DirectFBWindowOptionsNames( options_names );

/**********************************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

static void
dump_config( const DFBWindowConfig *config )
{
     D_INFO( "  -> bounds       %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( &config->bounds ) );
     D_INFO( "  -> opacity      %d\n", config->opacity );
     D_INFO( "  -> cursor flags 0x%08x\n", config->cursor_flags );
}

/**********************************************************************************************************************/

static void
Test_Watcher_WindowAdd( void                *context,
                        const DFBWindowInfo *info )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, info->window_id );
     D_INFO( "  -> caps         0x%08x\n", info->caps );
     D_INFO( "  -> resource id  0x%016llx\n", (unsigned long long) info->resource_id );
     D_INFO( "  -> process id   %d\n", info->process_id );
     D_INFO( "  -> instance id  %d\n", info->instance_id );
     D_INFO( "  -> state        0x%08x\n", info->state );

     dump_config( &info->config );
}

static void
Test_Watcher_WindowRemove( void        *context,
                           DFBWindowID  window_id )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, window_id );
}

static void
Test_Watcher_WindowConfig( void                  *context,
                           DFBWindowID            window_id,
                           const DFBWindowConfig *config,
                           DFBWindowConfigFlags   flags )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, window_id );
     D_INFO( "  -> flags        0x%08x\n", flags );
     D_INFO( "  -> cursor flags 0x%08x\n", config->cursor_flags );

     dump_config( config );
}

static void
Test_Watcher_WindowState( void                 *context,
                          DFBWindowID           window_id,
                          const DFBWindowState *state )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, window_id );
     D_INFO( "  -> flags        0x%08x\n", state->flags );
}

static void
Test_Watcher_WindowRestack( void         *context,
                            DFBWindowID   window_id,
                            unsigned int  index )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, window_id );
     D_INFO( "  -> index        %u\n", index );
}

static void
Test_Watcher_WindowFocus( void        *context,
                          DFBWindowID  window_id )
{
     D_INFO( "%s( ID %u )\n", __FUNCTION__, window_id );
}

static DFBWindowsWatcher watcher = {
     .WindowAdd     = Test_Watcher_WindowAdd,
     .WindowRemove  = Test_Watcher_WindowRemove,
     .WindowConfig  = Test_Watcher_WindowConfig,
     .WindowState   = Test_Watcher_WindowState,
     .WindowRestack = Test_Watcher_WindowRestack,
     .WindowFocus   = Test_Watcher_WindowFocus,
};

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult         ret;
     IDirectFB        *dfb;
     IDirectFBWindows *windows;

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

     ret = dfb->GetInterface( dfb, "IDirectFBWindows", NULL, NULL, (void**) &windows );
     if (ret) {
          D_DERROR( ret, "IDirectFB::GetInterface( 'IDirectFBWindows' ) failed!\n" );
          return -4;
     }

     ret = windows->RegisterWatcher( windows, &watcher, NULL );
     if (ret) {
          D_DERROR( ret, "IDirectFBWindows::RegisterWatcher() failed!\n" );
          return -5;
     }

     pause();

     /* Release the windows interface. */
     windows->Release( windows );

     /* Release the super interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Windows Watcher Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg_name);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                            Show this help message\n");
     fprintf (stderr, "  -v, --version                         Print version information\n");
     fprintf (stderr, "\n");
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

          print_usage (argv[0]);

          return false;
     }

     return true;
}

