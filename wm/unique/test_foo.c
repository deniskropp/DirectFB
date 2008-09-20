/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/layer_context.h>
#include <core/layers_internal.h>

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

static void
dispatch_motion( UniqueWindow                  *window,
                 const UniqueInputPointerEvent *event )
{
     D_MAGIC_ASSERT( window, UniqueWindow );
     D_ASSERT( event != NULL );

     if (event->buttons) {
          CoreWindowConfig config;

          unique_window_get_config( window, &config );

          config.bounds.x -= window->foo_motion.x - event->x;
          config.bounds.y -= window->foo_motion.y - event->y;

          unique_window_set_config( window, &config, CWCF_POSITION );
     }

     window->foo_motion.x = event->x;
     window->foo_motion.y = event->y;
}

static void
dispatch_button( UniqueWindow                  *window,
                 const UniqueInputPointerEvent *event )
{
     D_MAGIC_ASSERT( window, UniqueWindow );
     D_ASSERT( event != NULL );

     if (event->press)
          unique_window_restack( window, NULL, 1 );
}

static ReactionResult
foo_channel_listener( const void *msg_data,
                      void       *ctx )
{
     const UniqueInputEvent *event   = msg_data;
     UniqueContext          *context = ctx;
     CoreLayerRegion        *region;
     StretRegion            *stret;
     WMShared               *shared;
     static UniqueWindow    *window;

     (void) context;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_DEBUG_AT( UniQuE_TestFoo, "foo_channel_listener( %p, %p )\n", event, context );

     region = context->region;
     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

     shared = context->shared;
     D_MAGIC_ASSERT( shared, WMShared );

     dfb_layer_context_lock( region->context );

     switch (event->type) {
          case UIET_MOTION:
          case UIET_BUTTON:
               /* FIXME: This is a workaround because of the global input channel used for all windows. */
               stret = stret_region_at( context->root, event->pointer.x, event->pointer.y,
                                        SRF_INPUT, shared->region_classes[URCI_FOO] );
               if (stret)
                    window = stret->data;
               else if (event->type == UIET_BUTTON && !event->pointer.buttons)
                    window = NULL;

               if (window) {
                    D_MAGIC_ASSERT( window, UniqueWindow );

                    if (event->type == UIET_MOTION)
                         dispatch_motion( window, &event->pointer );
                    else
                         dispatch_button( window, &event->pointer );
               }
               break;

          case UIET_WHEEL:
               break;

          case UIET_KEY:
               break;

          case UIET_CHANNEL:
               break;

          default:
               D_ONCE( "unknown event type" );
               break;
     }

     dfb_layer_context_unlock( region->context );

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

