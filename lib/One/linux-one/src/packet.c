/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Packet holds data to be transferred
*/

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/uio.h>

#include <asm/uaccess.h>

#include <linux/one.h>

#include "debug.h"
#include "onedev.h"
#include "list.h"
#include "packet.h"




/******************************************************************************/

OnePacket *
OnePacket_New()
{
     OnePacket *packet;

     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     packet = one_core_malloc( one_core, sizeof(OnePacket) );
     if (!packet)
          return NULL;

     packet->link.magic = 0;
     packet->link.prev  = NULL;
     packet->link.next  = NULL;

     packet->magic = 0;
     packet->flush = false;

     memset( &packet->header, 0, sizeof(packet->header) );

     D_MAGIC_SET( packet, OnePacket );

     return packet;
}

void
OnePacket_Free( OnePacket *packet )
{
     ONE_DEBUG( "%s( %p )\n", __FUNCTION__, packet );

     D_MAGIC_ASSERT( packet, OnePacket );

     D_ASSERT( packet->link.prev == NULL );
     D_ASSERT( packet->link.next == NULL );

     D_MAGIC_CLEAR( packet );

     one_core_free( one_core, packet );
}

int
OnePacket_Write( OnePacket          *packet,
                 OnePacketHeader    *header,
                 const struct iovec *iov,
                 size_t              iov_count )
{
     size_t i, len, offset;

     ONE_DEBUG( "%s( packet %p, queue_id %u, flags 0x%x, size %u, uncompressed %u, iov %p, iov_count %zu )\n",
                __FUNCTION__, packet, header->queue_id, header->flags, header->size, header->uncompressed, iov, iov_count );

     D_MAGIC_ASSERT( packet, OnePacket );

     /* Queue ID must already be set */
     D_ASSERT( packet->header.queue_id == header->queue_id );

     /* When appending, check certain limitations */
     if (packet->header.size) {
          D_ASSERT( !(packet->header.flags & ONE_PACKET_COMPRESSED) );

          /* Cannot append to compressed packets */
          if (packet->header.flags & ONE_PACKET_COMPRESSED)
               return -EIO;

          /* Cannot append with different flags, e.g. compression */
          if (packet->header.flags != header->flags)
               return -EIO;
     }

     /* Check if packet has enough free space */
     if (packet->header.size + header->size > ONE_MAX_PACKET_SIZE)
          return -E2BIG;

     D_ASSERT( (header->size == header->uncompressed) || (header->flags & ONE_PACKET_COMPRESSED) );

     len    = header->size;
     offset = packet->header.size;

     for (i=0; i<iov_count; i++) {
          if (iov[i].iov_len > len)
               return -EINVAL;

          if (copy_from_user( packet->buf + offset, iov[i].iov_base, iov[i].iov_len ))
               return -EFAULT;

          offset += iov[i].iov_len;
          len    -= iov[i].iov_len;
     }

     /* Insufficient data from iovec? */
     if (len)
          return -EINVAL;

     /* Update packet header */
     packet->header.flags         = header->flags;
     packet->header.size         += header->size;
     packet->header.uncompressed += header->uncompressed;

     return 0;
}

int
OnePacket_Read( OnePacket          *packet,
                const struct iovec *iov,
                size_t              iov_count,
                size_t              offset )
{
     size_t i, iov_start = 0, read_offset = 0, left;

     ONE_DEBUG( "%s( packet %p, queue_id %u, flags 0x%x, size %u, uncompressed %u, iov %p, iov_count %zu, offset %zu )\n", __FUNCTION__,
                packet, packet->header.queue_id, packet->header.flags, packet->header.size, packet->header.uncompressed, iov, iov_count, offset );

     D_MAGIC_ASSERT( packet, OnePacket );

     left = sizeof(OnePacketHeader) + packet->header.size;

     for (i=0; i<iov_count; i++) {
          if (iov_start + iov[i].iov_len > offset) {
               size_t iov_offset = offset - iov_start;
               size_t copy       = iov[i].iov_len - iov_offset;

               if (copy > left)
                    copy = left;

               if (read_offset < sizeof(OnePacketHeader)) {
                    size_t header_copy = sizeof(OnePacketHeader) - read_offset;

                    if (header_copy > copy)
                         header_copy = copy;

                    if (copy_to_user( iov[i].iov_base + iov_offset, &packet->header + read_offset, header_copy ))
                         return -EFAULT;

                    iov_offset  += header_copy;
                    copy        -= header_copy;

                    offset      += header_copy;
                    read_offset += header_copy;
                    left        -= header_copy;
               }

               if (copy) {
                    if (copy_to_user( iov[i].iov_base + iov_offset, packet->buf + read_offset - sizeof(OnePacketHeader), copy ))
                         return -EFAULT;

                    offset      += copy;
                    read_offset += copy;
                    left        -= copy;
               }
          }

          iov_start += iov[i].iov_len;
     }

     D_ASSERT( left == 0 );

     return 0;
}

