/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/mem.h>

#include <voodoo/play.h>

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/if.h>

#include <sys/ioctl.h>



DirectResult
voodoo_play_get_broadcast( VoodooPlayAddress **ret_addr,
                           size_t             *ret_num )
{
     size_t             num = 0;
     size_t             i   = 0;
     VoodooPlayAddress *addr;

     int            ret;
     int            fd;
     char          *ptr, lastname[IFNAMSIZ];
     struct ifreq   req[16];
     struct ifconf  conf;

     D_ASSERT( ret_addr != NULL );
     D_ASSERT( ret_num != NULL );

     conf.ifc_buf = (char*) req;
     conf.ifc_len = sizeof(req);

     fd = socket( AF_INET, SOCK_DGRAM, 0 );
     if (fd < 0) {
          D_PERROR( "Voodoo/Unix: socket( AF_INET, SOCK_DGRAM, 0 ) failed!\n" );
          return DR_FAILURE;
     }

     ret = ioctl( fd, SIOCGIFCONF, &conf );
     if (ret) {
          D_PERROR( "Voodoo/Player: ioctl( SIOCGIFCONF ) failed!\n" );
          close( fd );
          return DR_FAILURE;
     }

     lastname[0] = 0;

     for (ptr = conf.ifc_buf; ptr < conf.ifc_buf + conf.ifc_len; ) {
          struct ifreq         ifrcopy, *ifr  = (struct ifreq *)ptr;
          struct sockaddr_in  *saddr = (struct sockaddr_in*) &ifr->ifr_broadaddr;

#ifdef MACOS
          ptr += sizeof(ifr->ifr_name) + MAX(sizeof(struct sockaddr), ifr->ifr_addr.sa_len); // for next one in buffer
#else
          ptr += sizeof(req[0]);
#endif

          if (strncmp(lastname, ifr->ifr_name, IFNAMSIZ) == 0) {
               continue; /* already processed this interface */
          }

          memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

          ifrcopy = *ifr;
          ioctl( fd, SIOCGIFFLAGS, &ifrcopy);
          if ((ifrcopy.ifr_flags & IFF_UP) == 0)
               continue;   // ignore if interface not up

          ret = ioctl( fd, SIOCGIFBRDADDR, ifr );
          if (ret)
               continue;

          if (!saddr->sin_addr.s_addr) {
               ret = ioctl( fd, SIOCGIFDSTADDR, ifr );
               if (ret)
                    continue;
          }

          num++;
     }


     addr = D_CALLOC( num, sizeof(VoodooPlayAddress) );
     if (!addr) {
          close( fd );
          return D_OOM();
     }


     for (ptr = conf.ifc_buf; ptr < conf.ifc_buf + conf.ifc_len; ) {
          char                 buf[100];
          struct ifreq         ifrcopy, *ifr  = (struct ifreq *)ptr;
          struct sockaddr_in  *saddr = (struct sockaddr_in*) &ifr->ifr_broadaddr;

#ifdef MACOS
          ptr += sizeof(ifr->ifr_name) + MAX(sizeof(struct sockaddr), ifr->ifr_addr.sa_len); // for next one in buffer
#else
          ptr += sizeof(req[0]);
#endif

          if (strncmp(lastname, ifr->ifr_name, IFNAMSIZ) == 0) {
               continue; /* already processed this interface */
          }

          memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

          ifrcopy = *ifr;
          ioctl( fd, SIOCGIFFLAGS, &ifrcopy);
          if ((ifrcopy.ifr_flags & IFF_UP) == 0) {
               D_INFO( "Voodoo/Player:   %-16s is not up.\n", ifrcopy.ifr_name );
               continue;   // ignore if interface not up
          }

          ret = ioctl( fd, SIOCGIFBRDADDR, ifr );
          if (ret) {
               D_PERROR( "Voodoo/Player: ioctl( SIOCGIFBRDADDR ) %-16s failed!\n", ifr->ifr_name );
               continue;
          }

          if (saddr->sin_addr.s_addr) {
               inet_ntop( AF_INET, &saddr->sin_addr, buf, sizeof(buf) );

               D_INFO( "Voodoo/Player:   %-16s (%s)\n", ifr->ifr_name, buf );
          }
          else {
               ret = ioctl( fd, SIOCGIFDSTADDR, ifr );
               if (ret) {
                    D_PERROR( "Voodoo/Player: ioctl( SIOCGIFDSTADDR ) failed!\n" );
                    continue;
               }

               inet_ntop( AF_INET, &saddr->sin_addr, buf, sizeof(buf) );

               D_INFO( "Voodoo/Player:   %-16s (%s) (P-t-P)\n", ifr->ifr_name, buf );
          }

          voodoo_play_from_inet_addr( &addr[i++], saddr->sin_addr.s_addr );
     }

     close( fd );

     *ret_addr = addr;
     *ret_num  = num;

     return DR_OK;
}



#if 0

DirectResult
voodoo_play_get_broadcast( VoodooPlayAddress **ret_addr,
                           size_t             *ret_num )
{
     DirectResult       ret = DR_OK;
     VoodooPlayAddress *addr;

     // Get local host name
     char szHostName[128] = "";

     if (gethostname(szHostName, sizeof(szHostName))) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Win32: gethostname() failed!\n" );
          return ret;
     }

     // Get local IP addresses
     struct hostent *pHost = 0;

     pHost = gethostbyname(szHostName);
     if (!pHost) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Win32: gethostbyname('%s') failed!\n", szHostName );
          return ret;
     }


     size_t iCnt, iTotal = 0;

     for (iCnt = 0; pHost->h_addr_list[iCnt]; ++iCnt)
          iTotal++;


     addr = D_CALLOC( iTotal, sizeof(VoodooPlayAddress) );
     if (!addr)
          return D_OOM();

     for (iCnt = 0; pHost->h_addr_list[iCnt]; ++iCnt) {
          struct sockaddr_in SocketAddress;

          memcpy(&SocketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);

          voodoo_play_from_inet_addr( &addr[iCnt], SocketAddress.sin_addr.s_addr );
     }

     *ret_addr = addr;
     *ret_num  = iTotal;

     return DR_OK;
}

#endif

