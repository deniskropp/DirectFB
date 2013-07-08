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



#ifndef __DIRECT__OS__WIN32__TYPES_H__
#define __DIRECT__OS__WIN32__TYPES_H__


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include <ctype.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

#include <sys/types.h>

#include <WinSock2.h>
#include <ws2tcpip.h>

#include <windows.h>


#ifndef __cplusplus
#define bool int
#define true 1
#define false 0
#endif

typedef unsigned int       unichar;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;

typedef unsigned int       sigset_t;
typedef int                pid_t;
typedef int                uid_t;
typedef int                gid_t;
typedef int                mode_t;
typedef long long          ssize_t;

typedef unsigned int       in_addr_t;

/*
struct timespec {
     int tv_sec;
     int tv_nsec;
};
*/

#define __inline__ __inline
#define D_UNUSED //__attribute__((unused))
#define __dfb_no_instrument_function__ //__attribute__((no_instrument_function))
#define __constructor__ //__attribute__((constructor))
#define __destructor__ //__attribute__((destructor))
#define __typeof__(x) void*
#define __func__ __FUNCTION__

#define _ZD "%d"
#define _ZU "%u"
#define _ZUn(x) "%" #x "u"


#define SIGTRAP      5
#define SIGKILL      9
#define SIGTERM     15

#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2

#define sigaddset(set,sig)  do {} while (0)

#define PF_LOCAL    0

#define O_NONBLOCK  0


#endif

