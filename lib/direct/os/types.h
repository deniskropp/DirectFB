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

#ifndef __DIRECT__OS__TYPES_H__
#define __DIRECT__OS__TYPES_H__

#include <direct/build.h>

/**********************************************************************************************************************/

#endif


#if DIRECT_BUILD_OSTYPE == DIRECT_OS_LINUX_GNU_LIBC


#ifdef __DIRECT__OS__TYPES_H__
#include <direct/os/linux/glibc/types.h>
#endif

#ifdef __DIRECT__OS__FILESYSTEM_H__
#include <direct/os/linux/glibc/filesystem.h>
#endif

#ifdef __DIRECT__OS__MUTEX_H__
#include <direct/os/linux/glibc/mutex.h>
#endif

#ifdef __DIRECT__OS__THREAD_H__
#include <direct/os/linux/glibc/thread.h>
#endif

#ifdef __DIRECT__OS__WAITQUEUE_H__
#include <direct/os/linux/glibc/waitqueue.h>
#endif




#elif DIRECT_BUILD_OSTYPE == DIRECT_OS_LINUX_KERNEL


#ifdef __DIRECT__OS__TYPES_H__
#include <direct/os/linux/kernel/types.h>
#endif

#ifdef __DIRECT__OS__FILESYSTEM_H__
#include <direct/os/linux/kernel/filesystem.h>
#endif

#ifdef __DIRECT__OS__MUTEX_H__
#include <direct/os/linux/kernel/mutex.h>
#endif

#ifdef __DIRECT__OS__THREAD_H__
#include <direct/os/linux/kernel/thread.h>
#endif

#ifdef __DIRECT__OS__WAITQUEUE_H__
#include <direct/os/linux/kernel/waitqueue.h>
#endif

#elif DIRECT_BUILD_OSTYPE == DIRECT_OS_PSP

#ifdef __DIRECT__OS__TYPES_H__
#include <direct/os/psp/types.h>
#endif

#ifdef __DIRECT__OS__FILESYSTEM_H__
#include <direct/os/psp/filesystem.h>
#endif

#ifdef __DIRECT__OS__MUTEX_H__
#include <direct/os/psp/mutex.h>
#endif

#ifdef __DIRECT__OS__THREAD_H__
#include <direct/os/psp/thread.h>
#endif

#ifdef __DIRECT__OS__WAITQUEUE_H__
#include <direct/os/psp/waitqueue.h>
#endif


#elif DIRECT_BUILD_OSTYPE == DIRECT_OS_WIN32

#ifdef __DIRECT__OS__TYPES_H__
#include <direct/os/win32/types.h>
#endif

#ifdef __DIRECT__OS__FILESYSTEM_H__
#include <direct/os/win32/filesystem.h>
#endif

#ifdef __DIRECT__OS__MUTEX_H__
#include <direct/os/win32/mutex.h>
#endif

#ifdef __DIRECT__OS__THREAD_H__
#include <direct/os/win32/thread.h>
#endif

#ifdef __DIRECT__OS__WAITQUEUE_H__
#include <direct/os/win32/waitqueue.h>
#endif


#else
#error Unsupported OS type (DIRECT_BUILD_OSTYPE)!
#endif



#include <direct/types.h>

