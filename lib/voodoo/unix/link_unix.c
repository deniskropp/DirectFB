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

#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/play.h>


#if !VOODOO_BUILD_NO_SETSOCKOPT
static const int one = 1;
static const int tos = IPTOS_LOWDELAY;
#endif

/**************************************************************************************************/

#define DUMP_SOCKET_OPTION(fd,o)                                                \
do {                                                                            \
     int val = 0;                                                               \
     unsigned int len = 4;                                                      \
                                                                                \
     if (getsockopt( fd, SOL_SOCKET, o, &val, &len ))                           \
          D_PERROR( "Voodoo/Manager: getsockopt() for " #o " failed!\n" );      \
     else                                                                       \
          D_DEBUG( "Voodoo/Manager: " #o " is %d\n", val );                     \
} while (0)

/**********************************************************************************************************************/

typedef struct {
     int fd;
} Link;

static void
Close( VoodooLink *link )
{
     Link *l = link->priv;

     D_INFO( "Voodoo/Link: Closing connection.\n" );

     close( l->fd );

     D_FREE( link->priv );
     link->priv = NULL;
}

static ssize_t
Read( VoodooLink *link,
      void       *buffer,
      size_t      count )
{
     Link *l = link->priv;

     return recv( l->fd, buffer, count, 0 );
}

static ssize_t
Write( VoodooLink *link,
       const void *buffer,
       size_t      count )
{
     Link *l = link->priv;

     return send( l->fd, buffer, count, 0 );
}

/**********************************************************************************************************************/

DirectResult
voodoo_link_init_connect( VoodooLink *link,
                          const char *hostname,
                          int         port )
{
     DirectResult     ret;
     int              err;
     struct addrinfo  hints;
     struct addrinfo *addr;
     char             portstr[10];
     Link            *l;



     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = PF_UNSPEC;

     D_INFO( "Voodoo/Link: Looking up host '%s'...\n", hostname );

     snprintf( portstr, sizeof(portstr), "%d", port );

     err = getaddrinfo( hostname, portstr, &hints, &addr );
     if (err) {
          switch (err) {
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
                    D_ERROR( "Direct/Log: Service is unreachable!\n" );
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
                    D_ERROR( "Direct/Log: Unknown error occured!?\n" );
                    return DR_FAILURE;
          }
     }


     l = D_CALLOC( 1, sizeof(Link) );
     if (!l)
          return D_OOM();

     /* Create the client socket. */
     l->fd = socket( addr->ai_family, SOCK_STREAM, 0 );
     if (l->fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Link: Socket creation failed!\n" );
          freeaddrinfo( addr );
          D_FREE( l );
          return ret;
     }

     D_INFO( "Voodoo/Link: Connecting to '%s:%d'...\n", addr->ai_canonname, port );

     /* Connect to the server. */
     err = connect( l->fd, addr->ai_addr, addr->ai_addrlen );
     freeaddrinfo( addr );

     if (err) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Link: Socket connect failed!\n" );
          close( l->fd );
          D_FREE( l );
          return ret;
     }


#if !VOODOO_BUILD_NO_SETSOCKOPT
     if (setsockopt( l->fd, SOL_IP, IP_TOS, &tos, sizeof(tos) ) < 0)
          D_PERROR( "Voodoo/Manager: Could not set IP_TOS!\n" );

     if (setsockopt( l->fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Manager: Could not set TCP_NODELAY!\n" );
#endif

     DUMP_SOCKET_OPTION( l->fd, SO_SNDLOWAT );
     DUMP_SOCKET_OPTION( l->fd, SO_RCVLOWAT );
     DUMP_SOCKET_OPTION( l->fd, SO_SNDBUF );
     DUMP_SOCKET_OPTION( l->fd, SO_RCVBUF );


     link->priv  = l;
     link->Close = Close;
     link->Read  = Read;
     link->Write = Write;

     return DR_OK;
}

DirectResult
voodoo_link_init_fd( VoodooLink *link,
                     int         fd )
{
     Link *l;

     l = D_CALLOC( 1, sizeof(Link) );
     if (!l)
          return D_OOM();

     l->fd = fd;

     link->priv  = l;
     link->Close = Close;
     link->Read  = Read;
     link->Write = Write;

     return DR_OK;
}

