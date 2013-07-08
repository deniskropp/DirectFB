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



#ifndef __DIRECT__BUILD_H__
#define __DIRECT__BUILD_H__

#define DIRECT_DISABLE_DEPRECATED  (1)

#define DIRECT_OS_LINUX_GNU_LIBC   (0)
#define DIRECT_OS_LINUX_KERNEL     (1)
#define DIRECT_OS_PSP              (2)
#define DIRECT_OS_WIN32            (3)


#define DIRECT_BUILD_DEBUG     (1)
#define DIRECT_BUILD_DEBUGS    (1)
#define DIRECT_BUILD_TRACE     (0)
#define DIRECT_BUILD_TEXT      (1)
#define DIRECT_BUILD_GETTID    (0)
#define DIRECT_BUILD_NETWORK   (0)
#define DIRECT_BUILD_STDBOOL   (0)
#define DIRECT_BUILD_DYNLOAD   (0)
#define DIRECT_BUILD_MULTICORE (0)
#define DIRECT_BUILD_OSTYPE    (DIRECT_OS_WIN32)



#if !DIRECT_BUILD_DEBUGS
#if defined(DIRECT_ENABLE_DEBUG) || defined(DIRECT_FORCE_DEBUG)
#define DIRECT_MINI_DEBUG
#endif
#undef DIRECT_ENABLE_DEBUG
#ifdef DIRECT_FORCE_DEBUG
#warning DIRECT_FORCE_DEBUG used with 'pure release' library headers.
#undef DIRECT_FORCE_DEBUG
#endif
#endif
 
#endif
