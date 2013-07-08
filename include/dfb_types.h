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



#ifndef __DFB_TYPES_H__
#define __DFB_TYPES_H__

#include <direct/types.h>

#ifdef WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DIRECTFB_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DIRECTFB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DIRECTFB_EXPORTS
#define DIRECTFB_API __declspec(dllexport)
#else
#define DIRECTFB_API __declspec(dllimport)
#endif
#else
#define DIRECTFB_API
#endif


#ifdef DIRECTFB_ENABLE_DEPRECATED
#define __u8   u8
#define __u16  u16
#define __u32  u32
#define __u64  u64
#define __s8   s8
#define __s16  s16
#define __s32  s32
#define __s64  s64
#endif


/*
 * Return code of all interface methods and most functions
 *
 * Whenever a method has to return any information, it is done via output parameters. These are pointers to
 * primitive types such as <i>int *ret_num</i>, enumerated types like <i>DFBBoolean *ret_enabled</i>, structures
 * as in <i>DFBDisplayLayerConfig *ret_config</i>, just <i>void **ret_data</i> or other types...
 */
typedef enum {
     /*
      * Aliases for backward compatibility and uniform look in DirectFB code
      */
     DFB_OK              = DR_OK,                 /* No error occured. */
     DFB_FAILURE         = DR_FAILURE,            /* A general or unknown error occured. */
     DFB_INIT            = DR_INIT,               /* A general initialization error occured. */
     DFB_BUG             = DR_BUG,                /* Internal bug or inconsistency has been detected. */
     DFB_DEAD            = DR_DEAD,               /* Interface has a zero reference counter (available in debug mode). */
     DFB_UNSUPPORTED     = DR_UNSUPPORTED,        /* The requested operation or an argument is (currently) not supported. */
     DFB_UNIMPLEMENTED   = DR_UNIMPLEMENTED,      /* The requested operation is not implemented, yet. */
     DFB_ACCESSDENIED    = DR_ACCESSDENIED,       /* Access to the resource is denied. */
     DFB_INVAREA         = DR_INVAREA,            /* An invalid area has been specified or detected. */
     DFB_INVARG          = DR_INVARG,             /* An invalid argument has been specified. */
     DFB_NOSYSTEMMEMORY  = DR_NOLOCALMEMORY,      /* There's not enough system memory. */
     DFB_NOSHAREDMEMORY  = DR_NOSHAREDMEMORY,     /* There's not enough shared memory. */
     DFB_LOCKED          = DR_LOCKED,             /* The resource is (already) locked. */
     DFB_BUFFEREMPTY     = DR_BUFFEREMPTY,        /* The buffer is empty. */
     DFB_FILENOTFOUND    = DR_FILENOTFOUND,       /* The specified file has not been found. */
     DFB_IO              = DR_IO,                 /* A general I/O error occured. */
     DFB_BUSY            = DR_BUSY,               /* The resource or device is busy. */
     DFB_NOIMPL          = DR_NOIMPL,             /* No implementation for this interface or content type has been found. */
     DFB_TIMEOUT         = DR_TIMEOUT,            /* The operation timed out. */
     DFB_THIZNULL        = DR_THIZNULL,           /* 'thiz' pointer is NULL. */
     DFB_IDNOTFOUND      = DR_IDNOTFOUND,         /* No resource has been found by the specified id. */
     DFB_DESTROYED       = DR_DESTROYED,          /* The underlying object (e.g. a window or surface) has been destroyed. */
     DFB_FUSION          = DR_FUSION,             /* Internal fusion error detected, most likely related to IPC resources. */
     DFB_BUFFERTOOLARGE  = DR_BUFFERTOOLARGE,     /* Buffer is too large. */
     DFB_INTERRUPTED     = DR_INTERRUPTED,        /* The operation has been interrupted. */
     DFB_NOCONTEXT       = DR_NOCONTEXT,          /* No context available. */
     DFB_TEMPUNAVAIL     = DR_TEMPUNAVAIL,        /* Temporarily unavailable. */
     DFB_LIMITEXCEEDED   = DR_LIMITEXCEEDED,      /* Attempted to exceed limit, i.e. any kind of maximum size, count etc. */
     DFB_NOSUCHMETHOD    = DR_NOSUCHMETHOD,       /* Requested method is not known, e.g. to remote site. */
     DFB_NOSUCHINSTANCE  = DR_NOSUCHINSTANCE,     /* Requested instance is not known, e.g. to remote site. */
     DFB_ITEMNOTFOUND    = DR_ITEMNOTFOUND,       /* No such item found. */
     DFB_VERSIONMISMATCH = DR_VERSIONMISMATCH,    /* Some versions didn't match. */
     DFB_EOF             = DR_EOF,                /* Reached end of file. */
     DFB_SUSPENDED       = DR_SUSPENDED,          /* The requested object is suspended. */
     DFB_INCOMPLETE      = DR_INCOMPLETE,         /* The operation has been executed, but not completely. */
     DFB_NOCORE          = DR_NOCORE,             /* Core part not available. */

     /*
      * DirectFB specific result codes starting at (after) this offset
      */
     DFB__RESULT_BASE    = D_RESULT_TYPE_CODE_BASE( 'D','F','B','1' ),

     DFB_NOVIDEOMEMORY,  /* There's not enough video memory. */
     DFB_MISSINGFONT,    /* No font has been set. */
     DFB_MISSINGIMAGE,   /* No image has been set. */
     DFB_NOALLOCATION,   /* No allocation. */
     DFB_NOBUFFER,       /* No buffer. */

     DFB__RESULT_END
} DFBResult;


#endif
