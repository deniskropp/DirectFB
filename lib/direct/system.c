/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <errno.h>
#include <unistd.h>

#include <direct/build.h>
#include <direct/system.h>

#if HAVE_ASM_PAGE_H
#include <asm/page.h>
#else
#define PAGE_SIZE   sysconf( _SC_PAGESIZE )
#endif


#if DIRECT_BUILD_GETTID && defined(HAVE_LINUX_UNISTD_H)
#include <linux/unistd.h>

#ifdef __NR_gettid
static inline _syscall0(pid_t,gettid)
#else
#warning __NR_gettid was not found in "linux/unistd.h", using getpid instead
#define gettid getpid
#endif

#else

#include <unistd.h>
#define gettid getpid

#endif


__attribute__((no_instrument_function))
pid_t
direct_gettid()
{
     return gettid();
}

long
direct_pagesize()
{
     return PAGE_SIZE;
}

