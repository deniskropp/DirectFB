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



#ifndef __DIRECT__OS__LINUX__GLIBC__TYPES_H__
#define __DIRECT__OS__LINUX__GLIBC__TYPES_H__


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <ctype.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>

#include <netinet/in.h>

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>


typedef unsigned int       unichar;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


#define __inline__                      inline
#define D_UNUSED                        __attribute__((unused))
#define __dfb_no_instrument_function__  __attribute__((no_instrument_function))
#define __constructor__                 __attribute__((constructor))
#define __destructor__                  __attribute__((destructor))

#ifndef __func__
#define __func__                        __FUNCTION__
#endif

#define _ZD "%zd"
#define _ZU "%zu"
#define _ZUn(x) "%" #x "zu"

/*
 * Define the bool type by including stdbool.h (preferably)...
 */
#if DIRECT_BUILD_STDBOOL
#  include <stdbool.h>
/*
 * ...or defining it ourself, if not using C++ or another definition
 */
#elif !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#  warning Fallback definition of bool using u8! Checking for 'flags & 0x100' or higher bits will be false :(
   typedef u8 bool;
#  ifndef false
#   define false (0)
#  endif
#  ifndef true
#   define true (!false)
#  endif
#endif /* DIRECT_BUILD_STDBOOL */


#if DIRECT_BUILD_DYNLOAD
#include <dlfcn.h>
#endif


#ifndef SIGUNUSED
#define SIGUNUSED 31
#endif


#endif

