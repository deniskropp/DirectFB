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

#include "directfb.h"

#include "coredefs.h"
#include "core.h"
#include "sig.h"
#include "vt.h"

static int sigs_to_handle[] = { /*SIGALRM,*/ SIGHUP, SIGINT, /*SIGPIPE,*/ /*SIGPOLL,*/
                                SIGTERM, /*SIGUSR1, SIGUSR2,*/ /*SIGVTALRM,*/
                                /*SIGSTKFLT,*/ SIGABRT, SIGFPE, SIGILL, SIGQUIT,
                                SIGSEGV, SIGTRAP, /*SIGSYS, SIGEMT,*/ SIGBUS,
                                SIGXCPU, SIGXFSZ, -1 };

void dfb_sig_remove_handlers()
{
     int *sigs = sigs_to_handle;

     while (*sigs != -1) {
          if (!sigismember( &dfb_config->dont_catch, *sigs ))
               signal( *sigs, SIG_DFL );

          sigs++;
     }
}

static void dfb_sig_handler( int num )
{
     ERRORMSG( "--->  CAUGHT SIGNAL %d  <---\n", num );

     dfb_sig_remove_handlers();

     dfb_core_deinit_emergency();

     kill( 0, num );
}

void dfb_sig_install_handlers()
{
     int *sigs = sigs_to_handle;

     while (*sigs != -1) {
          if (!sigismember( &dfb_config->dont_catch, *sigs ))
               signal( *sigs, dfb_sig_handler );

          sigs++;
     }
}

