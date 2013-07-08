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



#ifndef __DIRECT__OS__LINUX__KERNEL__TYPES_H__
#define __DIRECT__OS__LINUX__KERNEL__TYPES_H__

#include <linux/autoconf.h>
#include <linux/compiler.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/rusage.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/version.h>

#include <asm/fcntl.h>
#include <asm/signal.h>
#include <asm/string.h>

#include "quad.h"


#define SCHED_OTHER SCHED_NORMAL


#define strtoul                     simple_strtoul


#undef SA_SIGINFO   // FIXME



#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 19 )
#include <stdbool.h>
#endif


#endif

