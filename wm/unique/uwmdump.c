/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <direct/util.h>

#include <fusion/build.h>
#include <fusion/vector.h>

#include <core/core.h>
#include <core/windowstack.h>

#include <unique/context.h>
#include <unique/internal.h>
#include <unique/uniquewm.h>


static void
print_usage( const char *prg_name )
{
     fprintf( stderr,
              "\n"
              "UniQuE Dump (version %s)\n"
              "\n"
              "Usage: %s [options]\n"
              "\n"
              "Options:\n"
              "   -h, --help      Show this help message\n"
              "   -v, --version   Print version information\n"
              "\n",
              DIRECTFB_VERSION, prg_name );
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int         n;
     const char *prg_name = strrchr( argv[0], '/' );

     if (prg_name)
          prg_name++;
     else
          prg_name = argv[0];

     for (n = 1; n < argc; n++) {
          const char *a = argv[n];

          if (*a != '-') {
               print_usage( prg_name );
               return DFB_FALSE;
          }
          if (strcmp (a, "-h") == 0 || strcmp (a, "--help") == 0) {
               print_usage( prg_name );
               return DFB_FALSE;
          }
          if (strcmp (a, "-v") == 0 || strcmp (a, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }
     }

     return DFB_TRUE;
}

static bool
window_callback( UniqueWindow *window )
{
     int           refs;
     DirectResult  ret;
     DFBRectangle *bounds = &window->bounds;

     if (window->object.state != FOS_ACTIVE)
          return true;

     ret = fusion_ref_stat( &window->object.ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return false;
     }

#if FUSION_BUILD_MULTI
     printf( "0x%08x : ", window->object.ref.multi.id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d   ", refs );


     printf( "%4d, %4d   ", bounds->x, bounds->y );

     printf( "%4d x %4d    ", bounds->w, bounds->h );

     printf( "0x%02x ", window->opacity );

     printf( "%5d  ", dfb_window_id( window->window ) );

     if (window->caps & DWHC_TOPMOST) {
          printf( "*  " );
     }
     else {
          switch (window->stacking) {
               case DWSC_UPPER:
                    printf( "^  " );
                    break;
               case DWSC_MIDDLE:
                    printf( "-  " );
                    break;
               case DWSC_LOWER:
                    printf( "v  " );
                    break;
               default:
                    printf( "?  " );
                    break;
          }
     }

     if (D_FLAGS_IS_SET( window->flags, UWF_VISIBLE ))
          printf( "VISIBLE    " );

     if (D_FLAGS_IS_SET( window->flags, UWF_DECORATED ))
          printf( "DECORATED  " );

     if (D_FLAGS_IS_SET( window->flags, UWF_DESTROYED ))
          printf( "DESTROYED  " );


     printf( "\n" );

     return true;
}

static bool
context_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     int            refs;
     DirectResult   ret;
     UniqueContext *context = (UniqueContext*) object;

     if (object->state != FOS_ACTIVE)
          return true;

     ret = fusion_ref_stat( &object->ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return false;
     }

     printf( "\n"
             "-------[ Contexts ]-------\n"
             "Reference  . Refs  Windows\n"
             "--------------------------\n" );

#if FUSION_BUILD_MULTI
     printf( "0x%08x : ", object->ref.multi.id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d    ", refs );

     printf( "%2d   ", fusion_vector_size( &context->windows ) );

     printf( "\n" );


     ret = dfb_windowstack_lock( context->stack );
     if (ret) {
          D_DERROR( ret, "UniQuE/Dump: Could not lock window stack!\n" );
          return true;
     }

     if (fusion_vector_has_elements( &context->windows )) {
          int           index;
          UniqueWindow *window;

          printf( "\n"
                  "-----------------------------------[ Windows ]------------------------------------\n" );
          printf( "Reference  . Refs     X     Y   Width Height Opacity   ID     Flags\n" );
          printf( "----------------------------------------------------------------------------------\n" );

          fusion_vector_foreach_reverse( window, index, context->windows )
               window_callback( window );
     }

     dfb_windowstack_unlock( context->stack );

     printf( "\n" );

     return true;
}

int
main( int argc, char *argv[] )
{
     DFBResult  ret;
     CoreDFB   *core;

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
     ret = dfb_core_create( &core );
     if (ret) {
          DirectFBError( "dfb_core_create() failed", ret );
          return -3;
     }

     if (!unique_wm_running()) {
          D_ERROR( "UniQuE/Dump: This session doesn't run UniQuE!\n" );
          dfb_core_destroy( core, false );
          return EXIT_FAILURE;
     }


     unique_wm_enum_contexts( context_callback, NULL );


     /* Release the super interface. */
     dfb_core_destroy( core, false );

     return EXIT_SUCCESS;
}

