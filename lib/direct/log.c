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
#include <arpa/inet.h>

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

     fd = open( filename, O_WRONLY | O_CREAT, 0644 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not open '%s' for writing!\n", filename );
          return ret;
     }

     log->fd = fd;

     return DFB_OK;
}

static DirectResult
parse_host_addr_port( const char     *hostport,
                      struct in_addr *ret_addr,
                      unsigned int   *ret_port )
{
     int   i;

     int   size = strlen( hostport ) + 1;
     char  buf[size];

     char *hoststr = buf;
     char *portstr = NULL;
     char *end;

     struct in_addr addr;
     unsigned int   port;

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

     port = strtoul( portstr, &end, 10 );

     if (end && *end) {
          D_ERROR( "Direct/Log: Parse error in port number '%s'!\n", portstr );
          return DFB_INVARG;
     }

     if (!inet_aton( hoststr, &addr )) {
          struct hostent *host;

          D_INFO( "Direct/Log: Looking up host '%s'...\n", hoststr );

          /* TODO: use getaddrinfo(3) and support IPv6 */
          host = gethostbyname( hoststr );
          if (!host) {
               switch (h_errno) {
                    case HOST_NOT_FOUND:
                         D_ERROR( "Direct/Log: Host not found!\n" );
                         return DFB_FAILURE;

                    case NO_ADDRESS:
                         D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                         return DFB_FAILURE;

                    case NO_RECOVERY:
                         D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                         return DFB_FAILURE;

                    case TRY_AGAIN:
                         D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                         return DFB_TEMPUNAVAIL;
               }

               D_ERROR( "Direct/Log: Unknown error occured!?\n" );
               return DFB_FAILURE;
          }

          if (host->h_addrtype != AF_INET || host->h_length != 4) {
               D_ERROR( "Direct/Log: Not an IPv4 address!\n" );
               return DFB_FAILURE;
          }

          memcpy( &addr, host->h_addr_list[0], sizeof(addr) );
     }

     *ret_addr = addr;
     *ret_port = port;

     return DFB_OK;
}

static DirectResult
init_udp( DirectLog  *log,
          const char *hostport )
{
     DirectResult       ret;
     int                fd;
     unsigned int       port;
     struct in_addr     addr;
     struct sockaddr_in sock_addr;

     fd = socket( PF_INET, SOCK_DGRAM, 0 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not create a UDP socket!\n" );
          return ret;
     }

     ret = parse_host_addr_port( hostport, &addr, &port );
     if (ret) {
          close( fd );
          return ret;
     }

     sock_addr.sin_family = AF_INET;
     sock_addr.sin_addr   = addr;
     sock_addr.sin_port   = htons( port );

     if (connect( fd, &sock_addr, sizeof(sock_addr) )) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not connect UDP socket to '%s'!\n", hostport );
          return ret;
     }

     log->fd = fd;

     return DFB_OK;
}

