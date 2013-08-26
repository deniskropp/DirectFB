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



#ifndef __EGL__RESULT_H__
#define __EGL__RESULT_H__

#include <direct/types.h>

typedef enum {
     EGL_OK              = DR_OK,                 /* No error occured. */
     EGL_FAILURE         = DR_FAILURE,            /* A general or unknown error occured. */
     EGL_INIT            = DR_INIT,               /* A general initialization error occured. */
     EGL_BUG             = DR_BUG,                /* Internal bug or inconsistency has been detected. */
     EGL_DEAD            = DR_DEAD,               /* Interface has a zero reference counter (available in debug mode). */
     EGL_UNSUPPORTED     = DR_UNSUPPORTED,        /* The requested operation or an argument is (currently) not supported. */
     EGL_UNIMPLEMENTED   = DR_UNIMPLEMENTED,      /* The requested operation is not implemented, yet. */
     EGL_ACCESSDENIED    = DR_ACCESSDENIED,       /* Access to the resource is denied. */
     EGL_INVAREA         = DR_INVAREA,            /* An invalid area has been specified or detected. */
     EGL_INVARG          = DR_INVARG,             /* An invalid argument has been specified. */
     EGL_NOSYSTEMMEMORY  = DR_NOLOCALMEMORY,      /* There's not enough system memory. */
     EGL_NOSHAREDMEMORY  = DR_NOSHAREDMEMORY,     /* There's not enough shared memory. */
     EGL_LOCKED          = DR_LOCKED,             /* The resource is (already) locked. */
     EGL_BUFFEREMPTY     = DR_BUFFEREMPTY,        /* The buffer is empty. */
     EGL_FILENOTFOUND    = DR_FILENOTFOUND,       /* The specified file has not been found. */
     EGL_IO              = DR_IO,                 /* A general I/O error occured. */
     EGL_BUSY            = DR_BUSY,               /* The resource or device is busy. */
     EGL_NOIMPL          = DR_NOIMPL,             /* No implementation for this interface or content type has been found. */
     EGL_TIMEOUT         = DR_TIMEOUT,            /* The operation timed out. */
     EGL_THIZNULL        = DR_THIZNULL,           /* 'thiz' pointer is NULL. */
     EGL_IDNOTFOUND      = DR_IDNOTFOUND,         /* No resource has been found by the specified id. */
     EGL_DESTROYED       = DR_DESTROYED,          /* The underlying object (e.g. a window or surface) has been destroyed. */
     EGL_FUSION          = DR_FUSION,             /* Internal fusion error detected, most likely related to IPC resources. */
     EGL_BUFFERTOOLARGE  = DR_BUFFERTOOLARGE,     /* Buffer is too large. */
     EGL_INTERRUPTED     = DR_INTERRUPTED,        /* The operation has been interrupted. */
     EGL_NOCONTEXT       = DR_NOCONTEXT,          /* No context available. */
     EGL_TEMPUNAVAIL     = DR_TEMPUNAVAIL,        /* Temporarily unavailable. */
     EGL_LIMITEXCEEDED   = DR_LIMITEXCEEDED,      /* Attempted to exceed limit, i.e. any kind of maximum size, count etc. */
     EGL_NOSUCHMETHOD    = DR_NOSUCHMETHOD,       /* Requested method is not known, e.g. to remote site. */
     EGL_NOSUCHINSTANCE  = DR_NOSUCHINSTANCE,     /* Requested instance is not known, e.g. to remote site. */
     EGL_ITEMNOTFOUND    = DR_ITEMNOTFOUND,       /* No such item found. */
     EGL_VERSIONMISMATCH = DR_VERSIONMISMATCH,    /* Some versions didn't match. */
     EGL_EOF             = DR_EOF,                /* Reached end of file. */
     EGL_SUSPENDED       = DR_SUSPENDED,          /* The requested object is suspended. */
     EGL_INCOMPLETE      = DR_INCOMPLETE,         /* The operation has been executed, but not completely. */
     EGL_NOCORE          = DR_NOCORE,             /* Core part not available. */

     /*
      * DirectFB specific result codes starting at (after) this offset
      */
     EGL__RESULT_BASE    = D_RESULT_TYPE_CODE_BASE( 'E','G','L','1' ),

     EGL_NOVIDEOMEMORY,  /* There's not enough video memory. */
     EGL_MISSINGFONT,    /* No font has been set. */
     EGL_MISSINGIMAGE,   /* No image has been set. */
     EGL_NOALLOCATION,   /* No allocation. */
     EGL_NOBUFFER,       /* No buffer. */

     EGL__RESULT_END
} EGLResult;


#endif
