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

#ifndef __DIRECT__LOG_H__
#define __DIRECT__LOG_H__

#include <direct/types.h>
#include <direct/messages.h>


typedef enum {
     DLT_STDERR,    /* Simply print out log on stderr. */
     DLT_FILE,      /* Write log into a file. */
     DLT_UDP        /* Send out log via UDP. */
} DirectLogType;


/*
 * Creates a logging facility.
 *
 * For each 'type' the 'param' has a different meaning:
 *   DLT_STDERR     ignored (leave NULL)
 *   DLT_FILE       file name
 *   DLT_UDP        <ip>:<port>
 */
DirectResult direct_log_create     ( DirectLogType     type,
                                     const char       *param,
                                     DirectLog       **ret_log );

/*
 * Destroys a logging facility.
 */
DirectResult direct_log_destroy    ( DirectLog        *log );

/*
 * Write to the log in a printf fashion.
 *
 * If log is NULL, the default log is used if it's valid,
 * otherwise stderr is used a fallback until now.
 */
DirectResult direct_log_printf     ( DirectLog        *log,
                                     const char       *format, ... )  D_FORMAT_PRINTF(2);

/*
 * Set the default log that's used when no valid log is passed.
 */
DirectResult direct_log_set_default( DirectLog        *log );

/*
 * Locks a logging facility for non-intermixed output of multiple calls in multiple threads. Not mandatory.
 */
void         direct_log_lock       ( DirectLog        *log );

/*
 * Unlocks a logging facility.
 */
void         direct_log_unlock     ( DirectLog        *log );

/*
 * Returns the default log.
 */
DirectLog   *direct_log_default( void );

#endif
