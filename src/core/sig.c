/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

static int sigs_to_handle[] = { /*SIGALRM,*/ SIGHUP, SIGINT, SIGPIPE, /*SIGPOLL,*/
                                SIGTERM, /*SIGUSR1, SIGUSR2,*/ /*SIGVTALRM,*/
                                /*SIGSTKFLT,*/ SIGABRT, SIGFPE, SIGILL, SIGQUIT,
                                SIGSEGV, SIGTRAP, /*SIGSYS, SIGEMT,*/ SIGBUS,
                                SIGXCPU, SIGXFSZ, -1 };

void sig_remove_handlers()
{
     int *sigs = sigs_to_handle;

     while (*sigs != -1) {
          signal( *sigs, SIG_DFL );
          sigs++;
     }
}

void sig_handler( int num )
{
     ERRORMSG( "--->  CAUGHT SIGNAL %d  <---\n", num );

     sig_remove_handlers();

     core_deinit_emergency();
     
     kill( 0, SIGHUP );
}

void sig_install_handlers()
{
   int *sigs = sigs_to_handle;

   while (*sigs != -1) {
      signal( *sigs, sig_handler );
      sigs++;
   }
}

