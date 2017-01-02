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



#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/log.h>
#include <direct/print.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <netdb.h>

#ifdef ANDROID_NDK
#include <android/log.h>
#endif

/**********************************************************************************************************************/

static DirectResult init_stderr( DirectLog  *log );

static DirectResult init_file  ( DirectLog  *log,
                                 const char *filename );

static DirectResult init_udp   ( DirectLog  *log,
                                 const char *hostport );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult
direct_log_init( DirectLog  *log,
                 const char *param )
{
     switch (log->type) {
          case DLT_STDERR:
               return init_stderr( log );

          case DLT_FILE:
               return init_file( log, param );

          case DLT_UDP:
               return init_udp( log, param );

          default:
               break;
     }

     return DR_UNSUPPORTED;
}

DirectResult
direct_log_deinit( DirectLog *log )
{
     close( (long) log->data );

     return DR_OK;
}

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static DirectResult
common_log_write( DirectLog  *log,
                  const char *buffer,
                  size_t      bytes )
{
     ssize_t ret;

     ret = write( (long) log->data, buffer, bytes );
     if (ret < 0)
          perror( "write() to log failed" );

#ifdef ANDROID_NDK
__android_log_print( ANDROID_LOG_INFO, "android-dfb", "%s", buffer );
#endif
     return DR_OK;
}

__attribute__((no_instrument_function))
static DirectResult
common_log_flush( DirectLog *log,
                  bool       sync )
{
     if (log->type == DLT_STDERR && fflush( stderr ))
          return errno2result( errno );

     if (sync) {
          if (fdatasync( (long) log->data ))
               return errno2result( errno );
     }

     return DR_OK;
}

__attribute__((no_instrument_function))
static DirectResult
stderr_log_write( DirectLog  *log,
                  const char *buffer,
                  size_t      bytes )
{
     size_t ret;

     ret = fwrite( buffer, bytes, 1, stderr );

     (void)ret;

#ifdef ANDROID_NDK
__android_log_print( ANDROID_LOG_INFO, "android-dfb", "%s", buffer );
#endif
     return DR_OK;
}

static DirectResult
stderr_log_set_buffer( DirectLog *log,
                       char      *buffer,
                       size_t     bytes )
{
     if (setvbuf( stderr, buffer, _IOLBF, bytes ))
          return errno2result( errno );

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
init_stderr( DirectLog *log )
{
     log->data = (void*)(long) dup( fileno( stderr ) );

     log->write      = stderr_log_write;
     log->flush      = common_log_flush;
     log->set_buffer = stderr_log_set_buffer;

     return DR_OK;
}

static DirectResult
init_file( DirectLog  *log,
           const char *filename )
{
     DirectResult ret;
     int          fd;

     fd = open( filename, O_WRONLY | O_CREAT | O_APPEND, 0664 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not open '%s' for writing!\n", filename );
          return ret;
     }

     log->data = (void*)(long) fd;

     log->write = common_log_write;
     log->flush = common_log_flush;

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
parse_host_addr( const char       *hostport,
                 struct addrinfo **ret_addr )
{
     int   i, ret;
     unsigned long int res;
     
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
          return DR_INVARG;
     }

     res = strtoul( portstr, &end, 10 );
     (void)res;

     if (end && *end) {
          D_ERROR( "Direct/Log: Parse error in port number '%s'!\n", portstr );
          return DR_INVARG;
     }
     
     memset( &hints, 0, sizeof(hints) );
     hints.ai_socktype = SOCK_DGRAM;
     hints.ai_family   = PF_UNSPEC;
     
     ret = getaddrinfo( hoststr, portstr, &hints, ret_addr );
     if (ret) {
          switch (ret) {
               case EAI_FAMILY:
                    D_ERROR( "Direct/Log: Unsupported address family!\n" );
                    return DR_UNSUPPORTED;
               
               case EAI_SOCKTYPE:
                    D_ERROR( "Direct/Log: Unsupported socket type!\n" );
                    return DR_UNSUPPORTED;
               
               case EAI_NONAME:
                    D_ERROR( "Direct/Log: Host not found!\n" );
                    return DR_FAILURE;
                    
               case EAI_SERVICE:
                    D_ERROR( "Direct/Log: Port %s is unreachable!\n", portstr );
                    return DR_FAILURE;

#ifdef EAI_ADDRFAMILY
               case EAI_ADDRFAMILY:
#endif
               case EAI_NODATA:
                    D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                    return DR_FAILURE;
                    
               case EAI_MEMORY:
                    return D_OOM();

               case EAI_FAIL:
                    D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                    return DR_FAILURE;

               case EAI_AGAIN:
                    D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                    return DR_TEMPUNAVAIL;
                    
               default:
                    D_ERROR( "Direct/Log: Unknown error occurred!?\n" );
                    return DR_FAILURE;
          }
     }

     return DR_OK;
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

     log->data = (void*)(long) fd;

     log->write = common_log_write;
     log->flush = common_log_flush;

     return DR_OK;
}

