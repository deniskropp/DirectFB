/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   One Application is a user (process) of One
*/

//#define ONE_ENABLE_DEBUG

#include <linux/version.h>
#include <linux/module.h>
#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/sched.h>


#include "app.h"
#include "fifo.h"
#include "onedev.h"
#include "packet.h"
#include "queue.h"
#include "target.h"


/******************************************************************************/

static void
OneAppTarget_Destroy( OneTarget *target )
{
     OneAppTargetData *data = (OneAppTargetData*) target->data;
     OnePacket        *packet;

     D_MAGIC_ASSERT( target, OneTarget );

     while ((packet = (OnePacket *) one_fifo_get(&data->packets)) != NULL) {
          D_MAGIC_ASSERT( packet, OnePacket );

          OnePacket_Free( packet );
     }

     while ((packet = (OnePacket *) one_fifo_get(&data->free_packets)) != NULL) {
          D_MAGIC_ASSERT( packet, OnePacket );

          OnePacket_Free( packet );
     }

     OneTarget_Destroy( target );
}

static int
OneAppTarget_Dispatch( OneTarget          *target,
                       OnePacketHeader    *header,
                       const struct iovec *iov,
                       size_t              iov_count )
{
     int               ret;
     OneAppTargetData *data = (OneAppTargetData*) target->data;
     OnePacket        *packet;

     ONE_DEBUG( "%s( %p, header %p, iov %p, iov_count %zu )\n", __FUNCTION__, target, header, iov, iov_count );

     D_MAGIC_ASSERT( target, OneTarget );

     while (data->packets.count > 100) {
          one_core_wq_wait( one_core, &data->app->wait_for_free, NULL );

          if (signal_pending( current ))
               return -EINTR;
     }

     if (0&&data->packets.items) {
          packet = (OnePacket*) data->packets.items->prev;
          if (packet) {
               ret = OnePacket_Write( packet, header, iov, iov_count );
               if (ret == 0) {
                    printk(KERN_DEBUG "%s: successfully appended data (once this works, remove message)\n",__FUNCTION__);
                    goto out;
               }

               one_core_wq_wake( one_core, &data->app->wait_for_packets );
          }
     }

     packet = OnePacket_New();
     if (!packet)
          return -ENOMEM;

     packet->header.queue_id = data->queue_id;

     ret = OnePacket_Write( packet, header, iov, iov_count );
     if (ret) {
          OnePacket_Free( packet );
          return ret;
     }

     packet->flush = true;

     one_fifo_put( &data->packets, &packet->link );

out:
     /*
      * Link data to list of receiving targets if needed
      */
     if (!data->link.prev)
          direct_list_append( &data->app->recv_data, &data->link );

     one_core_wq_wake( one_core, &data->app->wait_for_packets );

     return 0;
}

static int
OneAppTarget_WakeUp( OneTarget *target )
{
     OneAppTargetData *data = (OneAppTargetData*) target->data;

     ONE_DEBUG( "%s( %p )\n", __FUNCTION__, target );

     D_MAGIC_ASSERT( target, OneTarget );

     /*
      * Set wake up flag for receiver
      */
     data->wakeup = true;

     /*
      * Link data to list of receiving targets if needed
      */
     if (!data->link.prev)
          direct_list_append( &data->app->recv_data, &data->link );

     /*
      * Wake up all receivers to check for wake up
      */
     one_core_wq_wake( one_core, &data->app->wait_for_packets );

     return 0;
}

typedef struct {
     OneApp   *app;
     OneQID    queue_id;
} OneAppTarget_InitContext;

static int
OneAppTarget_Init( OneTarget *target,
                   void      *ctx )
{
     OneAppTarget_InitContext *context = (OneAppTarget_InitContext*) ctx;
     OneAppTargetData         *data    = (OneAppTargetData*) target->data;

     data->app      = context->app;
     data->queue_id = context->queue_id;

     one_fifo_reset( &data->packets );
     one_fifo_reset( &data->free_packets );

     target->Destroy  = OneAppTarget_Destroy;
     target->Dispatch = OneAppTarget_Dispatch;
     target->WakeUp   = OneAppTarget_WakeUp;

     return 0;
}

/******************************************************************************/

int
OneApp_New( OneDev  *dev,
            OneID    one_id,
            OneApp **ret_oneapp )
{
     OneApp *oneapp;

     oneapp = one_core_malloc( one_core, sizeof(OneApp) );
     if (!oneapp)
          return -ENOMEM;

     oneapp->dev       = dev;
     oneapp->one_id    = one_id;
     oneapp->recv_data = NULL;

     oneapp->link.magic = 0;
     oneapp->magic      = 0;

     one_core_wq_init( one_core, &oneapp->wait_for_free );
     one_core_wq_init( one_core, &oneapp->wait_for_packets );

     D_MAGIC_SET( oneapp, OneApp );

     *ret_oneapp = oneapp;

     return 0;
}

void
OneApp_Destroy( OneApp *oneapp )
{
     D_MAGIC_ASSERT( oneapp, OneApp );

     one_queue_destroy_all( oneapp );

     one_core_wq_deinit( one_core, &oneapp->wait_for_free );
     one_core_wq_deinit( one_core, &oneapp->wait_for_packets );

     D_MAGIC_CLEAR( oneapp );

     one_core_free( one_core, oneapp );
}

int
OneApp_CreateTarget( OneApp     *oneapp,
                     OneQueue   *queue,
                     OneTarget **ret_target )
{
     int                       ret;
     OneTarget                *target;
     OneAppTarget_InitContext  context;

     D_MAGIC_ASSERT( oneapp, OneApp );

     context.app      = oneapp;
     context.queue_id = queue->entry.id;

     ret = OneTarget_New( OneAppTarget_Init, &context, sizeof(OneAppTargetData), &target );
     if (ret)
          return ret;

     *ret_target = target;

     return 0;
}

static int
receive_from( OneApp             *oneapp,
              OneAppTargetData   *data,
              const struct iovec *iov,
              size_t              iov_count,
              size_t              offset,
              size_t              length,
              size_t             *ret_received )
{
     int    ret      = 0;
     size_t received = 0;

     while (data->packets.count) {
          OnePacket *packet = (OnePacket*) data->packets.items;

          if (sizeof(OnePacketHeader) + packet->header.size > length) {
               ret = -E2BIG;
               goto out;
          }

          ret = OnePacket_Read( packet, iov, iov_count, offset + received );
          if (ret)
               goto out;

          one_fifo_get( &data->packets );

          OnePacket_Free( packet );

          received += packet->header.size + sizeof(OnePacketHeader);
          length   -= packet->header.size + sizeof(OnePacketHeader);
     }

     direct_list_remove( &oneapp->recv_data, &data->link );

     one_core_wq_wake( one_core, &data->app->wait_for_free );

out:
     *ret_received = received;

     return ret;
}

int
OneApp_Receive( OneApp             *oneapp,
                const OneQID       *ids,
                size_t              ids_count,
                const struct iovec *iov,
                size_t              iov_count,
                size_t             *ret_received,
                int                 timeout_ms )
{
     int               ret = 0;
     size_t            i;
     bool              wakeup   = false;
     size_t            received = 0;
     size_t            length   = 0;
     OneAppTargetData *data;

     /*
      * Calculate total length of IO vector
      */
     for (i=0; i<iov_count; i++)
          length += iov[i].iov_len;

     /*
      * Until we receive something or get woken up... 
      *  
      * If data is available and we got woken up we just return the data.
      */
     while (!received && !wakeup) {
          /*
           * Check each receiving target against passed Queue IDs
           */
          direct_list_foreach (data, oneapp->recv_data) {
               for (i=0; i<ids_count; i++) {
                    if (data->queue_id == ids[i]) {
                         size_t rec = 0;

                         /* Check and clear wake up flags */
                         if (data->wakeup) {
                              data->wakeup = false;

                              wakeup = true;
                         }

                         one_queue_stamp_receive( oneapp, data->queue_id );

                         /*
                          * Receive from matching target
                          */
                         ret = receive_from( oneapp, data, iov, iov_count, received, length, &rec );

                         received += rec;
                         length   -= rec;

                         if (ret < 0)
                              goto out;
                    }
               }
          }

          /*
           * Wait when nothing was received
           */
          if (!received && !wakeup) {
               if (timeout_ms == 0) {
                    one_core_wq_wait( one_core, &oneapp->wait_for_packets, NULL );
               }
               else if (timeout_ms > 0) {
                    one_core_wq_wait( one_core, &oneapp->wait_for_packets, &timeout_ms );

                    if (!timeout_ms) {
                         ret = -ETIMEDOUT;
                         break;
                    }
               }
               else {
                    ret = 0;
                    break;
               }
          }

          if (signal_pending( current )) {
               ret = -EINTR;
               break;
          }
     }

out:
     *ret_received = received;

     return received ? 0 : ret;
}

