/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/udp.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>

#include <linux/kthread.h>

#include "onecore.h"
#include "onecore_impl.h"
#include "one_udp.h"
#include "packet.h"
#include "queue.h"


#define ONE_PORT              0x4F4C         // 'O' 'L'   - One Link



struct __One_OneUDP {
     OneCore *core;
     struct task_struct *thread;
     struct socket *sock;
     struct sockaddr_in addr;
     struct socket *sock_send;
     struct sockaddr_in addr_send;
     int running;
};


/* function prototypes */
static int ksocket_receive(struct socket *sock, struct sockaddr_in *addr, void *buf, int len);
static int ksocket_send_iov(struct socket      *sock,
                            struct sockaddr_in *addr,
                            const struct iovec *iov,
                            size_t              iov_count);


static int
one_udp_thread( void *arg )
{
     OneUDP        *udp = arg;
     int            size;
     OneUDPMessage *message;

     struct sched_param param;

     param.sched_priority = 50;

     sched_setscheduler( current, SCHED_FIFO, &param );

     /* kernel thread initialization */
     udp->running = 1;

     printk(KERN_DEBUG "One/UDP: listening on port %d\n", ONE_PORT);

     message = kmalloc( sizeof(OneUDPMessage), GFP_KERNEL );
     if (!message) {
          printk( KERN_ERR "One/UDP: could not allocate %zu bytes for receiving messages\n", sizeof(OneUDPMessage) );
          return -ENOMEM;
     }

     /* main loop */
     while (!kthread_should_stop()) {
          size = ksocket_receive( udp->sock, &udp->addr, message, sizeof(OneUDPMessage) );

          if (signal_pending(current))
               break;

          if (size < 0)
               printk(KERN_DEBUG "One/UDP: error getting datagram, sock_recvmsg error = %d\n", size);
          else {
               int              ret;
               OneQueueDispatch dispatch;
               struct iovec     iov;

               ONE_DEBUG( "UDP: Received %d bytes\n", size);

               switch (message->type) {
                    case OUMT_DISPATCH:
                         ONE_DEBUG( "  -> DISPATCH\n" );
                         ONE_DEBUG( "     queue_id:     0x%08x\n", message->dispatch.header.queue_id );
                         ONE_DEBUG( "     flags:        0x%08x\n", message->dispatch.header.flags );
                         ONE_DEBUG( "     size:         %u\n", message->dispatch.header.size );
                         ONE_DEBUG( "     uncompressed: %u\n", message->dispatch.header.uncompressed );

                         if (message->dispatch.header.flags) {
                              printk(KERN_ERR "One/UDP: unsupported flags!\n" );
                              break;
                         }

                         if (message->dispatch.header.uncompressed > ONE_MAX_PACKET_SIZE) {
                              printk(KERN_ERR "One/UDP: uncompressed > ONE_MAX_PACKET_SIZE!\n" );
                              break;
                         }

                         if (message->dispatch.header.size > message->dispatch.header.uncompressed) {
                              printk(KERN_ERR "One/UDP: size (%u) > uncompressed (%u)!\n",
                                     message->dispatch.header.size, message->dispatch.header.uncompressed );
                              break;
                         }

                         one_core_lock( one_core );

                         dispatch.header    = message->dispatch.header;
                         dispatch.iov       = &iov;
                         dispatch.iov_count = 1;

                         iov.iov_base = message->dispatch.buf;
                         iov.iov_len  = message->dispatch.header.size;

                         ret = one_queue_dispatch( NULL, &dispatch );
                         if (ret)
                              printk(KERN_ERR "One/UDP: dispatch error %d!\n", ret );

                         one_core_unlock( one_core );
                         break;

                    case OUMT_ATTACH:
                         ONE_DEBUG( "  -> ATTACH\n" );
                         break;

                    case OUMT_DETACH:
                         ONE_DEBUG( "  -> DETACH\n" );
                         break;

                    default:
                         printk(KERN_ERR "One/UDP: unknown message type 0x%08x!\n", message->type );
                         break;
               }
          }
     }

     kfree( message );

     return 0;
}

static int
ksocket_send_iov( struct socket      *sock,
                  struct sockaddr_in *addr,
                  const struct iovec *iov,
                  size_t              iov_count )
{
     struct msghdr msg;
     mm_segment_t oldfs;
     int size = 0;
     size_t len = 0;
     int    i;

     D_ASSERT( sock != NULL );
     D_ASSERT( addr != NULL );
     D_ASSERT( iov != NULL );
     D_ASSERT( iov_count > 0 );

     if (sock->sk==NULL)
          return 0;

     for (i=0; i<iov_count; i++)
          len += iov[i].iov_len;

     msg.msg_flags = 0;
     msg.msg_name = addr;
     msg.msg_namelen  = sizeof(struct sockaddr_in);
     msg.msg_control = NULL;
     msg.msg_controllen = 0;
     msg.msg_iov = (struct iovec*) iov;
     msg.msg_iovlen = iov_count;
     msg.msg_control = NULL;

     oldfs = get_fs();
     set_fs(KERNEL_DS);
     size = sock_sendmsg(sock,&msg,len);
     set_fs(oldfs);

     return size;
}

static int
ksocket_receive(struct socket* sock, struct sockaddr_in* addr, void *buf, int len)
{
     struct msghdr msg;
     struct iovec iov;
     mm_segment_t oldfs;
     int size = 0;

     D_ASSERT( sock != NULL );
     D_ASSERT( addr != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( len > 0 );

     if (sock->sk==NULL) return 0;

     iov.iov_base = buf;
     iov.iov_len = len;

     msg.msg_flags = 0;
     msg.msg_name = addr;
     msg.msg_namelen  = sizeof(struct sockaddr_in);
     msg.msg_control = NULL;
     msg.msg_controllen = 0;
     msg.msg_iov = &iov;
     msg.msg_iovlen = 1;
     msg.msg_control = NULL;

     oldfs = get_fs();
     set_fs(KERNEL_DS);
     size = sock_recvmsg(sock,&msg,len,msg.msg_flags);
     set_fs(oldfs);

     return size;
}

int
one_udp_create( OneCore  *core,
                u32       other_ip,
                OneUDP  **ret_udp )
{
     OneUDP *udp;
     int     ret;

     ONE_DEBUG( "%s( other 0x%08x )\n", __FUNCTION__, other_ip );

     udp = kmalloc(sizeof(OneUDP), GFP_KERNEL);

     memset( udp, 0, sizeof(OneUDP) );

     udp->core = core;

     memset( &udp->addr,      0, sizeof(struct sockaddr) );
     memset( &udp->addr_send, 0, sizeof(struct sockaddr) );

     udp->addr.sin_family      = AF_INET;
     udp->addr_send.sin_family = AF_INET;

     udp->addr.sin_addr.s_addr      = htonl(INADDR_ANY);
     udp->addr_send.sin_addr.s_addr = htonl(other_ip);

     udp->addr.sin_port      = htons(ONE_PORT);
     udp->addr_send.sin_port = htons(ONE_PORT);


     /* create a socket */
     ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &udp->sock);
     if (ret) {
          printk(KERN_INFO "One/UDP: Could not create a datagram socket, error = %d\n", -ENXIO);
          goto error;
     }

     ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &udp->sock_send);
     if (ret) {
          printk(KERN_INFO "One/UDP: Could not create a datagram socket, error = %d\n", -ENXIO);
          goto error;
     }

     ret = udp->sock->ops->bind( udp->sock, (struct sockaddr *)&udp->addr, sizeof(struct sockaddr) );
     if (ret) {
          printk(KERN_INFO "One/UDP: Could not bind the socket, error = %d\n", -ret);
          goto error;
     }

     ret = udp->sock->ops->connect( udp->sock_send, (struct sockaddr *)&udp->addr_send, sizeof(struct sockaddr), 0 );
     if (ret) {
          printk(KERN_INFO "One/UDP: Could not connect the socket, error = %d\n", -ret);
          goto error;
     }


     /* start kernel thread */
     udp->thread = kthread_run( one_udp_thread, udp, "One/UDP" );
     if (IS_ERR(udp->thread)) {
          printk(KERN_INFO "One/UDP: unable to start kernel thread\n");
          kfree(udp);
          ret = -ENOMEM;
          goto error;
     }

     *ret_udp = udp;

     return 0;


error:
     if (udp->sock)
          sock_release(udp->sock);

     if (udp->sock_send)
          sock_release(udp->sock_send);

     return ret;
}

void
one_udp_destroy( OneUDP *udp )
{
     if (udp->thread==NULL)
          printk(KERN_INFO "One/UDP: no kernel thread to kill\n");
     else {
          force_sig( SIGTERM, udp->thread );
          kthread_stop( udp->thread );
     }

     /* free allocated resources before exit */
     if (udp->sock != NULL) {
          sock_release(udp->sock);
          udp->sock = NULL;
     }

     kfree(udp);
}

int
one_udp_send_message( OneUDP             *udp,
                      const struct iovec *iov,
                      size_t              iov_count )
{
     ONE_DEBUG( "%s( iov %p, count %u )\n", __FUNCTION__, iov, iov_count );

     return ksocket_send_iov( udp->sock_send, &udp->addr_send, iov, iov_count );
}

int
one_udp_dispatch( OneUDP                *udp,
                  OneApp                *app,
                  const OnePacketHeader *header,
                  const struct iovec    *iov,
                  size_t                 iov_count )
{
     int                ret;
     struct iovec      *iov_copy;
     OneUDPMessageType  type = OUMT_DISPATCH;

     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     iov_copy = kmalloc( sizeof(struct iovec) * (iov_count + 2), GFP_KERNEL );
     if (!iov_copy)
          return -ENOMEM;

     iov_copy[0].iov_base = (void*) &type;
     iov_copy[0].iov_len  = sizeof(OneUDPMessageType);

     iov_copy[1].iov_base = (void*) header;
     iov_copy[1].iov_len  = sizeof(OnePacketHeader);

     memcpy( iov_copy + 2, iov, sizeof(struct iovec) * iov_count );

     ret = one_udp_send_message( udp, iov_copy, iov_count + 2 );
     if (ret < 0)
          printk( KERN_ERR "One/Core: one_udp_send_message() failed! (error %d)\n", ret );

     kfree( iov_copy );

     return ret;
}

