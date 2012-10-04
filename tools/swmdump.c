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
#include <sawman_internal.h>

#include <isawman.h>

static DFBBoolean show_geometry = DFB_FALSE;
static DFBBoolean m_listen      = DFB_FALSE;
static DFBBoolean m_performance = DFB_FALSE;

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
     DFBRectangle     *bounds  = &sawwin->bounds;

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

     if (    (window->caps & DWCAPS_INPUTONLY)
          || (window->config.options & DWOP_INPUTONLY) )
     {
          if (window->caps & DWCAPS_NODECORATION)
               printf( "-- input   window --  " );
          else
               printf( "-- border  window --  " );
     }
     else if (window->caps & DWCAPS_COLOR)
          printf( "-- color   window --  " );
     else {
          D_MAGIC_ASSERT( surface, CoreSurface );

          printf( "[%4dx%4d %8s]  ", surface->config.size.w, surface->config.size.h,
                  dfb_pixelformat_name( surface->config.format ) );
     }

     printf( "0x%02x  ", config->opacity );

     printf( "%2d ", fusion_vector_index_of( &sawman->layout, sawwin ) );

     if (DFB_WINDOW_FOCUSED( window ))
          printf( "# " );
     else
          printf( "  " );

     printf( "%4d ", window->id );

     if (window->config.association) {
          D_ASSUME( sawwin->parent != NULL );

          printf( "%4d ", window->config.association );
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

#ifndef OLD_COREWINDOWS_STRUCTURE
     if (window->caps & DWCAPS_SUBWINDOW)
          printf( "SUBWINDOW(%2d) ", window->toplevel_id );
#endif

     if (window->caps & DWCAPS_ALPHACHANNEL)
          printf( "ALPHACHANNEL  " );

     if (config->options & DWCAPS_COLOR)
          printf( "COLOR         " );

     if (window->caps & DWCAPS_INPUTONLY)
          printf( "INPUTONLY     " );

     if (window->caps & DWCAPS_DOUBLEBUFFER)
          printf( "DOUBLEBUFFER  " );

     if (window->caps & DWCAPS_NODECORATION)
          printf( "NODECORATION  " );

     if (config->options & DWOP_GHOST)
          printf( "GHOST         " );

     if (config->options & DWOP_SCALE)
          printf( "SCALED        " );

     if (config->options & DWOP_COLORKEYING)
          printf( "COLORKEYED    " );

     if (DFB_WINDOW_DESTROYED( window ))
          printf( "DESTROYED     " );

     printf( "\n" );


     if (show_geometry) {
          printf( "                      " );

          printf( "%4d,%4d - %4dx%4d   ", sawwin->dst.x, sawwin->dst.y, sawwin->dst.w, sawwin->dst.h );
          printf( "%4dx%4d - %3d,%3d   ", sawwin->src.w, sawwin->src.h, sawwin->src.x, sawwin->src.y );


          printf( "\n" );
     }
}

static void
dump_tier( SaWMan *sawman, SaWManTier *tier, int n )
{
     CoreLayer       *layer;
     CoreLayerRegion *region;
     SaWManWindow    *sawwin;
     const char      *is_standard = "";
     const char      *is_border   = "";
     const char      *is_single   = "";

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );

     layer = dfb_layer_at( tier->layer_id );
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     region = tier->region;

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

     printf( "  Layer         [%d] %s\n", tier->layer_id, layer->shared->description.name );
     printf( "  Size          %dx%d\n", tier->size.w, tier->size.h );
     printf( "  Standard      %dx%d %-8s%s\n", tier->config.width, tier->config.height, dfb_pixelformat_name(tier->config.pixelformat), is_standard );
     printf( "  Border        %dx%d %-8s%s\n", tier->border_config.width, tier->border_config.height, dfb_pixelformat_name(tier->border_config.pixelformat), is_border );
     printf( "  Single        %dx%d %-8s%s\n", tier->single_width, tier->single_height, dfb_pixelformat_name(tier->single_format), is_single );

     if (tier->single_window && tier->single_mode) {
          CoreWindow  *window;
          CoreSurface *surface;

          D_MAGIC_ASSERT( tier->single_window, SaWManWindow );

          window = tier->single_window->window;
          D_ASSERT( window != NULL );

          surface = window->surface;
          D_ASSERT( surface != NULL );

          printf( "             Window %p <%p> [%u]\n", tier->single_window, window, window->id );

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

          printf( "             Dest   %4d,%4d - %4dx%4d\n", DFB_RECTANGLE_VALS( &tier->single_dst ) );
          printf( "             Source %4d,%4d - %4dx%4d\n", DFB_RECTANGLE_VALS( &tier->single_src ) );
     }

     if (region) {
          printf( "  Layer Region\n" );
          printf( "             Dest   %4d,%4d - %4dx%4d\n", DFB_RECTANGLE_VALS( &region->config.dest ) );
          printf( "             Source %4d,%4d - %4dx%4d\n", DFB_RECTANGLE_VALS( &region->config.source ) );
          printf( "             State  0x%08x\n", region->state );
     }

     if (tier->left.updates.num_regions) {
          int i;

          printf( "  Left Updates\n" );

          for (i=0; i<tier->left.updates.num_regions; i++)
               printf( "             [%d]    %4d,%4d - %4dx%4d\n", i, DFB_RECTANGLE_VALS_FROM_REGION( &tier->left.updates.regions[i] ) );
     }

     if (tier->right.updates.num_regions) {
          int i;

          printf( "  Right Updates\n" );

          for (i=0; i<tier->right.updates.num_regions; i++)
               printf( "             [%d]    %4d,%4d - %4dx%4d\n", i, DFB_RECTANGLE_VALS_FROM_REGION( &tier->right.updates.regions[i] ) );
     }

     printf( "\n"
             "---------------------------------------------------[ Windows in Tier %d ]---------------------------------------------\n", n );
     printf( "Reference   FID  . Refs  Window - Bounds      Surface - Format   Opacity In Fo  ID  PID  St Caps, State, Options...\n" );
     printf( "---------------------------------------------------------------------------------------------------------------------\n" );

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

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          dump_tier( sawman, tier, n++ );
     }
}

/**********************************************************************************************************************/

static void
Listen_TierUpdate( void                *context,
                   DFBSurfaceStereoEye  stereo_eye,
                   DFBDisplayLayerID    layer_id,
                   const DFBRegion     *updates,
                   unsigned int         num_updates )
{
     unsigned int i;

     for (i=0; i<num_updates; i++) {
          D_INFO( "Tier/Update: %-5s [%u] "DFB_RECT_FORMAT"\n",
                  stereo_eye == DSSE_LEFT ? "LEFT" : "RIGHT", layer_id,
                  DFB_RECTANGLE_VALS_FROM_REGION(&updates[i]) );
     }
}

static void
Listen_WindowBlit( void                *context,
                   DFBSurfaceStereoEye  stereo_eye,
                   DFBWindowID          window_id,
                   u32                  resource_id,
                   const DFBRectangle  *src,
                   const DFBRectangle  *dst )
{
     D_INFO( "Window/Blit: %-5s [%u] (0x%04x) "DFB_RECT_FORMAT" -> "DFB_RECT_FORMAT"\n",
             stereo_eye == DSSE_LEFT ? "LEFT" : "RIGHT", window_id, resource_id,
             DFB_RECTANGLE_VALS(src), DFB_RECTANGLE_VALS(dst) );
}

static SaWManListeners m_listeners = {
     .TierUpdate = Listen_TierUpdate,
     .WindowBlit = Listen_WindowBlit,
};

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
     if (data->sawman->unselkeys_window) {
          CoreWindow *window = data->sawman->unselkeys_window->window;

          printf( "Collector window: ID %d (%d,%d-%dx%d, caps 0x%08x)\n", window->id,
                  DFB_RECTANGLE_VALS( &window->config.bounds ), window->caps );
     }
     else
          printf( "No collector window\n" );
     printf( "\n" );

     if (m_listen) {
          ret = saw->RegisterListeners( saw, &m_listeners, NULL );
          if (ret) {
               DirectFBError( "ISaWMan::RegisterListener", ret );
               return -5;
          }

          pause();
     }

     fflush( stdout );

     if (m_performance) {
          unsigned int       updates;
          unsigned long long pixels;
          long long          duration;

          saw->GetPerformance( saw, DWSC_LOWER, DFB_TRUE, &updates, &pixels, &duration );
          saw->GetPerformance( saw, DWSC_UPPER, DFB_TRUE, &updates, &pixels, &duration );

          while (true) {
               unsigned int mpixels;

               sleep( 2 );


               ret = saw->GetPerformance( saw, DWSC_LOWER, DFB_TRUE, &updates, &pixels, &duration );
               if (ret) {
                    DirectFBError( "ISaWMan::GetPerformance", ret );
                    return -6;
               }

               mpixels = pixels / 1000000;

               D_INFO( "Performance [LOWER]: %u updates (%u /sec), %u Mpixels (%u /sec)\n",
                       updates, updates * 1000 / (int)duration, mpixels, mpixels * 1000 / (int)duration );


               ret = saw->GetPerformance( saw, DWSC_UPPER, DFB_TRUE, &updates, &pixels, &duration );
               if (ret) {
                    DirectFBError( "ISaWMan::GetPerformance", ret );
                    return -6;
               }

               // FIXME: need to know if different hw layers
               if (duration > 1000) {
                    mpixels = pixels / 1000000;

                    D_INFO( "Performance [UPPER]: %u updates (%u /sec), %u Mpixels (%u /sec)\n",
                            updates, updates * 1000 / (int)duration, mpixels, mpixels * 1000 / (int)duration );
               }
          }
     }

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
     fprintf (stderr, "\nSaWMan Dump (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -g, --geometry     Show advanced geometry settings\n");
     fprintf (stderr, "   -l, --listen       Register listener and print events\n");
     fprintf (stderr, "   -p, --performance  Show performance counters\n");
     fprintf (stderr, "   -h, --help         Show this help message\n");
     fprintf (stderr, "   -v, --version      Print version information\n");
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
               fprintf (stderr, "swmdump version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-g") == 0 || strcmp (arg, "--geometry") == 0) {
               show_geometry = true;
               continue;
          }

          if (strcmp (arg, "-l") == 0 || strcmp (arg, "--listen") == 0) {
               m_listen = true;
               continue;
          }

          if (strcmp (arg, "-p") == 0 || strcmp (arg, "--performance") == 0) {
               m_performance = true;
               continue;
          }

          print_usage (argv[0]);

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

