/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <netdb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/log.h>
#include <direct/util.h>


struct __D_DirectLog {
     int            magic;

     DirectLogType  type;

     int            fd;
};

static DirectLog *default_log;

/**********************************************************************************************************************/

static DirectResult init_stderr( DirectLog  *log );

static DirectResult init_file  ( DirectLog  *log,
                                 const char *filename );

static DirectResult init_udp   ( DirectLog  *log,
                                 const char *hostport );

/**********************************************************************************************************************/

DirectResult
direct_log_create( DirectLogType   type,
                   const char     *param,
                   DirectLog     **ret_log )
{
     DirectResult  ret = DFB_INVARG;
     DirectLog    *log;

     log = D_CALLOC( 1, sizeof(DirectLog) );
     if (!log)
          return D_OOM();

     log->type = type;

     switch (type) {
          case DLT_STDERR:
               ret = init_stderr( log );
               break;

          case DLT_FILE:
               ret = init_file( log, param );
               break;

          case DLT_UDP:
               ret = init_udp( log, param );
               break;
     }

     if (ret)
          D_FREE( log );
     else {
          D_MAGIC_SET( log, DirectLog );

          *ret_log = log;
     }

     return ret;
}

DirectResult
direct_log_destroy( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     if (default_log == log)
          default_log = NULL;

     close( log->fd );

     D_MAGIC_CLEAR( log );

     D_FREE( log );

     return DFB_OK;
}

__attribute__((no_instrument_function))
DirectResult
direct_log_printf( DirectLog  *log,
                   const char *format, ... )
{
     va_list args;

     /*
      * Don't use D_MAGIC_ASSERT or any other
      * macros/functions that might cause an endless loop.
      */

     va_start( args, format );

     /* Use the default log if passed log is invalid. */
     if (!log || log->magic != D_MAGIC("DirectLog"))
          log = default_log;

     /* Write to stderr as a fallback if default is invalid, too. */
     if (!log || log->magic != D_MAGIC("DirectLog")) {
          vfprintf( stderr, format, args );
     }
     else {
          int  len;
          char buf[512];

          len = vsnprintf( buf, sizeof(buf), format, args );

          write( log->fd, buf, len );
     }

     va_end( args );

     return DFB_OK;
}

DirectResult
direct_log_set_default( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     default_log = log;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DirectResult
init_stderr( DirectLog *log )
{
     log->fd = dup( fileno( stderr ) );

     return DFB_OK;
}

static DirectResult
init_file( DirectLog  *log,
           const char *filename )
{
     DirectResult ret;
     int          fd;

     fd = open( filename, O_WRONLY | O_CREAT | O_APPEND, 0644 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not open '%s' for writing!\n", filename );
          return ret;
     }

     log->fd = fd;

     return DFB_OK;
}

static DirectResult
parse_host_addr( const char       *hostport,
                 struct addrinfo **ret_addr )
{
     int   i, ret;
     
     int   size = strlen( hostport ) + 1;
     char  buf[size];
     
     char *hoststr = buf;
     char *portstr = NULL;
     char *end;

     struct addrinfo hints;

     memcpy( buf, hostport, size );

     for (i=0; i<size; i++) {
          if (buf[i] == ':') {
               buf[i]  = 0;
               portstr = &buf[i+1];

               break;
          }
     }

     if (!portstr) {
          D_ERROR( "Direct/Log: Parse error in '%s' that should be '<host>:<port>'!\n", hostport );
          return DFB_INVARG;
     }

     strtoul( portstr, &end, 10 );
     if (end && *end) {
          D_ERROR( "Direct/Log: Parse error in port number '%s'!\n", portstr );
          return DFB_INVARG;
     }
     
     memset( &hints, 0, sizeof(hints) );
     hints.ai_socktype = SOCK_DGRAM;
     hints.ai_family   = PF_UNSPEC;
     
     ret = getaddrinfo( hoststr, portstr, &hints, ret_addr );
     if (ret) {
          switch (ret) {
               case EAI_FAMILY:
                    D_ERROR( "Direct/Log: Unsupported address family!\n" );
                    return DFB_UNSUPPORTED;
               
               case EAI_SOCKTYPE:
                    D_ERROR( "Direct/Log: Unsupported socket type!\n" );
                    return DFB_UNSUPPORTED;
               
               case EAI_NONAME:
                    D_ERROR( "Direct/Log: Host not found!\n" );
                    return DFB_FAILURE;
                    
               case EAI_SERVICE:
                    D_ERROR( "Direct/Log: Port %s is unreachable!\n", portstr );
                    return DFB_FAILURE;
               
               case EAI_ADDRFAMILY:
               case EAI_NODATA:
                    D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                    return DFB_FAILURE;
                    
               case EAI_MEMORY:
                    return D_OOM();

               case EAI_FAIL:
                    D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                    return DFB_FAILURE;

               case EAI_AGAIN:
                    D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                    return DFB_TEMPUNAVAIL;
                    
               default:
                    D_ERROR( "Direct/Log: Unknown error occured!?\n" );
                    return DFB_FAILURE;
          }
     }

     return DFB_OK;
}

static DirectResult
init_udp( DirectLog  *log,
          const char *hostport )
{
     DirectResult     ret;
     int              fd;
     struct addrinfo *addr;
     
     ret = parse_host_addr( hostport, &addr );
     if (ret)
          return ret;

     fd = socket( addr->ai_family, SOCK_DGRAM, 0 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not create a UDP socket!\n" );
          freeaddrinfo( addr );
          return ret;
     }

     ret = connect( fd, addr->ai_addr, addr->ai_addrlen );
     freeaddrinfo( addr );
     
     if (ret) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not connect UDP socket to '%s'!\n", hostport );
          close( fd );
          return ret;
     }

     log->fd = fd;

     return DFB_OK;
}
