/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <signal.h>

#include <stdlib.h>
#include <stdio.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/core.h>
#include <core/sig.h>

#include <misc/conf.h>

typedef struct {
     int              signum;
     struct sigaction old_action;
} SigHandled;

static int sigs_to_handle[] = { /*SIGALRM,*/ SIGHUP, SIGINT, /*SIGPIPE,*/ /*SIGPOLL,*/
                                SIGTERM, /*SIGUSR1, SIGUSR2,*/ /*SIGVTALRM,*/
                                /*SIGSTKFLT,*/ SIGABRT, SIGFPE, SIGILL, SIGQUIT,
                                SIGSEGV, SIGTRAP, /*SIGSYS, SIGEMT,*/ SIGBUS,
                                SIGXCPU, SIGXFSZ };

#define NUM_SIGS_TO_HANDLE (sizeof(sigs_to_handle)/sizeof(sigs_to_handle[0]))

static SigHandled sigs_handled[NUM_SIGS_TO_HANDLE];

void
dfb_sig_remove_handlers()
{
     int i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum != -1) {
               int signum = sigs_handled[i].signum;
               
               if (sigaction( signum, &sigs_handled[i].old_action, NULL )) {
                    PERRORMSG("DirectFB/core/sig: Unable to restore previous "
                              "handler for signal %d!\n", signum);
               }
               
               sigs_handled[i].signum = -1;
          }
     }
}

static void
dfb_sig_handler( int num )
{
     ERRORMSG( "--->  CAUGHT SIGNAL %d  <---\n", num );

     dfb_sig_remove_handlers();

     dfb_core_deinit_emergency();

     kill( 0, num );
}

void
dfb_sig_install_handlers()
{
     int i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (dfb_config->sighandler &&
              !sigismember( &dfb_config->dont_catch, sigs_to_handle[i] ))
          {
               struct sigaction action;
               int              signum = sigs_to_handle[i];

               action.sa_handler = dfb_sig_handler;
               action.sa_flags   = SA_RESTART;
               
               sigfillset( &action.sa_mask );

               if (sigaction( signum, &action, &sigs_handled[i].old_action )) {
                    PERRORMSG("DirectFB/core/sig: Unable to install signal "
                              "handler for signal %d!\n", signum);
                    continue;
               }
               
               sigs_handled[i].signum = signum;
          }
     }
}

void
dfb_sig_block_all()
{
     sigset_t signals;
     
     sigfillset( &signals );

     if (pthread_sigmask( SIG_BLOCK, &signals, NULL ))
          PERRORMSG( "DirectFB/Core: Setting signal mask failed!\n" );
}

