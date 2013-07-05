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

#include <asm/page.h>

#include <linux/futex.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>

#include <direct/atomic.h>
#include <direct/debug.h>
#include <direct/system.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Futex, "Direct/Futex", "Direct Futex" );

/**********************************************************************************************************************/

long
direct_pagesize( void )
{
     return PAGE_SIZE;
}

unsigned long
direct_page_align( unsigned long value )
{
     unsigned long mask = PAGE_SIZE - 1;

     return (value + mask) & ~mask;
}

/**********************************************************************************************************************/

pid_t
direct_getpid( void )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION( 2, 6, 18 )    // FIXME: which version?
     return task_tgid_vnr(current);
#else
     return current->pid;
#endif
}

__attribute__((no_instrument_function))
pid_t
direct_gettid( void )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION( 2, 6, 18 )    // FIXME: which version?
     return task_pid_vnr(current);
#else
     return current->tgid;
#endif
}

/**********************************************************************************************************************/

DirectResult
direct_tgkill( int tgid, int tid, int sig )
{
     int ret;

     D_UNIMPLEMENTED();
     return DR_UNIMPLEMENTED;
//     sys_rt_sigqueueinfo( direct_getpid(), SIGKILL, &info );
//     ret = sys_tgkill( tgid, tid, sig );
     switch (ret) {
          case -ESRCH:
               return DR_TASK_NOT_FOUND;

          default:
               return errno2result( -ret );

          case 0:
               break;
     }

     return DR_OK;
}

void
direct_trap( const char *domain, int sig )
{
//     sigset_t set;

     //D_DEBUG( "Direct/%s: Raising SIGTRAP...\n", domain );

     //direct_tgkill( direct_getpid(), direct_gettid(), sig );

     //D_DEBUG( "Direct/%s: ...tgkill on ourself returned, maybe blocked, trying sigsuspend()...\n", domain );

//     sigemptyset( &set );
//     sigsuspend( &set );

//     D_DEBUG( "Direct/%s: ...still running, calling _exit()!\n", domain );

//     _exit( DR_BUG );

     BUG();
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

     ret = -ENOSYS;//sys_futex( uaddr, op, val, (void*) timeout, uaddr2, val3 );
     if (ret < 0) {
          schedule_timeout_uninterruptible( 1 );  // FIXME
          return errno2result( -ret );
     }

     return DR_OK;
}

