/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/build.h>

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

/* makes it possible to prevent definition of "direct" standard types */
#ifndef __DIRECT__STDTYPES__
#define __DIRECT__STDTYPES__
#ifdef USE_KOS

#include <sys/types.h>

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef sint8 s8;
typedef sint16 s16;
typedef sint32 s32;
typedef sint64 s64;

#else

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#endif
#endif /* __DIRECT__STDTYPES__ */


typedef enum {
     DR_OK = 0x00000000, /* No error occured. */
     DR_FAILURE,         /* A general or unknown error occured. */
     DR_INIT,            /* A general initialization error occured. */
     DR_BUG,             /* Internal bug or inconsistency has been detected. */
     DR_DEAD,            /* Interface has a zero reference counter (available in debug mode). */
     DR_UNSUPPORTED,     /* The requested operation or an argument is (currently) not supported. */
     DR_UNIMPLEMENTED,   /* The requested operation is not implemented, yet. */
     DR_ACCESSDENIED,    /* Access to the resource is denied. */
     DR_INVAREA,         /* An invalid area has been specified or detected. */
     DR_INVARG,          /* An invalid argument has been specified. */
     DR_NOLOCALMEMORY,   /* There's not enough local system memory. */
     DR_NOSHAREDMEMORY,  /* There's not enough shared system memory. */
     DR_LOCKED,          /* The resource is (already) locked. */
     DR_BUFFEREMPTY,     /* The buffer is empty. */
     DR_FILENOTFOUND,    /* The specified file has not been found. */
     DR_IO,              /* A general I/O error occured. */
     DR_BUSY,            /* The resource or device is busy. */
     DR_NOIMPL,          /* No implementation for this interface or content type has been found. */
     DR_TIMEOUT,         /* The operation timed out. */
     DR_THIZNULL,        /* 'thiz' pointer is NULL. */
     DR_IDNOTFOUND,      /* No resource has been found by the specified id. */
     DR_DESTROYED,       /* The requested object has been destroyed. */
     DR_FUSION,          /* Internal fusion error detected, most likely related to IPC resources. */
     DR_BUFFERTOOLARGE,  /* Buffer is too large. */
     DR_INTERRUPTED,     /* The operation has been interrupted. */
     DR_NOCONTEXT,       /* No context available. */
     DR_TEMPUNAVAIL,     /* Temporarily unavailable. */
     DR_LIMITEXCEEDED,   /* Attempted to exceed limit, i.e. any kind of maximum size, count etc. */
     DR_NOSUCHMETHOD,    /* Requested method is not known. */
     DR_NOSUCHINSTANCE,  /* Requested instance is not known. */
     DR_ITEMNOTFOUND,    /* No such item found. */
     DR_VERSIONMISMATCH, /* Some versions didn't match. */
     DR_EOF,             /* Reached end of file. */
     DR_SUSPENDED,       /* The requested object is suspended. */
     DR_INCOMPLETE,      /* The operation has been executed, but not completely. */
     DR_NOCORE           /* Core part not available. */
} DirectResult;

/*
 * Generate result code base for API 'A','B','C', e.g. 'D','F','B'.
 */
#define D_RESULT_TYPE_BASE( a,b,c )     ((((unsigned)(a)&0x7f) * 0x02000000) + \
                                         (((unsigned)(b)&0x7f) * 0x00040000) + \
                                         (((unsigned)(c)&0x7f) * 0x00000800))

/*
 * Generate result code maximum for API 'A','B','C', e.g. 'D','F','B'.
 */
#define D_RESULT_TYPE_MAX( a,b,c )      (D_RESULT_TYPE_BASE(a,b,c) + 0x7ff)

/*
 * Check if given result code belongs to API 'A','B','C', e.g. 'D','F','B'.
 */
#define D_RESULT_TYPE_IS( code,a,b,c )  ((code) >= D_RESULT_TYPE_BASE(a,b,c) && (code) <= D_RESULT_TYPE_MAX(a,b,c))


/*
 * Return value of enumeration callbacks
 */
typedef enum {
     DENUM_OK       = 0x00000000,  /* Proceed with enumeration */
     DENUM_CANCEL   = 0x00000001   /* Cancel enumeration */
} DirectEnumerationResult;


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

