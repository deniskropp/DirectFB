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

#ifndef __DIRECT__OS__LOG_H__
#define __DIRECT__OS__LOG_H__

#include <direct/os/mutex.h>

/**********************************************************************************************************************/

typedef enum {
     DLT_STDERR,    /* Simply print out log on stderr or comparable, e.g. using printk. */
     DLT_FILE,      /* Write log into a file. */
     DLT_UDP        /* Send out log via UDP. */
} DirectLogType;

/**********************************************************************************************************************/

typedef DirectResult (*DirectLogWriteFunc)    ( DirectLog  *log,
                                                const char *buffer,
                                                size_t      bytes );

typedef DirectResult (*DirectLogFlushFunc)    ( DirectLog  *log,
                                                bool        sync );

typedef DirectResult (*DirectLogSetBufferFunc)( DirectLog  *log,
                                                char       *buffer,
                                                size_t      bytes );

/**********************************************************************************************************************/

struct __D_DirectLog {
     int                     magic;

     DirectLogType           type;

     DirectMutex             lock;

     void                   *data;

     DirectLogWriteFunc      write;
     DirectLogFlushFunc      flush;
     DirectLogSetBufferFunc  set_buffer;
};

/**********************************************************************************************************************/

/*
 * Initializes a logging facility.
 *
 * For each 'log->type' the 'param' has a different meaning:
 *   DLT_STDERR     ignored (leave NULL)
 *   DLT_FILE       file name
 *   DLT_UDP        <ip>:<port>
 *
 * Implementation may set 'log->data' and should at least provide 'log->write' callback!
 */
DirectResult direct_log_init  ( DirectLog  *log,
                                const char *param );

/*
 * Destroys a logging facility.
 */
DirectResult direct_log_deinit( DirectLog  *log );

#endif
