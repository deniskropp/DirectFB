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

#ifndef __DIRECT__OS__SYSTEM_H__
#define __DIRECT__OS__SYSTEM_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

/*
 * Mainly special system calls...
 */

long          DIRECT_API  direct_pagesize( void );

unsigned long DIRECT_API  direct_page_align( unsigned long value );

pid_t         DIRECT_API  direct_getpid( void );
pid_t         DIRECT_API  direct_gettid( void );

/* May return DR_TASK_NOT_FOUND */
DirectResult  DIRECT_API  direct_tgkill( int tgid, int tid, int sig );

/* shall not return! */
void          DIRECT_API  direct_trap( const char *domain, int sig );

DirectResult  DIRECT_API  direct_kill( pid_t pid, int sig );
void          DIRECT_API  direct_sync( void );

DirectResult  DIRECT_API  direct_socketpair( int __domain, int __type, int __protocol, int __fds[2] );



DirectResult  DIRECT_API  direct_sigprocmask( int __how, const sigset_t *__set, sigset_t *__oset );



uid_t         DIRECT_API  direct_getuid( void );
uid_t         DIRECT_API  direct_geteuid( void );

char          DIRECT_API *direct_getenv( const char *name );


DirectResult  DIRECT_API  direct_futex( int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3 );

#define FUTEX_WAIT              0
#define FUTEX_WAKE              1


#endif

