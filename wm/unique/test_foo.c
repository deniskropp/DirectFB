/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <direct/debug.h>

#include <unique/context.h>
#include <unique/input_channel.h>
#include <unique/internal.h>
#include <unique/uniquewm.h>

D_DEBUG_DOMAIN( UniQuE_TestFoo, "UniQuE/TestFoo", "UniQuE's Test Foo Application" );

/*****************************************************************************/

static IDirectFB *dfb = NULL;

/*****************************************************************************/

static DFBBoolean parse_command_line( int argc, char *argv[] );

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

static ReactionResult
foo_channel_listener( const void *msg_data,
                      void       *ctx )
{
     const UniqueInputEvent *event   = msg_data;
     UniqueContext          *context = ctx;

     (void) context;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_DEBUG_AT( UniQuE_TestFoo, "foo_channel_listener( %p, %p )\n", event, context );

     switch (event->type) {
          case UIET_MOTION:
               //dispatch_motion( window, event );
               break;

          case UIET_BUTTON:
               //dispatch_button( window, event );
               break;

          case UIET_WHEEL:
               //dispatch_wheel( window, event );
               break;

          case UIET_KEY:
               //dispatch_key( window, event );
               break;

          case UIET_CHANNEL:
               //dispatch_channel( window, event );
               break;

          default:
               D_ONCE( "unknown event type" );
               break;
     }

     return RS_OK;
}


int
main( int argc, char *argv[] )
{
     DFBResult ret;
     Reaction  reaction;

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


     unique_input_channel_attach( context->foo_channel, foo_channel_listener, context, &reaction );

     pause();

     unique_input_channel_detach( context->foo_channel, &reaction );

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
              "UniQuE Foo Test (version %s)\n"
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

