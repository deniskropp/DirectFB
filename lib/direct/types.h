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

#ifndef __DIRECT__TYPES_H__
#define __DIRECT__TYPES_H__

#include <dfb_types.h>


#include <directfb.h>
typedef DFBResult DirectResult;    /* FIXME */

/* can be removed if directfb.h is no longer included */
#ifdef __APPLE__
#undef main
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif


#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#ifndef NO_WARN_STDBOOL
#warning \
     bool definition herein is not 100% compliant,          \
     add AC_CHECK_HEADERS(stdbool.h) to your configure.in   \
     or define HAVE_STDBOOL_H or NO_WARN_STDBOOL.
#endif
#ifndef false
#define false (0)
#endif
#ifndef true
#define true (!false)
#endif
typedef __u8 bool;
#endif

typedef __u32 unichar;

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

