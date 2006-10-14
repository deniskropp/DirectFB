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

#include <unique/context.h>
#include <unique/uniquewm.h>

/*****************************************************************************/

static IDirectFB  *dfb      = NULL;

static const char *filename = NULL;
static DFBBoolean  color    = DFB_FALSE;


/*****************************************************************************/

static DFBBoolean parse_command_line   ( int argc, char *argv[] );
static void       set_color ( void );

/*****************************************************************************/

static UniqueContext *context;

static bool
context_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     if (object->state != FOS_ACTIVE)
          return true;

     context = (UniqueContext*) object;
     if (unique_context_ref( context )) {
          D_ERROR( "UniQuE/Test: unique_context_ref() failed!\n" );
          return true;
     }

     return false;
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;

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

     if (!unique_wm_running()) {
          D_ERROR( "UniQuE/Test: This session doesn't run UniQuE!\n" );
          dfb->Release( dfb );
          return EXIT_FAILURE;
     }

     unique_wm_enum_contexts( context_callback, NULL );

     if (!context) {
          D_ERROR( "UniQuE/Test: No context available!\n" );
          dfb->Release( dfb );
          return EXIT_FAILURE;
     }

     /* Set the background according to the users wishes. */
     if (color)
          set_color();

     unique_context_unref( context );

     /* Release the super interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/*****************************************************************************/

static void
print_usage( const char *prg_name )
{
     fprintf( stderr,
              "\n"
              "UniQuE Test Application (version %s)\n"
              "\n"
              "Usage: %s [options] <color>\n"
              "\n"
              "Options:\n"
              "   -c, --color     Set <color> in AARRGGBB format (hexadecimal)\n"
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
               if (!filename) {
                    filename = a;
                    continue;
               }
               else {
                    print_usage( prg_name );
                    return DFB_FALSE;
               }
          }
          if (strcmp (a, "-h") == 0 || strcmp (a, "--help") == 0) {
               print_usage( prg_name );
               return DFB_FALSE;
          }
          if (strcmp (a, "-v") == 0 || strcmp (a, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }
          if (strcmp (a, "-c") == 0 || strcmp (a, "--color") == 0) {
               color = DFB_TRUE;
               continue;
          }
     }

     if (!filename) {
          print_usage( prg_name );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static void
set_color()
{
     DFBResult  ret;
     char      *error;
     u32        argb;
     DFBColor   color;

     if (*filename == '#')
          filename++;

     argb = strtoul( filename, &error, 16 );

     if (*error) {
          fprintf( stderr, "Invalid characters in color string: '%s'\n", error );
          return;
     }

     color.a = (argb & 0xFF000000) >> 24;
     color.r = (argb & 0xFF0000) >> 16;
     color.g = (argb & 0xFF00) >> 8;
     color.b = (argb & 0xFF);

     ret = unique_context_set_color( context, &color );
     if (ret)
          D_DERROR( ret, "UniQuE/Test: unique_context_set_color() failed!\n" );
}

