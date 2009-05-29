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

#ifndef __DIRECT__CONF_H__
#define __DIRECT__CONF_H__


#include <direct/messages.h>

#ifndef __LINUX__
#include <sys/signal.h>
#endif

typedef enum {
     DCFL_NONE,     /* None is fatal. */
     DCFL_ASSERT,   /* ASSERT is fatal. */
     DCFL_ASSUME    /* ASSERT and ASSUME are fatal. */
} DirectConfigFatalLevel;

typedef enum {
     DCTS_OTHER,
     DCTS_FIFO,
     DCTS_RR
} DirectConfigThreadScheduler;

struct __D_DirectConfig {
     DirectMessageType             quiet;
     bool                          debug;
     bool                          trace;

     char                         *memcpy;            /* Don't probe for memcpy routines to save a lot of
                                                         startup time. Use this one instead if it's set. */

     char                        **disable_module;    /* Never load these modules. */
     char                         *module_dir;        /* module dir override */

     bool                          sighandler;
     sigset_t                      dont_catch;        /* don't catch these signals */

     DirectLog                    *log;

     DirectConfigFatalLevel        fatal;
     
     bool                          debugmem;

     bool                          thread_block_signals;

     bool                          fatal_break;        /* Should D_BREAK() cause a trap? */

     int                           thread_priority;
     DirectConfigThreadScheduler   thread_scheduler;
     int                           thread_stack_size;
     int                           thread_priority_scale;
};

extern DirectConfig *direct_config;

extern const char   *direct_config_usage;


DirectResult direct_config_set( const char *name, const char *value );


#endif

