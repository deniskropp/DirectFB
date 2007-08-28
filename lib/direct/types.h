/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __DIRECT__TYPES_H__
#define __DIRECT__TYPES_H__

#include <dfb_types.h>

#include <directfb.h>

#include <direct/build.h>

typedef DFBResult DirectResult;    /* FIXME */

/* can be removed if directfb.h is no longer included */
#ifdef __APPLE__
#undef main
#endif

#if DIRECT_BUILD_STDBOOL

#include <stdbool.h>

#else

#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#ifndef false
#define false (0)
#endif
#ifndef true
#define true (!false)
#endif
typedef u8 bool;
#endif

#endif /* DIRECT_BUILD_STDBOOL */

typedef u32 unichar;

typedef struct __D_DirectCleanupHandler      DirectCleanupHandler;
typedef struct __D_DirectConfig              DirectConfig;
typedef struct __D_DirectHash                DirectHash;
typedef struct __D_DirectLink                DirectLink;
typedef struct __D_DirectLog                 DirectLog;
typedef struct __D_DirectModuleDir           DirectModuleDir;
typedef struct __D_DirectModuleEntry         DirectModuleEntry;
typedef struct __D_DirectSerial              DirectSerial;
typedef struct __D_DirectSignalHandler       DirectSignalHandler;
typedef struct __D_DirectStream              DirectStream;
typedef struct __D_DirectTraceBuffer         DirectTraceBuffer;
typedef struct __D_DirectTree                DirectTree;
typedef struct __D_DirectThread              DirectThread;
typedef struct __D_DirectThreadInitHandler   DirectThreadInitHandler;

#endif

