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

#ifndef __VOODOO__CONNECTION_LINK_H__
#define __VOODOO__CONNECTION_LINK_H__

#include <voodoo/connection.h>

extern "C" {
#include <direct/thread.h>
}

#define VOODOO_CONNECTION_PACKET_NUM_OUTPUT  2
#define VOODOO_CONNECTION_LINK_INPUT_BUF_MAX ((VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader)) * 1)


class VoodooConnectionLink : public VoodooConnection {
protected:
     struct {
          u8                    *buffer;
          size_t                 start;
          size_t                 last;
          size_t                 end;
          size_t                 max;
     } input;

     struct {
          DirectMutex            lock;
          DirectWaitQueue        wait;
          DirectTLS              tls;
          DirectLink            *packets;

          VoodooPacket          *sending;
          size_t                 sent;
     } output;

public:
     VoodooConnectionLink( VoodooManager *manager,
                           VoodooLink    *link );

     virtual ~VoodooConnectionLink();


     virtual VoodooPacket *GetPacket( size_t        length );
     virtual void          PutPacket( VoodooPacket *packet,
                                      bool          flush );


private:
     void                  Flush    ( VoodooPacket *packet );


private:
     class Packets {
     private:
          VoodooPacket *packets[VOODOO_CONNECTION_PACKET_NUM_OUTPUT];
          size_t        next;
          size_t        num;

     public:
          VoodooPacket *active;

     public:
          Packets()
               :
               next(0),
               num(0),
               active(NULL)
          {
               memset( packets, 0, sizeof(packets) );
          }

          VoodooPacket *Get();
     };
};


#endif
