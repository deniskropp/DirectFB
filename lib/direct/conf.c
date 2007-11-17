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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <direct/conf.h>
#include <direct/mem.h>
#include <direct/util.h>


static DirectConfig config = {
     debug:                   false,
     trace:                   true,
     sighandler:              true,

     fatal:                   DCFL_ASSERT,
     thread_block_signals:    true
};

DirectConfig *direct_config       = &config;
const char   *direct_config_usage =
     "libdirect options:\n"
     "  memcpy=<method>                Skip memcpy() probing (help = show list)\n"
     "  [no-]quiet                     Text output (except debugging)?\n"
     "  [no-]debug                     Enable debug output\n"
     "  [no-]debugmem                  Enable memory allocation tracking\n"
     "  [no-]trace                     Enable stack trace support\n"
     "  log-file=<name>                Write all messages to a file\n"
     "  log-udp=<host>:<port>          Send all messages via UDP to host:port\n"
     "  fatal-level=<level>            Abort on NONE, ASSERT (default) or ASSUME (incl. assert)\n"
     "  dont-catch=<num>[[,<num>]...]  Don't catch these signals\n"
     "  [no-]sighandler                Enable signal handler\n"
     "  [no-]thread-block-signals      Block all signals in new threads?\n"
     "  disable-module=<module_name>   suppress loading this module\n"
     "\n";

/**********************************************************************************************************************/

DirectResult
direct_config_set( const char *name, const char *value )
{
     if (strcmp (name, "disable-module" ) == 0) {
          if (value) {
               int n = 0;

               while (direct_config->disable_module &&
                      direct_config->disable_module[n])
                    n++;

               direct_config->disable_module = D_REALLOC( direct_config->disable_module,
                                                          sizeof(char*) * (n + 2) );

               direct_config->disable_module[n] = D_STRDUP( value );
               direct_config->disable_module[n+1] = NULL;
          }
          else {
               D_ERROR("Direct/Config '%s': No module name specified!\n", name);
               return DFB_INVARG;
          }
     }
     else
          if (strcmp (name, "memcpy" ) == 0) {
          if (value) {
               if (direct_config->memcpy)
                    D_FREE( direct_config->memcpy );
               direct_config->memcpy = D_STRDUP( value );
          }
          else {
               D_ERROR("Direct/Config '%s': No method specified!\n", name);
               return DFB_INVARG;
          }
     }
     else
          if (strcmp (name, "quiet" ) == 0) {
          direct_config->quiet = true;
     }
     else
          if (strcmp (name, "no-quiet" ) == 0) {
          direct_config->quiet = false;
     }
     else
          if (strcmp (name, "debug" ) == 0) {
          if (value)
               direct_debug_config_domain( value, true );
          else
               direct_config->debug = true;
     }
     else
          if (strcmp (name, "no-debug" ) == 0) {
          if (value)
               direct_debug_config_domain( value, false );
          else
               direct_config->debug = false;
     }
     else
          if (strcmp (name, "debugmem" ) == 0) {
          direct_config->debugmem = true;
     }
     else
          if (strcmp (name, "no-debugmem" ) == 0) {
          direct_config->debugmem = false;
     }
     else
          if (strcmp (name, "trace" ) == 0) {
          direct_config->trace = true;
     }
     else
          if (strcmp (name, "no-trace" ) == 0) {
          direct_config->trace = false;
     }
     else
          if (strcmp (name, "log-file" ) == 0 || strcmp (name, "log-udp" ) == 0) {
          if (value) {
               DirectResult  ret;
               DirectLog    *log;

               ret = direct_log_create( strcmp(name,"log-udp") ? DLT_FILE : DLT_UDP, value, &log );
               if (ret)
                    return ret;

               if (direct_config->log)
                    direct_log_destroy( direct_config->log );

               direct_config->log = log;

               direct_log_set_default( log );
          }
          else {
               if (strcmp(name,"log-udp"))
                    D_ERROR("Direct/Config '%s': No file name specified!\n", name);
               else
                    D_ERROR("Direct/Config '%s': No host and port specified!\n", name);
               return DFB_INVARG;
          }
     }
     else
          if (strcmp (name, "fatal-level" ) == 0) {
          if (strcasecmp (value, "none" ) == 0) {
               direct_config->fatal = DCFL_NONE;
          }
          else
               if (strcasecmp (value, "assert" ) == 0) {
               direct_config->fatal = DCFL_ASSERT;
          }
          else
               if (strcasecmp (value, "assume" ) == 0) {
               direct_config->fatal = DCFL_ASSUME;
          }
          else {
               D_ERROR("Direct/Config '%s': Unknown level specified (use 'none', 'assert', 'assume')!\n", name);
               return DFB_INVARG;
          }
     }
     else
          if (strcmp (name, "sighandler" ) == 0) {
          direct_config->sighandler = true;
     }
     else
          if (strcmp (name, "no-sighandler" ) == 0) {
          direct_config->sighandler = false;
     }
     else
          if (strcmp (name, "dont-catch" ) == 0) {
          if (value) {
               char *signals   = D_STRDUP( value );
               char *p = NULL, *r, *s = signals;

               while ((r = strtok_r( s, ",", &p ))) {
                    char          *error;
                    unsigned long  signum;

                    direct_trim( &r );

                    signum = strtoul( r, &error, 10 );

                    if (*error) {
                         D_ERROR( "Direct/Config '%s': Error in number at '%s'!\n", name, error );
                         D_FREE( signals );
                         return DFB_INVARG;
                    }

                    sigaddset( &direct_config->dont_catch, signum );

                    s = NULL;
               }

               D_FREE( signals );
          }
          else {
               D_ERROR("Direct/Config '%s': No signals specified!\n", name);
               return DFB_INVARG;
          }
     }
     else
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

