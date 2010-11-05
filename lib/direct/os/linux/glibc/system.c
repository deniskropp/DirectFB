/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <linux/unistd.h>

#include <direct/atomic.h>
#include <direct/debug.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Futex, "Direct/Futex", "Direct Futex" );
D_LOG_DOMAIN( Direct_Trap,  "Direct/Trap",  "Direct Trap" );

/**********************************************************************************************************************/

#if DIRECT_BUILD_GETTID && defined(HAVE_LINUX_UNISTD_H)
#include <linux/unistd.h>
#endif

/**********************************************************************************************************************/

long
direct_pagesize( void )
{
     return sysconf( _SC_PAGESIZE );
}

unsigned long
direct_page_align( unsigned long value )
{
     unsigned long mask = sysconf( _SC_PAGESIZE ) - 1;

     return (value + mask) & ~mask;
}

/**********************************************************************************************************************/

pid_t
direct_getpid( void )
{
     return getpid();
}

__attribute__((no_instrument_function))
pid_t
direct_gettid( void )
{
     pid_t tid = -1;
#if DIRECT_BUILD_GETTID && defined(__NR_gettid) /* present on linux >= 2.4.20 */
     tid = syscall(__NR_gettid);
#endif
     if (tid < 0)
          tid = getpid();

     return tid;
}

/**********************************************************************************************************************/

DirectResult
direct_tgkill( int tgid, int tid, int sig )
{
#if defined(__NR_tgkill) /* present on linux >= 2.5.75 */
     if (syscall(__NR_tgkill) < 0)
#else
#warning no tgkill
#endif
          return errno2result( errno );

     return DR_OK;
}

void
direct_trap( const char *domain, int sig )
{
     sigval_t val;

     D_LOG( Direct_Trap, VERBOSE, "Raising signal %d from %s...\n", sig, domain );

     val.sival_int = direct_gettid();

     sigqueue( direct_gettid(), sig, val );
//     direct_tgkill( direct_getpid(), direct_gettid(), sig );

     D_LOG( Direct_Trap, VERBOSE, "...tgkill(%d) on ourself returned, maybe blocked, calling %s()!\n", sig,
#ifdef __NR_exit_group
            "exit_group" );

     syscall( __NR_exit_group, DR_BUG );
#else
            "_exit" );

            _exit( DR_BUG );
#endif
}

/**********************************************************************************************************************/

DirectResult
direct_kill( pid_t pid, int sig )
{
     if (kill( pid, sig ))
          return errno2result( errno );

     return DR_OK;
}

void
direct_sync( void )
{
     sync();
}

DirectResult
direct_socketpair( int __domain, int __type, int __protocol, int __fds[2] )
{
     if (socketpair( __domain, __type, __protocol, __fds ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_sigprocmask( int __how, __const sigset_t *__restrict __set,
                    sigset_t *__restrict __oset )
{
     if (sigprocmask( __how, __set, __oset ))
          return errno2result( errno );

     return DR_OK;
}

uid_t
direct_getuid()
{
     return getuid();
}

uid_t
direct_geteuid()
{
     return geteuid();
}

/**********************************************************************************************************************/

DirectResult
direct_futex( int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3 )
{
     int          ret;
     unsigned int count;

     switch (op) {
          case FUTEX_WAIT:
               count = D_SYNC_ADD_AND_FETCH( &__Direct_Futex_Wait_Count, 1 );
               D_DEBUG_AT( Direct_Futex, "## ## WAIT FOR --> %p <--  %d (<-%d) ## ## ## ## * %u\n", uaddr, *uaddr, val, count );
               break;

          case FUTEX_WAKE:
               count = D_SYNC_ADD_AND_FETCH( &__Direct_Futex_Wake_Count, 1 );
               D_DEBUG_AT( Direct_Futex, "###   WAKE UP =--> %p <--= %d (->%d) ### ### ### * %u\n", uaddr, *uaddr, val, count );
               break;

          default:
               D_DEBUG_AT( Direct_Futex, "# #  UNKNOWN FUTEX OP  # #\n" );
     }

     ret = syscall( __NR_futex, uaddr, op, val, timeout, uaddr2, val3 );
     if (ret < 0)
          return errno2result( errno );

     return DR_OK;
}

