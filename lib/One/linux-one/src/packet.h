/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__PACKET_H__
#define __ONE__PACKET_H__

#include "onedev.h"
#include "types.h"


struct __One_OnePacket {
     DirectLink          link;

     int                 magic;

     bool                flush;

     OnePacketHeader     header;

     char                buf[ONE_MAX_PACKET_SIZE];
};


OnePacket *OnePacket_New  ( void );

void       OnePacket_Free ( OnePacket          *packet );

int        OnePacket_Write( OnePacket          *packet,
                            OnePacketHeader    *header,
                            const struct iovec *iov,
                            size_t              iov_count );

int        OnePacket_Read ( OnePacket          *packet,
                            const struct iovec *iov,
                            size_t              iov_count,
                            size_t              offset );


#endif
