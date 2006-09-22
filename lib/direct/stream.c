/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/debug.h>
#include <direct/stream.h>


struct __D_DirectStream {
     int                   magic;
     int                   ref;
     
     int                   fd;
     unsigned int          offset;
     int                   length;
     
     char                 *mime;

     /* cache for piped streams */
     void                 *cache;
     unsigned int          cache_size;

     /* remote streams data */
     struct {
          int              sd;
          
          char            *host;
          int              port;
          struct addrinfo *addr;
          
          char            *user;
          char            *pass;
          char            *auth;
          
          char            *path;
          
          int              redirects;
     } remote;

     DirectResult  (*wait)  ( DirectStream   *stream,
                              unsigned int    length,
                              struct timeval *tv );
     DirectResult  (*peek)  ( DirectStream *stream,
                              unsigned int  length,
                              int           offset,
                              void         *buf,
                              unsigned int *read_out );
     DirectResult  (*read)  ( DirectStream *stream,
                              unsigned int  length,
                              void         *buf, 
                              unsigned int *read_out );
     DirectResult  (*seek)  ( DirectStream *stream,
                              unsigned int  offset );
     void          (*close) ( DirectStream *stream );
};


#define NET_TIMEOUT         15
#define HTTP_PORT           80
#define FTP_PORT            21
#define HTTP_MAX_REDIRECTS  15

static DirectResult tcp_open ( DirectStream *stream, const char *filename );
static DirectResult udp_open ( DirectStream *stream, const char *filename );
static DirectResult http_open( DirectStream *stream, const char *filename );
static DirectResult ftp_open ( DirectStream *stream, const char *filename );
static DirectResult file_open( DirectStream *stream, const char *filename );


D_DEBUG_DOMAIN( Direct_Stream, "Direct/Stream", "Stream wrapper" );

/*****************************************************************************/

static inline char* trim( char *s )
{
     char *e;
     
     for (; isspace(*s); s++);
     
     e = s + strlen(s) - 1;
     for (; e > s && isspace(*e); *e-- = '\0');
     
     return s;
}

static void
parse_url( const char *url, char **ret_host, int *ret_port, 
           char **ret_user, char **ret_pass, char **ret_path )
{
     char *host = NULL;
     int   port = 0;
     char *user = NULL;
     char *pass = NULL;
     char *path;
     char *tmp;
     
     tmp = strchr( url, '/' );
     if (tmp) {
          host = alloca( tmp - url + 1 );
          memcpy( host, url, tmp - url );
          host[tmp-url] = '\0';
          path = tmp;
     } else {
          host = alloca( strlen( url ) + 1 );
          memcpy( host, url, strlen( url ) + 1 );
          path = "/";
     }

     tmp = strchr( host, '@' );
     if (tmp) {
          *tmp = '\0';
          pass = strchr( host, ':' );
          if (pass) {
               *pass = '\0';
               pass++;
          }
          user = host;
          host = tmp + 1;
     }

     tmp = strchr( host, ':' );
     if (tmp) {
          port = strtol( tmp+1, NULL, 10 );
          *tmp = '\0';
     }

     /* IPv6 variant (host within brackets) */
     if (*host == '[') {
          host++;
          tmp = strchr( host, ']' );
          if (tmp)
               *tmp = '\0';
     }
     
     if (ret_host)
          *ret_host = D_STRDUP( host );
     
     if (ret_port && port)
          *ret_port = port;

     if (ret_user && user)
          *ret_user = D_STRDUP( user );

     if (ret_pass && pass)
          *ret_pass = D_STRDUP( pass );

     if (ret_path)
          *ret_path = D_STRDUP( path );
}

static char*
base64_encode( const char *string )
{
     static char *enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789+/=";
     char        *str = (char*)string;
     char        *ret;
     char        *buf;
     int          len;

     len = strlen( string );
     buf = ret = D_MALLOC( ((len + 2) / 3 ) * 4 + 1 );
     if (!ret)
          return NULL;
     
     for (; len >= 3; len -= 3) {
          buf[0] = enc[((str[0] & 0xfc) >> 2)];
          buf[1] = enc[((str[0] & 0x03) << 4) | ((str[1] & 0xf0) >> 4)];
          buf[2] = enc[((str[1] & 0x0f) << 2) | ((str[2] & 0xc0) >> 6)];
          buf[3] = enc[((str[2] & 0x3f))];
          buf += 4;
          str += 3;
     }

     if (len > 0) {
          buf[0] = enc[(str[0] & 0xfc) >> 2];
          
          if (len > 1) {
               buf[1] = enc[((str[0] & 0x03) << 4) | ((str[1] & 0xf0) >> 4)];
               buf[2] = enc[((str[1] & 0x0f) << 2)];
          } else {
               buf[1] = enc[(str[0] & 0x03) << 4];
               buf[2] = '=';
          }

          buf[3] = '=';
          buf += 4;
     }

     *buf = '\0';

     return ret;
}

/*****************************************************************************/

static int
net_readl( int fd, char *buf, size_t size )
{
     fd_set         set;
     struct timeval tv;
     int            i;
     
     FD_ZERO( &set );
     FD_SET( fd, &set );

     for (i = 0; i < size-1; i++) {
          tv.tv_sec  = NET_TIMEOUT;
          tv.tv_usec = 0;
          select( fd+1, &set, NULL, NULL, &tv );
          
          if (recv( fd, buf+i, 1, 0 ) != 1)
               break;

          if (buf[i] == '\n') {
               if (i > 0 && buf[i-1] == '\r')
                    i--;
               break;
          }
     }

     buf[i] = '\0';
     
     return i;
}

static void
alarm_handler( int sig )
{
     return;
}

static DirectResult
net_wait( DirectStream   *stream,
          unsigned int    length,
          struct timeval *tv )
{
     DirectResult ret = DFB_OK;
     int          flags;
     char         buf[length];

     flags = fcntl( stream->fd, F_GETFL );

     if (tv) {
          struct itimerval t, ot;
          struct sigaction a, oa;
     
          /* FIXME: avoid using SIGALARM */
          
          if (tv->tv_sec || tv->tv_usec) {
               fcntl( stream->fd, F_SETFL, flags & ~O_NONBLOCK );
    
               memset( &a, 0, sizeof(a) );
               a.sa_handler = alarm_handler;
               a.sa_flags   = SA_RESETHAND | SA_RESTART;
               sigaction( SIGALRM, &a, &oa );
               
               t.it_interval.tv_sec  = 0;
               t.it_interval.tv_usec = 0;
               t.it_value            = *tv;
               setitimer( ITIMER_REAL, &t, &ot );
          }

          switch (recv( stream->fd, buf, length, MSG_PEEK | MSG_WAITALL )) {
               case 0:
                    ret = DFB_EOF;
                    break;
               case -1:
                    ret = (errno == EINTR) 
                          ? DFB_TIMEOUT : errno2result( errno );
                    break;
          }

          if (tv->tv_sec || tv->tv_usec) {
               setitimer( ITIMER_REAL, &ot, NULL );
               sigaction( SIGALRM, &oa, NULL );
               fcntl( stream->fd, F_SETFL, flags );
          }
     }
     else {
          fcntl( stream->fd, F_SETFL, flags & ~O_NONBLOCK );
          
          switch (recv( stream->fd, buf, length, MSG_PEEK | MSG_WAITALL )) {
               case 0:
                    ret = DFB_EOF;
                    break;
               case -1:
                    ret = errno2result( errno );
                    break;
          }

          fcntl( stream->fd, F_SETFL, flags );
     }

     return ret;
}

static DirectResult
net_peek( DirectStream *stream,
          unsigned int  length,
          int           offset,
          void         *buf,
          unsigned int *read_out )
{
     char *tmp;
     int   size;

     if (offset < 0)
          return DFB_UNSUPPORTED;

     tmp = alloca( length + offset );
     
     size = recv( stream->fd, tmp, length+offset, MSG_PEEK );
     switch (size) {
          case 0:
               return DFB_EOF;
          case -1:
               if (errno == EAGAIN)
                    return DFB_BUFFEREMPTY;
               return errno2result( errno );
          default:
               if (size < offset)
                    return DFB_BUFFEREMPTY;
               size -= offset;
               break;
     }

     direct_memcpy( buf, tmp+offset, size );

     if (read_out)
          *read_out = size;

     return DFB_OK;
}

static DirectResult
net_read( DirectStream *stream, 
          unsigned int  length,
          void         *buf,
          unsigned int *read_out )
{
     int size;
     
     size = recv( stream->fd, buf, length, 0 );
     switch (size) {
          case 0:
               return DFB_EOF;
          case -1:
               if (errno == EAGAIN)
                    return DFB_BUFFEREMPTY;
               return errno2result( errno );
     }

     stream->offset += size;
     
     if (read_out)
          *read_out = size;

     return DFB_OK;
}

static void
net_close( DirectStream *stream )
{
     if (stream->remote.host) {
          D_FREE( stream->remote.host );
          stream->remote.host = NULL;
     }

     if (stream->remote.user) {
          D_FREE( stream->remote.user );
          stream->remote.user = NULL;
     }

     if (stream->remote.pass) {
          D_FREE( stream->remote.pass );
          stream->remote.pass = NULL;
     }

     if (stream->remote.auth) {
          D_FREE( stream->remote.auth );
          stream->remote.auth = NULL;
     }

     if (stream->remote.path) {
          D_FREE( stream->remote.path );
          stream->remote.path = NULL;
     }
     
     if (stream->remote.addr) {
          freeaddrinfo( stream->remote.addr );
          stream->remote.addr = NULL;
     }

     if (stream->remote.sd > 0) {
          close( stream->remote.sd ); 
          stream->remote.sd = -1;
     }
     
     if (stream->mime) {
          D_FREE( stream->mime );
          stream->mime = NULL;
     }

     if (stream->fd > 0) {
          close( stream->fd );
          stream->fd = -1;
     }
}

static DirectResult
net_connect( struct addrinfo *addr, int type, int proto, int *ret_fd )
{
     DirectResult     ret = DFB_OK;
     int              fd  = -1;
     struct addrinfo *tmp;

     D_ASSERT( addr != NULL );
     D_ASSERT( ret_fd != NULL );

     for (tmp = addr; tmp; tmp = tmp->ai_next) {
          int err;
          
          fd = socket( tmp->ai_family, type, proto );
          if (fd < 0) {
               ret = errno2result( errno );
               D_DEBUG_AT( Direct_Stream,
                           "failed to create socket!\n\t->%s",
                           strerror(errno) );
               continue;
          }

          fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) | O_NONBLOCK );

          D_DEBUG_AT( Direct_Stream, 
                      "connecting to %s...\n", tmp->ai_canonname );
 
          err = connect( fd, tmp->ai_addr, tmp->ai_addrlen );
          if (err == 0 || errno == EINPROGRESS) {
               struct timeval t = { NET_TIMEOUT, 0 };
               fd_set         s;

               FD_ZERO( &s );
               FD_SET( fd, &s );
              
               err = select( fd+1, NULL, &s, NULL, &t );
               if (err < 1) {
                    D_DEBUG_AT( Direct_Stream, "...connection failed.\n" );
                    
                    close( fd );
                    fd = -1;

                    if (err == 0) {
                         ret = DFB_TIMEOUT;
                         continue;
                    } else {
                         ret = errno2result( errno );
                         break;
                    }
               }

               D_DEBUG_AT( Direct_Stream, "...connected.\n" );
               
               ret = DFB_OK;
               break;
          }
     }

     *ret_fd = fd;

     return ret;
}

/*****************************************************************************/

static DirectResult
tcp_open( DirectStream *stream, const char *filename )
{
     DirectResult    ret = DFB_OK; 
     struct addrinfo hints;
     char            port[16];
     
     parse_url( filename, 
                &stream->remote.host,
                &stream->remote.port,
                &stream->remote.user,
                &stream->remote.pass,
                &stream->remote.path );
     
     snprintf( port, sizeof(port), "%d", stream->remote.port );

     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = PF_UNSPEC;
     
     if (getaddrinfo( stream->remote.host, port,
                      &hints, &stream->remote.addr )) {
          D_ERROR( "Direct/Stream: "
                   "failed to resolve host '%s'!\n", stream->remote.host );
          net_close( stream );
          return DFB_FAILURE;
     }

     ret = net_connect( stream->remote.addr, 
                        SOCK_STREAM, IPPROTO_TCP, &stream->remote.sd );
     if (ret)
          return ret;

     stream->fd     = stream->remote.sd;
     stream->length = -1; 
     stream->wait   = net_wait;
     stream->peek   = net_peek;
     stream->read   = net_read;
     stream->close  = net_close;

     return ret;
}

/*****************************************************************************/

static DirectResult
http_seek( DirectStream *stream, unsigned int offset )
{
     DirectResult ret;
     int          status = 0;
     char         buf[1024]; 
     
     close( stream->remote.sd );
     stream->remote.sd = -1;

     ret = net_connect( stream->remote.addr, 
                        SOCK_STREAM, IPPROTO_TCP, &stream->remote.sd );
     if (ret)
          return ret;
     
     stream->fd = stream->remote.sd;
 
     if (stream->remote.auth) {
          snprintf( buf, sizeof(buf), 
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s:%d\r\n"
                    "Authorization: Basic %s\r\n"
                    "User-Agent: DirectFB/%s\r\n"
                    "Accept: */*\r\n"
                    "Range: bytes=%d-\r\n"
                    "Connection: Close\r\n"
                    "\r\n",
                    stream->remote.path, 
                    stream->remote.host,
                    stream->remote.port,
                    stream->remote.auth,
                    DIRECTFB_VERSION, offset );
     }
     else {
          snprintf( buf, sizeof(buf), 
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s:%d\r\n"
                    "User-Agent: DirectFB/%s\r\n"
                    "Accept: */*\r\n"
                    "Range: bytes=%d-\r\n"
                    "Connection: Close\r\n"
                    "\r\n",
                    stream->remote.path, 
                    stream->remote.host,
                    stream->remote.port,
                    DIRECTFB_VERSION, offset );
     }
     
     send( stream->remote.sd, buf, strlen( buf ), 0 );
    
     D_DEBUG_AT( Direct_Stream, "sent [%s].\n", buf );
     
     while (net_readl( stream->remote.sd, buf, sizeof(buf) ) > 0) {
          D_DEBUG_AT( Direct_Stream, "got [%s].\n", buf );
          
          if (!strncmp( buf, "HTTP/", sizeof("HTTP/")-1 )) {
               int version;
               sscanf( buf, "HTTP/1.%d %d", &version, &status );
          }
          else if (!strncmp( buf, "ICY ", sizeof("ICY ")-1 )) {
               /* Icecast/Shoutcast */
               sscanf( buf, "ICY %d", &status );
          }
     }

     switch (status) {
          case 200 ... 299:
               break;
          default:
               if (status)
                    D_DEBUG_AT( Direct_Stream,
                                "server returned status %d.\n", status );
               return DFB_FAILURE;
     }

     stream->offset = offset;

     return DFB_OK;
}    

static DirectResult
http_open( DirectStream *stream, const char *filename )
{
     DirectResult ret    = DFB_OK;
     int          status = 0;
     char         buf[1024];
     
     stream->remote.port = HTTP_PORT;

     ret = tcp_open( stream, filename );
     if (ret)
          return ret;

     if (stream->remote.user) {
          char *tmp;

          if (stream->remote.pass) {
               tmp = alloca( strlen( stream->remote.user ) +
                             strlen( stream->remote.pass ) + 2 );
               sprintf( tmp, "%s:%s",
                        stream->remote.user, stream->remote.pass );
          } else {
               tmp = alloca( strlen( stream->remote.user ) + 2 );
               sprintf( tmp, "%s:", stream->remote.user );
          }

          stream->remote.auth = base64_encode( tmp );
     }

     if (stream->remote.auth) {
          snprintf( buf, sizeof(buf), 
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s:%d\r\n"
                    "Authorization: Basic %s\r\n"
                    "User-Agent: DirectFB/%s\r\n"
                    "Accept: */*\r\n"
                    "Connection: Close\r\n"
                    "\r\n",
                    stream->remote.path,
                    stream->remote.host,
                    stream->remote.port,
                    stream->remote.auth,
                    DIRECTFB_VERSION );
     }
     else {
          snprintf( buf, sizeof(buf), 
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s:%d\r\n"
                    "User-Agent: DirectFB/%s\r\n"
                    "Accept: */*\r\n"
                    "Connection: Close\r\n"
                    "\r\n",
                    stream->remote.path, 
                    stream->remote.host,
                    stream->remote.port,
                    DIRECTFB_VERSION );
     }
     
     send( stream->remote.sd, buf, strlen( buf ), 0 );
     
     D_DEBUG_AT( Direct_Stream, "sent [%s].\n", buf );

     while (net_readl( stream->remote.sd, buf, sizeof(buf) ) > 0) {
          D_DEBUG_AT( Direct_Stream, "got [%s].\n", buf );

          if (!strncmp( buf, "HTTP/", sizeof("HTTP/")-1 )) {
               int version;
               sscanf( buf, "HTTP/1.%d %d", &version, &status );
          }
          else if (!strncmp( buf, "ICY ", sizeof("ICY ")-1 )) {
               /* Icecast/Shoutcast */
               sscanf( buf, "ICY %d", &status );
          }
          else if (!strncasecmp( buf, "Accept-Ranges:", sizeof("Accept-Ranges:")-1 )) {
               if (strcmp( trim( buf+sizeof("Accept-Ranges:")-1 ), "none" ))
                    stream->seek = http_seek;
          }
          else if (!strncasecmp( buf, "Content-Type:", sizeof("Content-Type:")-1 )) {
               if (stream->mime)
                    D_FREE( stream->mime );
               stream->mime = D_STRDUP( trim( buf+sizeof("Content-Type:")-1 ) );
          }
          else if (!strncasecmp( buf, "Content-Length:", sizeof("Content-Length:")-1 )) {
               char *tmp = trim( buf+sizeof("Content-Length:")-1 );
               if (sscanf( tmp, "%d", &stream->length ) < 1)
                    sscanf( tmp, "bytes=%d", &stream->length );
          }
          else if (!strncasecmp( buf, "Location:", sizeof("Location:")-1 )) { 
               net_close( stream );
               stream->seek = NULL;
               
               if (++stream->remote.redirects > HTTP_MAX_REDIRECTS) {
                    D_ERROR( "Direct/Stream: "
                             "reached maximum number of redirects (%d).\n",
                             HTTP_MAX_REDIRECTS );
                    stream->remote.redirects = 0;
                    return DFB_FAILURE;
               }
               
               filename = trim( buf+sizeof("Location:")-1 );
               if (!strncmp( filename, "http://", 7 )) {
                    return http_open( stream, filename+7 );
               }
               else if (!strncmp( filename, "ftp://", 6 )) {
                    return ftp_open( stream, filename+6 );
               }
               else {
                    return DFB_UNSUPPORTED;
               }
          }
     }

     switch (status) {
          case 200 ... 299:
               break;
          default:
               if (status)
                    D_DEBUG_AT( Direct_Stream,
                                "server returned status %d.\n", status );     
               net_close( stream );
               return (status == 404) ? DFB_FILENOTFOUND : DFB_FAILURE;
     }

     return DFB_OK;
}

/*****************************************************************************/

static DirectResult
ftp_command( DirectStream *stream, char *cmd, int len )
{
     fd_set         s;
     struct timeval t;

     FD_ZERO( &s );
     FD_SET( stream->remote.sd, &s );

     t.tv_sec  = NET_TIMEOUT;
     t.tv_usec = 0;

     switch (select( stream->remote.sd+1, NULL, &s, NULL, &t )) {
          case 0:
               return DFB_TIMEOUT;
          case -1:
               return errno2result( errno );
     }

     if (send( stream->remote.sd, cmd, len, 0 ) < 0)
          return errno2result( errno );

     if (D_DEBUG_ENABLED) {
          if (cmd[0] != 'P' || cmd[1] != 'A' ||
              cmd[2] != 'S' || cmd[3] != 'S')
          {
               cmd[len-2] = '\0';
               D_DEBUG_AT( Direct_Stream, "sent [%s].\n", cmd );
          }
          else {
               D_DEBUG_AT( Direct_Stream, "sent [PASS ****].\n" );
          }
     }

     return DFB_OK;
}

static int
ftp_response( DirectStream *stream, char *buf, size_t size )
{
     while (net_readl( stream->remote.sd, buf, size ) > 0) {
          D_DEBUG_AT( Direct_Stream, "got [%s].\n", buf );
         
          if (isdigit(buf[0]) && isdigit(buf[1]) &&
              isdigit(buf[2]) && buf[3] == ' ')
          {
               return (buf[0] - '0') * 100 +
                      (buf[1] - '0') *  10 +
                      (buf[2] - '0');
          }
     }

     return -1;
}

static DirectResult
ftp_open_pasv( DirectStream *stream, char *buf, size_t size )
{
     DirectResult ret = DFB_FAILURE;
     int          len;
     int          i;

     len = snprintf( buf, size, "PASV\r\n" );
     ret = ftp_command( stream, buf, len );
     if (ret)
          return ret;

     if (ftp_response( stream, buf, size ) != 227)
          return DFB_FAILURE;

     /* parse IP and port for passive mode */
     for (i = 4; buf[i]; i++) {
          if (isdigit( buf[i] )) {
               unsigned int d[6];

               if (sscanf( &buf[i], "%u,%u,%u,%u,%u,%u",
                           &d[0], &d[1], &d[2], &d[3], &d[4], &d[5] ) == 6)
               {
                    struct addrinfo hints, *addr;
                    
                    /* address */
                    len = snprintf( buf, size, 
                                    "%u.%u.%u.%u",
                                    d[0], d[1], d[2], d[3] );
                    /* port */
                    snprintf( buf+len+1, size-len-1,
                              "%u", ((d[4] & 0xff) << 8) | (d[5] & 0xff) );

                    memset( &hints, 0, sizeof(hints) );
                    hints.ai_flags    = AI_CANONNAME;
                    hints.ai_socktype = SOCK_STREAM;
                    hints.ai_family   = PF_UNSPEC;

                    if (getaddrinfo( buf, buf+len+1, &hints, &addr )) {
                         D_DEBUG_AT( Direct_Stream, 
                                     "failed to resolve host '%s'.\n", buf );
                         return DFB_FAILURE;
                    }
                    
                    ret = net_connect( addr, SOCK_STREAM, 
                                       IPPROTO_TCP, &stream->fd );
                    freeaddrinfo( addr );
               
                    return ret;
               }
          }
     }

     return DFB_FAILURE;
}

static DirectResult
ftp_seek( DirectStream *stream, unsigned int offset )
{
     DirectResult ret = DFB_OK;
     int          status;
     char         buf[512];
     int          len;
    
     if (stream->fd > 0) {
          close( stream->fd );
          stream->fd = -1;

          /* ignore response */
          ftp_response( stream, buf, sizeof(buf) );
     }

     ret = ftp_open_pasv( stream, buf, sizeof(buf) );
     if (ret)
          return ret;
     
     len = snprintf( buf, sizeof(buf), "REST %d\r\n", offset );
     ret = ftp_command( stream, buf, len );
     if (ret)
          goto error;
     
     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 350) {
          ret = DFB_FAILURE;
          goto error;
     }

     len = snprintf( buf, sizeof(buf), "RETR %s\r\n", stream->remote.path );
     ret = ftp_command( stream, buf, len );
     if (ret)
          goto error;

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 150 && status != 125) {
          ret = DFB_FAILURE;
          goto error;
     }

     stream->offset = offset;

     return DFB_OK;

error:
     close( stream->fd );
     stream->fd = -1;
     
     return ret;
}

static DirectResult
ftp_open( DirectStream *stream, const char *filename )
{
     DirectResult ret = DFB_OK;
     int          status;
     char         buf[512];
     int          len;
     
     stream->remote.port = FTP_PORT;

     ret = tcp_open( stream, filename );
     if (ret)
          return ret;

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 220) {
          net_close( stream );
          return DFB_FAILURE;
     }
     
     /* login */
     len = snprintf( buf, sizeof(buf), "USER %s\r\n", 
                     stream->remote.user ? : "anonymous" );
     ret = ftp_command( stream, buf, len );
     if (ret) {
          net_close( stream );
          return ret;
     }

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 230 && status != 331) {
          net_close( stream );
          return DFB_FAILURE;
     }

     if (stream->remote.pass) {
          len = snprintf( buf, sizeof(buf),
                         "PASS %s\r\n", stream->remote.pass );
          ret = ftp_command( stream, buf, len );
          if (ret) {
               net_close( stream );
               return ret;
          }

          status = ftp_response( stream, buf, sizeof(buf) );
          if (status != 230) {
               net_close( stream );
               return DFB_FAILURE;
          }
     }
     
     /* enter binary mode */
     len = snprintf( buf, sizeof(buf), "TYPE I\r\n" );
     ret = ftp_command( stream, buf, len );
     if (ret) {
          net_close( stream );
          return ret;
     }

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 200) {
          net_close( stream );
          return DFB_FAILURE;
     }

     /* get file size */
     len = snprintf( buf, sizeof(buf), "SIZE %s\r\n", stream->remote.path );
     ret = ftp_command( stream, buf, len );
     if (ret) {
          net_close( stream );
          return ret;
     }

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status == 213)
          stream->length = strtol( buf+4, NULL, 10 );

     /* enter passive mode by default */
     ret = ftp_open_pasv( stream, buf, sizeof(buf) );
     if (ret) {
          net_close( stream );
          return ret;
     }

     /* retrieve file */
     len = snprintf( buf, sizeof(buf), "RETR %s\r\n", stream->remote.path );
     ret = ftp_command( stream, buf, len );
     if (ret) {
          net_close( stream );
          return ret;
     }

     status = ftp_response( stream, buf, sizeof(buf) );
     if (status != 150 && status != 125) {
          net_close( stream );
          return DFB_FAILURE;
     }

     stream->seek = ftp_seek;
     
     return DFB_OK;
}

/*****************************************************************************/

static DirectResult
udp_open( DirectStream *stream, const char *filename )
{
     DirectResult    ret = DFB_OK; 
     struct addrinfo hints;
     char            port[16];
     
     parse_url( filename, 
                &stream->remote.host,
                &stream->remote.port,
                NULL,
                NULL,
                NULL );
     
     snprintf( port, sizeof(port), "%d", stream->remote.port );

     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_DGRAM;
     hints.ai_family   = PF_UNSPEC;
     
     if (getaddrinfo( stream->remote.host, port,
                      &hints, &stream->remote.addr )) {
          D_ERROR( "Direct/Stream: "
                   "failed to resolve host '%s'!\n", stream->remote.host );
          net_close( stream );
          return DFB_FAILURE;
     }

     ret = net_connect( stream->remote.addr, 
                        SOCK_DGRAM, IPPROTO_UDP, &stream->remote.sd );
     if (ret)
          return ret;

     stream->fd     = stream->remote.sd;
     stream->length = -1; 
     stream->wait   = net_wait;
     stream->peek   = net_peek;
     stream->read   = net_read;
     stream->close  = net_close;

     return ret;
}

/*****************************************************************************/

static DirectResult
pipe_wait( DirectStream   *stream,
           unsigned int    length,
           struct timeval *tv )
{
     fd_set s;

     if (stream->cache_size >= length)
          return DFB_OK;
     
     FD_ZERO( &s );
     FD_SET( stream->fd, &s );
     
     switch (select( stream->fd+1, &s, NULL, NULL, tv )) {
          case 0:
               return DFB_TIMEOUT;
          case -1:
               return errno2result( errno );
     }

     return DFB_OK;
}

static DirectResult
pipe_peek( DirectStream *stream,
           unsigned int  length,
           int           offset,
           void         *buf,
           unsigned int *read_out )
{
     unsigned int size = length;
     int          len;

     if (offset < 0)
          return DFB_UNSUPPORTED;

     len = length + offset;
     if (len > stream->cache_size) {
          ssize_t s;
          
          stream->cache = D_REALLOC( stream->cache, len );
          if (!stream->cache) {
               stream->cache_size = 0;
               return D_OOM();
          }

          s = read( stream->fd, 
                    stream->cache + stream->cache_size,
                    len - stream->cache_size );
          if (s < 0) {
               if (errno != EAGAIN || stream->cache_size == 0)
                    return errno2result( errno );
               s = 0;
          }
          
          stream->cache_size += s;
          if (stream->cache_size <= offset)
               return DFB_BUFFEREMPTY;
          
          size = stream->cache_size - offset;
     }

     direct_memcpy( buf, stream->cache+offset, size );

     if (read_out)
          *read_out = size;

     return DFB_OK;
}

static DirectResult
pipe_read( DirectStream *stream,
           unsigned int  length,
           void         *buf,
           unsigned int *read_out )
{
     unsigned int size = 0;

     if (stream->cache_size) {
          size = MIN( stream->cache_size, length );
          
          direct_memcpy( buf, stream->cache, size );
     
          length -= size;
          stream->cache_size -= size; 
          
          if (stream->cache_size) {
               direct_memcpy( stream->cache, 
                              stream->cache+size, stream->cache_size );
          } else {
               D_FREE( stream->cache );
               stream->cache = NULL;
               stream->cache_size = 0;
          }
     }

     if (length) {
          ssize_t s;
          
          s = read( stream->fd, buf+size, length-size );
          switch (s) {
               case 0:
                    if (!size)
                         return DFB_EOF;
                    break;
               case -1:
                    if (!size) {
                         return (errno == EAGAIN)
                                ? DFB_BUFFEREMPTY
                                : errno2result( errno );
                    }
                    break;
               default:
                    size += s;
                    break;
          }
     }

     stream->offset += size;

     if (read_out)
          *read_out = size;

     return DFB_OK;
}
 
/*****************************************************************************/

static DirectResult
file_peek( DirectStream *stream,
           unsigned int  length,
           int           offset,
           void         *buf,
           unsigned int *read_out )
{
     DirectResult ret = DFB_OK;
     ssize_t      size;
     
     if (lseek( stream->fd, offset, SEEK_CUR ) < 0)
          return DFB_FAILURE;

     size = read( stream->fd, buf, length );
     switch (size) {
          case 0:
               ret = DFB_EOF;
               break;
          case -1:
               ret = (errno == EAGAIN)
                     ? DFB_BUFFEREMPTY
                     : errno2result( errno );
               size = 0;
               break;
     }

     if (lseek( stream->fd, - offset - size, SEEK_CUR ) < 0)
          return DFB_FAILURE;
     
     if (read_out)
          *read_out = size;

     return ret;
}

static DirectResult
file_read( DirectStream *stream,
           unsigned int  length,
           void         *buf,
           unsigned int *read_out )
{
     ssize_t size;

     size = read( stream->fd, buf, length );
     switch (size) {
          case 0:
               return DFB_EOF;
          case -1:
               if (errno == EAGAIN)
                    return DFB_BUFFEREMPTY;
               return errno2result( errno );
     }

     stream->offset += size;
     
     if (read_out)
          *read_out = size;

     return DFB_OK;
}

static DirectResult
file_seek( DirectStream *stream, unsigned int offset )
{
     off_t off;

     off = lseek( stream->fd, offset, SEEK_SET );
     if (off < 0)
          return DFB_FAILURE;

     stream->offset = off;

     return DFB_OK;
}

static void
file_close( DirectStream *stream )
{
     if (stream->cache) {
          D_FREE( stream->cache );
          stream->cache = NULL;
     }
    
     fcntl( stream->fd, F_SETFL, 
               fcntl( stream->fd, F_GETFL ) & ~O_NONBLOCK );
     
     close( stream->fd );
}

static DirectResult
file_open( DirectStream *stream, const char *filename )
{
     if (filename)
          stream->fd = open( filename, O_RDONLY | O_NONBLOCK );
     else
          stream->fd = dup( fileno( stdin ) );
     
     if (stream->fd < 0)
          return errno2result( errno );
    
     fcntl( stream->fd, F_SETFL, 
               fcntl( stream->fd, F_GETFL ) | O_NONBLOCK );

     if (lseek( stream->fd, 0, SEEK_CUR ) < 0 && errno == ESPIPE) {
          stream->length = -1;
          stream->wait   = pipe_wait;
          stream->peek   = pipe_peek;
          stream->read   = pipe_read;
     }
     else {
          struct stat s;

          if (fstat( stream->fd, &s ) < 0) {
               DirectResult ret = errno2result( errno );
               close( stream->fd );
               return ret;
          }
      
          stream->length = s.st_size;
          stream->peek   = file_peek;
          stream->read   = file_read;
          stream->seek   = file_seek;
     }

     stream->close = file_close;
     
     return DFB_OK;
}

/*****************************************************************************/

DirectResult
direct_stream_create( const char    *filename,
                      DirectStream **ret_stream )
{
     DirectStream *stream;
     DirectResult  ret    = DFB_OK;

     D_ASSERT( filename != NULL );
     D_ASSERT( ret_stream != NULL );
     
     stream = D_CALLOC( 1, sizeof(DirectStream) );
     if (!stream)
          return D_OOM();

     D_MAGIC_SET( stream, DirectStream );
     
     stream->ref = 1;

     if (!strncmp( filename, "http://", 7 ) ||
         !strncmp( filename, "unsv://", 7 )) {
          ret = http_open( stream, filename+7 );
     }
     else if (!strncmp( filename, "ftp://", 6 )) {
          ret = ftp_open( stream, filename+6 );
     }
     else if (!strncmp( filename, "tcp://", 6 )) {
          ret = tcp_open( stream, filename+6 );
     }
     else if (!strncmp( filename, "udp://", 6 )) {
          ret = udp_open( stream, filename+6 );
     }
     else if (!strncmp( filename, "file:/", 6 )) {
          ret = file_open( stream, filename+6 );
     }
     else if (!strncmp( filename, "stdin:/", 7 )) {
          ret = file_open( stream, NULL );
     }
     else {
          ret = file_open( stream, filename );
     }

     if (ret) {
          D_FREE( stream );
          return ret;
     }

     *ret_stream = stream;

     return DFB_OK;
}

DirectStream*
direct_stream_dup( DirectStream *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
     
     stream->ref++;
     
     return stream;
}

int
direct_stream_fileno( DirectStream  *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
     
     return stream->fd;
}

bool
direct_stream_seekable( DirectStream *stream )
{
     D_ASSERT( stream != NULL );
     
     D_MAGIC_ASSERT( stream, DirectStream );

     return stream->seek ? true : false;
}

bool
direct_stream_remote( DirectStream *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
     
     return stream->remote.sd ? true : false;
}

const char*
direct_stream_mime( DirectStream *stream )
{
     D_ASSERT( stream != NULL );
     
     D_MAGIC_ASSERT( stream, DirectStream );
     
     return stream->mime;
}

unsigned int 
direct_stream_offset( DirectStream *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
     
     return stream->offset;
}

unsigned int
direct_stream_length( DirectStream *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );

     return (stream->length >= 0) ? stream->length : stream->offset;
}

DirectResult
direct_stream_wait( DirectStream   *stream,
                    unsigned int    length,
                    struct timeval *tv )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
 
     if (length && stream->wait)
          return stream->wait( stream, length, tv );

     return DFB_OK;
}

DirectResult
direct_stream_peek( DirectStream *stream,
                    unsigned int  length,
                    int           offset,
                    void         *buf,
                    unsigned int *read_out )
{
     D_ASSERT( stream != NULL );
     D_ASSERT( length != 0 );
     D_ASSERT( buf != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );
     
     if (stream->length >= 0 && (stream->offset + offset) >= stream->length)
          return DFB_EOF;

     if (stream->peek)
          return stream->peek( stream, length, offset, buf, read_out );

     return DFB_UNSUPPORTED;
}

DirectResult
direct_stream_read( DirectStream *stream,
                    unsigned int  length,
                    void         *buf,
                    unsigned int *read_out )
{
     D_ASSERT( stream != NULL ); 
     D_ASSERT( length != 0 );
     D_ASSERT( buf != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );

     if (stream->length >= 0 && stream->offset >= stream->length)
          return DFB_EOF;

     if (stream->read)
          return stream->read( stream, length, buf, read_out );

     return DFB_UNSUPPORTED;
}

DirectResult
direct_stream_seek( DirectStream *stream,
                    unsigned int  offset )
{
     D_ASSERT( stream != NULL );
     
     D_MAGIC_ASSERT( stream, DirectStream );

     if (stream->offset == offset)
          return DFB_OK;
     
     if (stream->length >= 0 && offset > stream->length)
          offset = stream->length;
     
     if (stream->seek)
          return stream->seek( stream, offset );

     return DFB_UNSUPPORTED;
}

void
direct_stream_destroy( DirectStream *stream )
{
     D_ASSERT( stream != NULL );

     D_MAGIC_ASSERT( stream, DirectStream );

     if (--stream->ref == 0) {
          if (stream->close)
               stream->close( stream );

          D_FREE( stream );
     }
}

