/*
   (c) Copyright 2002-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2002-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__ONE_UDP_H__
#define __ONE__ONE_UDP_H__

#include <linux/wait.h>

#include "packet.h"
#include "types.h"


int  one_udp_create      ( OneCore     *core,
                           u32          other_ip,
                           OneUDP     **ret_udp );

void one_udp_destroy     ( OneUDP      *udp );

int  one_udp_send_message( OneUDP             *udp,
                           const struct iovec *iov,
                           size_t              iov_count );

int
one_udp_dispatch( OneUDP                *core,
                  OneApp                *app,
                  const OnePacketHeader *header,
                  const struct iovec    *iov,
                  size_t                 iov_count );


typedef enum {
     OUMT_DISPATCH       = 1,
     OUMT_ATTACH         = 2,
     OUMT_DETACH         = 3,
} OneUDPMessageType;

typedef struct {
     OneUDPMessageType     type;

     union {
          struct {
               OnePacketHeader     header;

               char                buf[ONE_MAX_PACKET_SIZE];
          } dispatch;

          struct {
               OneQID              queue_id;
               OneQID              target_id;
          } attach;

          struct {
               OneQID              queue_id;
               OneQID              target_id;
          } detach;
     };
} OneUDPMessage;


#endif
