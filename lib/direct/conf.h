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

#ifndef __DIRECT__CONF_H__
#define __DIRECT__CONF_H__


#include <direct/log_domain.h>


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

typedef enum {
     DMT_NONE           = 0x00000000, /* No message type. */

     DMT_BANNER         = 0x00000001, /* Startup banner. */
     DMT_INFO           = 0x00000002, /* Info messages. */
     DMT_WARNING        = 0x00000004, /* Warnings. */
     DMT_ERROR          = 0x00000008, /* Error messages: regular, with DFBResult, bugs,
                                         system call errors, dlopen errors */
     DMT_UNIMPLEMENTED  = 0x00000010, /* Messages notifying unimplemented functionality. */
     DMT_ONCE           = 0x00000020, /* One-shot messages .*/
     DMT_UNTESTED       = 0x00000040, /* Messages notifying unimplemented functionality. */

     DMT_ALL            = 0x0000007F  /* All types. */
} DirectMessageType;


struct __D_DirectConfig {
     DirectMessageType             quiet;

     DirectLogLevel                log_level;
     bool                          log_all;
     bool                          log_none;

     bool                          trace;

     char                         *memcpy;            /* Don't probe for memcpy routines to save a lot of
                                                         startup time. Use this one instead if it's set. */

     char                        **disable_module;    /* Never load these modules. */
     char                         *module_dir;        /* module dir override */

     bool                          sighandler;
     sigset_t                      dont_catch;        /* don't catch these signals */

     DirectLog                    *log;

     DirectConfigFatalLevel        fatal;

     // @deprecated / FIXME: maybe adapt?
     bool                          debug;

     bool                          debugmem;

     bool                          thread_block_signals;

     bool                          fatal_break;        /* Should D_BREAK() cause a trap? */

     int                           thread_priority;
     DirectConfigThreadScheduler   thread_scheduler;
     int                           thread_stack_size;
     int                           thread_priority_scale;

     char                        **default_interface_implementation_types;
     char                        **default_interface_implementation_names;
};

extern DirectConfig DIRECT_API *direct_config;

extern const char   DIRECT_API *direct_config_usage;

DirectResult        DIRECT_API  direct_config_set( const char *name, const char *value );

/* Retrieve all values set on option 'name'. */
/* Pass an array of char* pointers and number of pointers in 'num'. */
/* The actual returned number of values gets returned in 'ret_num' */
/* The returned option/values respect directfbrc, cmdline options and DFBARGS envvar. */
/* The returned pointers are not extra allocated so do not free them! */
DirectResult        DIRECT_API  direct_config_get( const char *name, char **values, const int values_len, int *ret_num );

/* Return the integer value for the last occurrance of the passed option's setting. */
/* Note that 0 is also retuned in case the passed option was not found ot set. */
long long           DIRECT_API  direct_config_get_int_value( const char *name );

void __D_conf_init( void );
void __D_conf_deinit( void );

#endif

