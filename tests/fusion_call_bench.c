/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <direct/messages.h>

#include <fusion/call.h>
#include <fusion/lock.h>
#include <fusion/fusion.h>
#include <fusion/shm/pool.h>

#ifndef HAVE_FORK
# define fork() -1
#endif


static bool sync_calls;

/**********************************************************************************************************************/

static int parse_cmdline ( int argc, char *argv[] );
static int show_usage    ( void );

/**********************************************************************************************************************/

static FusionCallHandlerResult
call_handler( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     *ret_val = 0;

//     if (call_arg)
//          exit(0);

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

#define NUM_ITEMS 300000

int
main( int argc, char *argv[] )
{
     DirectResult         ret;
     DirectClock          clock;
     FusionWorld         *world;
     sigset_t             block;
     FusionCall           call = { 0 };

     int retcall;
     int i;

     if (parse_cmdline( argc, argv ))
          return -1;

     ret = fusion_enter( 0, 23, FER_ANY, &world );
     if (ret)
          return ret;

     ret = fusion_call_init( &call, call_handler, NULL, world );
     if (ret)
          return ret;

     /*
      * Do the fork() magic!
      */
     fusion_world_set_fork_action( world, FFA_FORK );

     pid_t f = fork();

     if (f == -1) {
          D_PERROR( "fork() failed!\n" );
          return -1;
     }

     fusion_world_set_fork_action( world, FFA_CLOSE );

     if (f) {
          /* we rely on exit() */
          sigemptyset( &block );
          sigsuspend( &block );
     }

     direct_clock_start( &clock );

     for (i=0; i<NUM_ITEMS; i++)
          fusion_call_execute( &call, sync_calls ? FCEF_NONE : FCEF_ONEWAY, 0, 0, &retcall );

     fusion_call_execute( &call, FCEF_NONE, 1, 0, &retcall );

     direct_clock_stop( &clock );


     D_INFO( "Fusion/Call: Stopped after %lld.%03lld seconds... (%lld items/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );

     return 0;
}

/**********************************************************************************************************************/

static int
parse_cmdline( int argc, char *argv[] )
{
     int i;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-s" ))
               sync_calls = true;
          else
               return show_usage();
     }

     return 0;
}

static int
show_usage( void )
{
     fprintf( stderr, "\n"
                      "Usage:\n"
                      "   fusion_call_bench [options]\n"
                      "\n"
                      "Options:\n"
                      "   -s  Synchronous calls\n"
                      "\n"
              );

     return -1;
}

