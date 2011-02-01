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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/play.h>

#include <WinSock.h>

/**********************************************************************************************************************/

typedef struct {
     SOCKET      socket;
     DirectMutex lock;
     WSAEVENT    event;
} Link;

static void
Close( VoodooLink *link )
{
     Link *l = link->priv;

     D_INFO( "Voodoo/Link: Closing connection.\n" );

     // FIXME: how to close socket?

     direct_mutex_deinit( &l->lock );

     WSACloseEvent( l->event );

     D_FREE( l );
}

static ssize_t
Read( VoodooLink *link,
      void       *buffer,
      size_t      count )
{
     Link *l = link->priv;

     while (true) {
          ssize_t        ret;
          fd_set         rfds;
          struct timeval tv;
          int            retval;

          FD_ZERO( &rfds );
          FD_SET( l->socket, &rfds );

          tv.tv_sec  = 0;
          tv.tv_usec = 0;//10000;

          direct_mutex_lock( &l->lock );

          retval = select( l->socket+1, &rfds, NULL, NULL, &tv );
          switch (retval) {
               default:
                    ret = recv( l->socket, buffer, count, 0 );
                    direct_mutex_unlock( &l->lock );
                    return ret;

               case 0:
                    D_DEBUG( "Voodoo/Link: Timeout during select()\n" );
                    break;

               case -1:
                    //if (errno && errno != EINTR)
                    D_PERROR( "Voodoo/Player: select() on socket failed!\n" );
                    direct_mutex_unlock( &l->lock );
                    return -1;
          }

          direct_mutex_unlock( &l->lock );

          direct_thread_sleep( 20000 );
     }

     return -1;
}

static ssize_t
Write( VoodooLink *link,
       const void *buffer,
       size_t      count )
{
     ssize_t  ret;
     Link    *l = link->priv;

     direct_mutex_lock( &l->lock );

     ret = send( l->socket, buffer, count, 0 );
     if (ret < 0) {
          D_PERROR( "send(): WSA error %d\n", WSAGetLastError() );
     }

     direct_mutex_unlock( &l->lock );

     return ret;
}

static DirectResult
SendReceive( VoodooLink  *link,
             VoodooChunk *sends,
             size_t       num_send,
             VoodooChunk *recvs,
             size_t       num_recv )
{
     Link    *l = link->priv;
     size_t   i;
     ssize_t  ret;
     DWORD    wait_result;

//     direct_mutex_lock( &l->lock );

     for (i=0; i<num_send; i++) {
          //printf("writing %d\n",sends[i].length);
          ret = send( l->socket, sends[i].ptr, sends[i].length, 0 );
          //printf("wrote %d/%d\n",ret,sends[i].length);
          if (ret < 0) {
               if (WSAGetLastError() == 10035) {
                    break;
               }
               D_PERROR( "Voodoo/Link: Failed to send() data error %d!\n", WSAGetLastError() );
          }
          else {
               sends[i].done = ret;

               if (sends[i].done != sends[i].length) {
                    D_UNIMPLEMENTED();
               //               direct_mutex_unlock( &l->lock );
                    return DR_UNIMPLEMENTED;
               }

               return DR_OK;
          }
     }


     while (true) {
          LONG events = 0;
          WSANETWORKEVENTS WsaNetworkEvents;

          if (num_recv) {
               //printf("wait for recv\n");
               events |= FD_READ;
          }

          if (num_send) {
               //printf("wait for send\n");
               events |= FD_WRITE;
          }

          WSAEventSelect( l->socket, l->event, events );

          wait_result = WSAWaitForMultipleEvents( 1, &l->event, FALSE, 10, FALSE );
          switch (wait_result) {
               case WSA_WAIT_EVENT_0:
                    WSAResetEvent( l->event );

                    WSAEnumNetworkEvents(l->socket, l->event, &WsaNetworkEvents);
                    //printf("<-- events 0x%08x\n",WsaNetworkEvents.lNetworkEvents);

                    if (!WsaNetworkEvents.lNetworkEvents)
                         return DR_INTERRUPTED;

                    if (WsaNetworkEvents.lNetworkEvents & FD_WRITE) {
                         //printf("<-- event write\n");

                         for (i=0; i<num_send; i++) {
                              //printf("writing %d\n",sends[i].length);
                              ret = send( l->socket, sends[i].ptr, sends[i].length, 0 );
                              //printf("wrote %d/%d\n",ret,sends[i].length);
                              if (ret < 0) {
                                   if (WSAGetLastError() == 10035) {
                                        break;
                                   }
                                   D_PERROR( "Voodoo/Link: Failed to send() data error %d!\n", WSAGetLastError() );
                              }
                              else {
                                   sends[i].done = ret;
                         
                                   if (sends[i].done != sends[i].length) {
                                        D_UNIMPLEMENTED();
                                   //               direct_mutex_unlock( &l->lock );
                                        return DR_UNIMPLEMENTED;
                                   }
                         
                                   return DR_OK;
                              }
                         }
                    }
     
                    if (WsaNetworkEvents.lNetworkEvents & FD_READ) {
                         //printf("<-- event read\n");

                         for (i=0; i<num_recv; i++) {
                              ret = recv( l->socket, recvs[i].ptr, recvs[i].length, 0 );
                              //printf("read %d\n",ret);
                              if (ret < 0) {
                                   if (WSAGetLastError() == 10035) {
                                        break;
                                   }
                                   D_PERROR( "Voodoo/Link: Failed to recv() data error %d!\n", WSAGetLastError() );
                    //               direct_mutex_unlock( &l->lock );
                                   return DR_FAILURE;
                              }
          
                              recvs[i].done = ret;
          
                              if (recvs[i].done < recvs[i].length)
                                   return DR_OK;

                              return DR_OK;
                         }
                    }
                    break;
     
               case WSA_WAIT_TIMEOUT:
                    //printf("<-- timeout\n");
                    return DR_TIMEOUT;
                    break;
     
               default:
                    D_ERROR( "Voodoo/Link: WaitForMultipleObjects() failed!\n" );
                    return DR_FAILURE;
          }
     }

//     direct_mutex_unlock( &l->lock );

     return DR_OK;
}

static DirectResult
WakeUp( VoodooLink *link )
{
     Link *l = link->priv;

     //printf("*** wakeup\n");

     SetEvent( l->event );

     return DR_OK;
}

/**********************************************************************************************************************/

int
StartWinsock( void )
{
     WORD wVersionRequested;
     WSADATA wsaData;
     int err;

     static init = 0;

     if (init)
          return 0;

     /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
     wVersionRequested = MAKEWORD(2, 2);

     err = WSAStartup(wVersionRequested, &wsaData);
     if (err != 0) {
          /* Tell the user that we could not find a usable */
          /* Winsock DLL.                                  */
          printf("WSAStartup failed with error: %d\n", err);
          return 1;
     }

     /* Confirm that the WinSock DLL supports 2.2.*/
     /* Note that if the DLL supports versions greater    */
     /* than 2.2 in addition to 2.2, it will still return */
     /* 2.2 in wVersion since that is the version we      */
     /* requested.                                        */

     if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
          /* Tell the user that we could not find a usable */
          /* WinSock DLL.                                  */
          printf("Could not find a usable version of Winsock.dll\n");
          WSACleanup();
          return 1;
     }
     else
          printf("The Winsock 2.2 dll was found okay\n");


     init = 1;


     return 0;
}

DirectResult
voodoo_link_init_connect( VoodooLink *link,
                          const char *hostname,
                          int         port )
{
     int                 ret; 
     struct sockaddr_in  addr;
     Link               *l;

     D_INFO( "Voodoo/Link: Connecting to '%s:%d'...\n", hostname, port );

     StartWinsock();

     l = D_CALLOC( 1, sizeof(Link) );
     if (!l)
          return D_OOM();

     l->socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
     if (l->socket == INVALID_SOCKET) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Link: Socket creation failed!\n" );
          D_FREE( l );
          return ret;
     }

     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = inet_addr( hostname );
     addr.sin_port        = htons( port );

     ret = connect( l->socket, (const struct sockaddr*) &addr, sizeof(addr) );
     if (ret < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Link: Socket connect failed!\n" );
          // FIXME: how to close the socket?
          D_FREE( l );
          return ret;
     }

     direct_mutex_init( &l->lock );

     l->event  = WSACreateEvent();

     link->priv        = l;
     link->Close       = Close;
     link->Read        = Read;
     link->Write       = Write;
     link->SendReceive = SendReceive;
     link->WakeUp      = WakeUp;

     return DR_OK;
}
