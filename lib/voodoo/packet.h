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

#ifndef __VOODOO__PACKET_H__
#define __VOODOO__PACKET_H__

extern "C" {
#include <voodoo/types.h>
}


typedef enum {
     VPHF_NONE       = 0x00000000,

     VPHF_COMPRESSED = 0x00000001,

     VPHF_ALL        = 0x00000001
} VoodooPacketHeaderFlags;


typedef struct {
     u32  size;
     u32  flags;
     u32  uncompressed;
     u32  align;
} VoodooPacketHeader;


class VoodooPacket {
public:
     DirectLink          link;
     bool                sending;

private:
     void               *data;
     void               *current;
     VoodooPacketHeader  header;

     VoodooPacket( u32   size,
                   void *data )
          :
          sending(false),
          data(data),
          current(data)
     {
          memset( &link, 0, sizeof(link) );

          header.size         = size;
          header.flags        = VPHF_NONE;
          header.uncompressed = size;
     }

     VoodooPacket( u32   size,
                   u32   uncompressed,
                   void *data )
          :
          sending(false),
          data(data),
          current(data)
     {
          memset( &link, 0, sizeof(link) );

          header.size         = size;
          header.flags        = VPHF_COMPRESSED;
          header.uncompressed = uncompressed;
     }

public:
/*
     static VoodooPacket *
     New( u32 size )
     {
          VoodooPacket *packet = (VoodooPacket*) D_MALLOC( sizeof(VoodooPacket) + size );

          if (!packet) {
               D_OOM();
               return NULL;
          }

          return new (packet) VoodooPacket( size, packet + 1 );


          if (data)
               this->data = data;
          else
               this->data = header + 1;
     }
*/
     static VoodooPacket *
     New( u32   size,
          void *data )
     {
          return new VoodooPacket( size, data );
     }

     static VoodooPacket *
     Reset( VoodooPacket *packet,
            u32           size,
            void         *data )
     {
          return new (packet) VoodooPacket( size, data );
     }

     static VoodooPacket *
     New( void *header,
          u32   size )
     {
          VoodooPacketHeader *h = (VoodooPacketHeader*) header;

          h->size         = size;
          h->flags        = VPHF_NONE;
          h->uncompressed = size;

          return new VoodooPacket( size, (char*) header + sizeof(VoodooPacketHeader) );
     }

     static VoodooPacket *
     New( u32 size )
     {
          VoodooPacket *p = (VoodooPacket*) malloc( sizeof(VoodooPacket) + VOODOO_PACKET_MAX );

          if (!p) {
               D_OOM();
               return NULL;
          }

          return new (p) VoodooPacket( size, p + 1 );
     }

     static VoodooPacket *
     Compressed( VoodooPacket *packet )
     {
          VoodooPacket *p = (VoodooPacket*) malloc( sizeof(VoodooPacket) + packet->header.size * 4 / 3 );

          if (!p) {
               D_OOM();
               return NULL;
          }

          int compressed = direct_fastlz_compress( packet->data, packet->header.uncompressed, p + 1 );

          if ((size_t) compressed < packet->header.uncompressed)
               return new (p) VoodooPacket( compressed, packet->header.uncompressed, p + 1 );

          free( p );

          return packet;
     }

     inline u32
     size() const
     {
          return header.size;
     }

     inline u32
     flags() const
     {
          return header.flags;
     }

     inline u32
     uncompressed() const
     {
          return header.uncompressed;
     }

     inline const void *
     data_header() const
     {
          D_ASSERT( data == this + 1 );

          return &header;
     }

     inline void *
     data_raw() const
     {
          return current;
     }


     inline bool
     append( size_t size )
     {
          D_ASSERT( data == this + 1 );

          if (header.size + size > VOODOO_PACKET_MAX)
               return false;

          current = (char*) data + header.size;

          header.size         += size;
          header.uncompressed += size;

          return true;
     }

     inline void
     reset( size_t size )
     {
          D_ASSERT( data == this + 1 );

          current = (char*) data;

          header.size         = size;
          header.uncompressed = size;
     }
};


#endif
