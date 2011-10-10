/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE_TYPES_H__
#define __ONE_TYPES_H__


/**********************************************************************************************************************
** Queues
*/ 


/*
 * The One Queue ID is a unique identifier for one queue.
 */
typedef uint32_t OneQID;

#define ONE_QID_NONE     0


/*
 * Queue Flags
 */
typedef enum {
     ONE_QUEUE_NO_FLAGS       = 0x00000000,

     ONE_QUEUE_VIRTUAL        = 0x00000001,  /* Virtual Queue, has no receive buffers, only forwards to attached queues */
} OneQueueFlags;


/*
 * Packet Header Flags
 */
typedef enum {
     ONE_PACKET_NO_FLAGS      = 0x00000000,

     ONE_PACKET_COMPRESSED    = 0x00000001   /* Compressed Packet */
} OnePacketHeaderFlags;


/*
 * Packet Header
 */
typedef struct {
     OneQID                   queue_id;      /* Queue ID this packet is sent to */

     OnePacketHeaderFlags     flags;         /* Packet Flags */
     uint32_t                 size;          /* Packet Size */
     uint32_t                 uncompressed;  /* Packet Size after decompression */
} OnePacketHeader;




#endif

