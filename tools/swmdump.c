/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/clock.h>
#include <direct/messages.h>

#include <core/palette.h>
#include <core/windows_internal.h>

#include <gfx/convert.h>

#include <sawman.h>
#include <sawman_manager.h>

#include <isawman.h>

static DFBBoolean parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

static void
dump_window( SaWMan       *sawman,
             SaWManWindow *sawwin,
             CoreWindow   *window )
{
     DirectResult      ret;
     int               refs    = -1;
     CoreWindowConfig *config  = &window->config;
     CoreSurface      *surface = window->surface;
     DFBRectangle     *bounds  = &config->bounds;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( window != NULL );

     ret = fusion_ref_stat( &window->object.ref, &refs );
     if (ret)
          D_DERROR( ret, "%s(): fusion_ref_stat() failed!\n", __FUNCTION__ );

#if FUSION_BUILD_MULTI
     printf( "0x%08x [%3lx] :", window->object.ref.multi.id, window->object.ref.multi.creator );
#else
     printf( "N/A              :" );
#endif

     printf( "%3d ", refs );

     printf( "%4d,%4d - %4dx%4d  ", bounds->x, bounds->y, bounds->w, bounds->h );

     if (window->caps & DWCAPS_INPUTONLY) {
          if (window->caps & DWCAPS_NODECORATION)
               printf( "-- input   window --  " );
          else
               printf( "-- border  window --  " );
     }
     else {
          D_MAGIC_ASSERT( surface, CoreSurface );

          printf( "[%4dx%4d %8s]  ", surface->config.size.w, surface->config.size.h,
                  dfb_pixelformat_name( surface->config.format ) );
     }

     printf( "0x%02x  ", config->opacity );

     if (sawwin->flags & SWMWF_INSERTED) {
          D_ASSERT( fusion_vector_contains( &sawman->layout, sawwin ) );

          printf( "+  " );
     }
     else
          printf( "   " );

     if (DFB_WINDOW_FOCUSED( window ))
          printf( "# " );
     else
          printf( "  " );

     printf( "%4d ", window->id );

     if (window->parent_id) {
          D_ASSERT( sawwin->parent != NULL );

          printf( "%4d ", window->parent_id ? : -1 );
     }
     else
          printf( " N/A " );

     switch (config->stacking) {
          case DWSC_UPPER:
               printf( " ^  " );
               break;
          case DWSC_MIDDLE:
               printf( " -  " );
               break;
          case DWSC_LOWER:
               printf( " v  " );
               break;
          default:
               printf( " ?  " );
               break;
     }

     if (window->caps & DWCAPS_ALPHACHANNEL)
          printf( "ALPHACHANNEL " );

     if (window->caps & DWCAPS_INPUTONLY)
          printf( "INPUTONLY    " );

     if (window->caps & DWCAPS_DOUBLEBUFFER)
          printf( "DOUBLEBUFFER " );

     if (window->caps & DWCAPS_NODECORATION)
          printf( "NODECORATION " );

     if (config->options & DWOP_GHOST)
          printf( "GHOST        " );

     if (config->options & DWOP_SCALE)
          printf( "SCALED       " );

     if (config->options & DWOP_COLORKEYING)
          printf( "COLORKEYED   " );

     if (DFB_WINDOW_DESTROYED( window ))
          printf( "DESTROYED    " );

     printf( "\n" );
}

static void
dump_tier( SaWMan *sawman, SaWManTier *tier, int n )
{
     CoreLayer    *layer;
     SaWManWindow *sawwin;
     const char   *is_standard = "";
     const char   *is_border   = "";
     const char   *is_single   = "";

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );

     layer = dfb_layer_at( tier->layer_id );
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     if (tier->active) {
          if (tier->single_mode && tier->single_window)
               is_single = "  (*)";
          else if (tier->border_only)
               is_border = "  (*)";
          else
               is_standard = "  (*)";
     }

     printf( "\nTier %d\n", n );

     printf( "  Stacking  " );
     if (tier->classes & SWMSC_LOWER)
          printf( " LOWER" );
     if (tier->classes & SWMSC_MIDDLE)
          printf( " MIDDLE" );
     if (tier->classes & SWMSC_UPPER)
          printf( " UPPER" );
     printf( "\n" );

     printf( "  Layer      [%d] %s\n", tier->layer_id, layer->shared->description.name );
     printf( "  Size       %dx%d\n", tier->size.w, tier->size.h );
     printf( "  Standard   %dx%d %-8s%s\n", tier->config.width, tier->config.height, dfb_pixelformat_name(tier->config.pixelformat), is_standard );
     printf( "  Border     %dx%d %-8s%s\n", tier->border_config.width, tier->border_config.height, dfb_pixelformat_name(tier->border_config.pixelformat), is_border );
     printf( "  Single     %dx%d %-8s%s\n", tier->single_width, tier->single_height, dfb_pixelformat_name(tier->single_format), is_single );

     if (tier->single_window && tier->single_mode) {
          CoreWindow  *window;
          CoreSurface *surface;

          D_MAGIC_ASSERT( tier->single_window, SaWManWindow );

          window = tier->single_window->window;
          D_ASSERT( window != NULL );

          surface = window->surface;
          D_ASSERT( surface != NULL );

          if (DFB_PIXELFORMAT_IS_INDEXED( tier->single_format )) {
               CorePalette *palette = surface->palette;

               D_ASSERT( palette != NULL );
               D_ASSERT( palette->num_entries > 0 );

               if (tier->single_options & DLOP_SRC_COLORKEY) {
                    int index = window->config.color_key % palette->num_entries;

                    printf( "             Key %d (%02x %02x %02x)\n", index, palette->entries[index].r,
                                                                             palette->entries[index].g,
                                                                             palette->entries[index].b );
               }
          }
          else if (tier->single_options & DLOP_SRC_COLORKEY) {
               DFBColor color;

               dfb_pixel_to_color( surface->config.format, window->config.color_key, &color );

               printf( "             Key 0x%08x (%02x %02x %02x)\n", window->config.color_key, color.r, color.g, color.b );
          }
     }

     printf( "\n"
             "---------------------------------------------------[ Windows in Tier %d ]--------------------------------------------------\n", n );
     printf( "Reference   FID  . Refs  Window - Bounds      Surface - Format   Opacity In Fo  ID  PID  St Capabilities / State & Options\n" );
     printf( "--------------------------------------------------------------------------------------------------------------------------\n" );

     direct_list_foreach (sawwin, sawman->windows) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_ASSERT( window != NULL );

          if (tier->classes & (1 << window->config.stacking))
               dump_window( sawman, sawwin, window );
     }

     printf( "\n" );
}

static void
dump_tiers( SaWMan *sawman )
{
     int         n = 0;
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );

     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          dump_tier( sawman, tier, n++ );
     }
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult     ret;
     long long     millis;
     long int      seconds, minutes, hours, days;
     IDirectFB    *dfb;
     ISaWMan      *saw;
     ISaWMan_data *data;

     char *buffer = malloc( 0x100000 );

     setvbuf( stdout, buffer, _IOFBF, 0x100000 );

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate", ret );
          return -3;
     }

     /* Create the SaWMan interface. */
     ret = SaWManCreate( &saw );
     if (ret) {
          DirectFBError( "SaWManCreate", ret );
          return -4;
     }

     /* Get private data pointer from SaWMan interface. */
     data = saw->priv;
     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( data->sawman, SaWMan );

     /* Get uptime. */
     millis = direct_clock_get_millis();

     seconds  = millis / 1000;
     millis  %= 1000;

     minutes  = seconds / 60;
     seconds %= 60;

     hours    = minutes / 60;
     minutes %= 60;

     days     = hours / 24;
     hours   %= 24;

     /* Print uptime. */
     switch (days) {
          case 0:
               printf( "\nSaWMan uptime: %02ld:%02ld:%02ld\n",
                       hours, minutes, seconds );
               break;

          case 1:
               printf( "\nSaWMan uptime: %ld day, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;

          default:
               printf( "\nSaWMan uptime: %ld days, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;
     }

     /* Dump information about tiers. */
     dump_tiers( data->sawman );

     printf( "\n" );

     /* SaWMan deinitialization. */
     saw->Release( saw );

     /* DirectFB deinitialization. */
     dfb->Release( dfb );

     return 0;
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "\nSaWMan Dump (version %s)\n\n", SAWMAN_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -g, --geometry  Show advanced geometry settings\n");
     fprintf (stderr, "   -h, --help      Show this help message\n");
     fprintf (stderr, "   -v, --version   Print version information\n");
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "swmdump version %s\n", SAWMAN_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-g") == 0 || strcmp (arg, "--geometry") == 0) {
//               show_geometry = true;
               continue;
          }

          print_usage (argv[0]);

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

