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

#ifndef __SIG_H__
#define __SIG_H__

#include <pthread.h>
#include <signal.h>

#include <core/coredefs.h>

/*
 * installs a signal handler for all signals
 * that would cause the program to terminate
 */
void dfb_sig_install_handlers();

/*
 * removes all installed handlers
 */
void dfb_sig_remove_handlers();


/*
 * Modifies the current thread's signal mask to block everything.
 * Should be called by input threads once to avoid killing themselves
 * in the signal handler by deinitializing all input drivers.
 */
void dfb_sig_block_all();

#endif
