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

/**********************************************************************************************************************/

static int parse_cmdline ( int argc, char *argv[] );
static int show_usage    ( void );

/**********************************************************************************************************************/

#define MAX_PARTICIPANTS (20)

static int participants = 2;
static int participant;

struct sharedData {
     volatile int   initDone;

     FusionCall     call[MAX_PARTICIPANTS+1]; /* 0 = empty. just being lazy. */
     FusionSkirmish skirmish;
};

/**********************************************************************************************************************/

static FusionCallHandlerResult
call_handler( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     struct sharedData *shared = ctx;

     int ret = 0;

     fusion_skirmish_prevail( &shared->skirmish );

     if (participant < participants)
          if (random() & 1)
               fusion_call_execute( &shared->call[participant+1], FCEF_NONE, 0, 0, &ret );

     fusion_skirmish_dismiss( &shared->skirmish );

     *ret_val = ret + 1;
     return FCHR_RETURN;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult         ret;
     FusionWorld         *world;
     FusionSHMPoolShared *pool;
     struct sharedData   *shared;
     sigset_t             block;

     int retcall;
     int i;

     if (parse_cmdline( argc, argv ))
          return -1;

     ret = fusion_enter( 0, 23, FER_ANY, &world );
     if (ret)
          return ret;

     ret = fusion_shm_pool_create( world, "Shared Memory", 8192, false, &pool );
     if (ret)
          return ret;

     ret = fusion_shm_pool_allocate( pool, sizeof(struct sharedData), true, true, (void**)&shared );
     if (ret)
          return ret;

     /*
      * Do the fork() magic!
      */
     fusion_world_set_fork_action( world, FFA_FORK );

     for (i=1; i<participants; i++) {

          pid_t f = fork();

          if (f == -1) {
               D_PERROR( "fork() failed!\n" );
               return -1;
          }

          if (f) {
               /* parent */
               participant = i;
               break;
          }

          participant = i+1;
     }

     fusion_world_set_fork_action( world, FFA_CLOSE );

     ret = fusion_call_init( &shared->call[participant], call_handler, shared, world );
     if (ret)
          return ret;
     shared->initDone++;

     /* wait a bit before start, make sure the calls exist */
     while( shared->initDone != participants )
          usleep(50000);

     if (participant == 1) {
          /* we generate a skirmish that will be transported along */
          ret = fusion_skirmish_init( &shared->skirmish, "Test", world );
          if (ret)
               return -1;

          fusion_skirmish_prevail( &shared->skirmish );

          for(;;) {
               fusion_call_execute( &shared->call[2], FCEF_NONE, 0, 0, &retcall );
               printf("pcp %d, result: %d\n", participant, retcall);
          }
     }

     /* we rely on exit() */
     sigemptyset( &block );
     sigsuspend( &block );
}

/**********************************************************************************************************************/

static int
parse_cmdline( int argc, char *argv[] )
{
     int   i;
     char *end;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-p" )) {
               if (++i < argc) {
                    participants = strtoul( argv[i], &end, 10 );

                    if (end && *end) {
                         D_ERROR( "Parse error in number '%s'!\n", argv[i] );
                         return -1;
                    }

                    if (participants < 1 || participants > MAX_PARTICIPANTS)
                         return show_usage();
               }
               else
                    return show_usage();
          }
     }

     return 0;
}

static int
show_usage( void )
{
     fprintf( stderr, "\n"
                      "Usage:\n"
                      "   fusion_call [options]\n"
                      "\n"
                      "Options:\n"
                      "   -p <2-MAX>  Number of participants\n"
                      "\n"
              );

     return -1;
}

