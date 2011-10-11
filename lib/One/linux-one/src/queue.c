/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Queue receives packets for a specific QID
   
   Virtual Queues have no actual storage,
   i.e. purely forwarding packets to other Queues
*/

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#include <linux/smp_lock.h>
#endif
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/uio.h>

#include <asm/uaccess.h>

#include <linux/one.h>

#include "app.h"
#include "debug.h"
#include "fifo.h"
#include "onedev.h"
#include "list.h"
#include "packet.h"
#include "queue.h"
#include "target.h"


typedef struct {
     DirectLink     link;

     OneQID         target_id;

     OneTarget     *target;

     int            count;         /* number of attach calls */
} QueueNode;

/******************************************************************************/

static inline QueueNode *
get_node(OneQueue * queue, OneQID target_id)
{
     QueueNode *node;

     direct_list_foreach (node, queue->nodes) {
          if (node->target_id == target_id)
               return node;
     }

     return NULL;
}

/******************************************************************************/

typedef struct {
     OneApp      *oneapp;
     OneQueueNew *queue_new;
} OneQueueConstructContext;

static int
one_queue_construct(OneEntry * entry, void *ctx, void *create_ctx)
{
     int                       ret;
     OneQueue                 *queue   = (OneQueue *) entry;
     OneQueueConstructContext *context = (OneQueueConstructContext *) create_ctx;

     queue->one_id = (context->queue_new->flags & 0x10000000) ? 0 : context->oneapp->one_id;
     queue->flags  = context->queue_new->flags;

     if (context->queue_new->queue_id)
          queue->entry.id = context->queue_new->queue_id;

     if (!(context->queue_new->flags & ONE_QUEUE_VIRTUAL)) {
          ret = OneApp_CreateTarget( context->oneapp, queue, &queue->target );
          if (ret)
               return ret;
     }

     return 0;
}

static void
one_queue_destruct(OneEntry * entry, void *ctx)
{
     OneQueue  *queue = (OneQueue *) entry;
     OneDev    *dev   = (OneDev *) ctx;
     QueueNode *node, *next;

     ONE_DEBUG( "%s( QID %u )\n", __FUNCTION__, queue->entry.id );

     one_queue_detach_all( dev, queue->entry.id );

     if (queue->target)
          queue->target->Destroy( queue->target );

     direct_list_foreach_safe (node, next, queue->nodes) {
//          node->target->Destroy( node->target );

          one_core_free( one_core, node );
     }
}

static void
one_queue_print(OneEntry * entry, void *ctx, struct seq_file *p)
{
     int         num   = 0;
     OneQueue   *queue = (OneQueue *) entry;
     DirectLink *node  = queue->nodes;

     direct_list_foreach (node, queue->nodes)
          num++;

     seq_printf( p, "%5dx dispatch, %d nodes%s\n", queue->dispatch_count,
                 num, queue->destroyed ? "  DESTROYED" : "" );
}

ONE_ENTRY_CLASS( OneQueue, queue, one_queue_construct,
                 one_queue_destruct, one_queue_print )

/******************************************************************************/

int
one_queue_init(OneDev * dev)
{
     one_entries_init(&dev->queue, &queue_class, dev, dev);

     one_entries_create_proc_entry(dev, "queues", &dev->queue);

     return 0;
}

void
one_queue_deinit(OneDev * dev)
{
     one_entries_destroy_proc_entry( dev, "queues" );

     one_entries_deinit(&dev->queue);
}

/******************************************************************************/

int
one_queue_new( OneApp      *oneapp,
               OneQueueNew *queue_new )
{
     OneQueueConstructContext  context;

     context.oneapp    = oneapp;
     context.queue_new = queue_new;

     if (queue_new->queue_id) {
          int       ret;
          OneQID    qid;
          OneQueue *queue;

          if (!one_core_is_local( one_core, queue_new->queue_id ))
               return -EINVAL;

          ret = one_queue_lookup( &oneapp->dev->queue, queue_new->queue_id, &queue );
          if (ret == 0) {
               printk( KERN_ERR "One: Queue with ID %u already exists! (OneID %u)\n", queue_new->queue_id, queue->one_id );
               return -EEXIST;
          }

          ret = one_entry_create( &oneapp->dev->queue, &qid, &context );
          if (ret)
               return ret;

          D_ASSERT( qid == queue_new->queue_id );
     }

     return one_entry_create( &oneapp->dev->queue, &queue_new->queue_id, &context );
}

int
one_queue_destroy( OneApp          *oneapp,
                   OneQueueDestroy *queue_destroy )
{
     int ret;
     OneQueue *queue;

     ret = one_queue_lookup(&oneapp->dev->queue, queue_destroy->queue_id, &queue);
     if (ret)
          return ret;

     if (queue->destroyed)
          return -EIDRM;

     queue->destroyed = true;

     if (!queue->nodes)
          one_entry_destroy_locked(&oneapp->dev->queue, &queue->entry);

     return 0;
}

int
one_queue_attach( OneApp         *oneapp,
                  OneQueueAttach *queue_attach )
{
     int        ret;
     QueueNode *node;
     OneQueue  *queue;
     OneQueue  *target_queue;

     ONE_DEBUG( "%s( %u to %u )\n", __FUNCTION__, queue_attach->target_id, queue_attach->queue_id );

     if (!one_core_is_local( one_core, queue_attach->target_id )) {
          /* Don't allow remote queue attaching to remote queue */
          if (!one_core_is_local( one_core, queue_attach->queue_id ))
               return -EINVAL;

          /* Attach remote queue to local queue */
          return one_core_attach( one_core, oneapp, queue_attach );
     }

     if (!one_core_is_local( one_core, queue_attach->queue_id )) {
          if (!oneapp) {
               OneApp *app;

               ret = one_queue_lookup(&one_devs[0].queue, queue_attach->target_id, &target_queue);
               if (ret)
                    return ret;

               direct_list_foreach (app, one_devs[0].apps) {
                    if (app->one_id == target_queue->one_id) {
                         oneapp = app;
                         break;
                    }
               }

               if (!oneapp)
                    return -EIDRM;
          }

          /* Attach local queue to remote queue */
          return one_core_attach( one_core, oneapp, queue_attach );
     }

     ret = one_queue_lookup(&oneapp->dev->queue, queue_attach->queue_id, &queue);
     if (ret)
          return ret;

     ret = one_queue_lookup(&oneapp->dev->queue, queue_attach->target_id, &target_queue);
     if (ret)
          return ret;

     /* may only attach to virtual queues */
     if (!(queue->flags & ONE_QUEUE_VIRTUAL))
          return -EINVAL;

     /* may only attach non-virtual queues */
     if (target_queue->flags & ONE_QUEUE_VIRTUAL)
          return -EINVAL;

     if (queue->destroyed)
          return -EIDRM;

     oneapp->dev->stat.queue_attach++;

     node = get_node(queue, queue_attach->target_id);
     if (!node) {
          node = one_core_malloc( one_core, sizeof(QueueNode) );
          if (!node)
               return -ENOMEM;

          node->target_id = queue_attach->target_id;
          node->target    = target_queue->target;
          node->count     = 1;

          direct_list_prepend(&queue->nodes, &node->link);
     }
     else {
          node->count++;
     }

     return 0;
}

int
one_queue_detach( OneApp         *oneapp,
                  OneQueueDetach *queue_detach )
{
     int ret;
     QueueNode *node;
     OneQueue *queue;

     ONE_DEBUG( "%s( %u from %u )\n", __FUNCTION__, queue_detach->target_id, queue_detach->queue_id );

     if (!one_core_is_local( one_core, queue_detach->target_id ))
          return -EINVAL;

     if (!one_core_is_local( one_core, queue_detach->queue_id ))
          return one_core_detach( one_core, oneapp, queue_detach );

     ret = one_queue_lookup(&oneapp->dev->queue, queue_detach->queue_id, &queue);
     if (ret)
          return ret;

     oneapp->dev->stat.queue_detach++;

     node = get_node(queue, queue_detach->target_id);
     if (!node)
          return -EIO;

     if (!--node->count) {
          direct_list_remove(&queue->nodes, &node->link);
          one_core_free( one_core, node );
     }

     if (queue->destroyed && !queue->nodes)
          one_entry_destroy_locked(&oneapp->dev->queue, &queue->entry);

     return 0;
}

int
one_queue_dispatch( OneApp           *oneapp,
                    OneQueueDispatch *queue_dispatch )
{
     int           ret = 0;
     struct iovec  iov_static[40];
     struct iovec *iov = iov_static;
     OneDev       *dev = &one_devs[0];

     ONE_DEBUG( "%s( QID %u, size %u )\n", __FUNCTION__, queue_dispatch->header.queue_id, queue_dispatch->header.size );

     /*
      * Allocate/copy IO vector
      */
     if (queue_dispatch->iov_count > D_ARRAY_SIZE(iov_static)) {
          iov = kmalloc( sizeof(struct iovec) * queue_dispatch->iov_count, GFP_KERNEL );
          if (!iov) {
               ret = -ENOMEM;
               goto out;
          }
     }

     if (copy_from_user( iov, queue_dispatch->iov, sizeof(struct iovec) * queue_dispatch->iov_count )) {
          ret = -EFAULT;
          goto out;
     }

     if (one_core_is_local( one_core, queue_dispatch->header.queue_id )) {
          OneQueue  *queue;
          QueueNode *node;

          ONE_DEBUG( "  -> local\n" );

          ret = one_queue_lookup( &dev->queue, queue_dispatch->header.queue_id, &queue );
          if (ret)
               goto out;

          if (queue->destroyed) {
               ret = -EIDRM;
               goto out;
          }

          queue->dispatch_count++;

          dev->stat.queue_dispatch++;

          /*
           * Dispatch to queue target
           */
          if (queue->target) {
               ONE_DEBUG( "  -> dispatching to queue target\n" );

               ret = queue->target->Dispatch( queue->target, &queue_dispatch->header, iov, queue_dispatch->iov_count );
               if (ret)
                    goto out;
          }

          /*
           * Dispatch to node targets
           */
          direct_list_foreach (node, queue->nodes) {
               OnePacketHeader header = queue_dispatch->header;

               ONE_DEBUG( "  -> dispatching to node target\n" );

               header.queue_id = node->target_id;

               ret = node->target->Dispatch( node->target, &header, iov, queue_dispatch->iov_count );
               if (ret)
                    goto out;
          }
     }
     else {
          ONE_DEBUG( "  -> remote\n" );

          ret = one_core_dispatch( one_core, oneapp, &queue_dispatch->header, iov, queue_dispatch->iov_count );
     }

out:
     if (iov != iov_static)
          kfree( iov );

     return ret;
}

int
one_queue_receive( OneApp          *oneapp,
                   OneQueueReceive *queue_receive )
{
     int           ret = 0;
     OneQID        ids_static[100];
     OneQID       *ids = ids_static;
     struct iovec  iov_static[10];
     struct iovec *iov = iov_static;

     /*
      * Allocate/copy Queue IDs
      */
     if (queue_receive->ids_count > D_ARRAY_SIZE(ids_static)) {
          ids = kmalloc( sizeof(OneQID) * queue_receive->ids_count, GFP_KERNEL );
          if (!ids) {
               ret = -ENOMEM;
               goto out;
          }
     }

     if (copy_from_user( ids, queue_receive->ids, sizeof(OneQID) * queue_receive->ids_count )) {
          ret = -EFAULT;
          goto out;
     }

     /*
      * Allocate/copy IO vector
      */
     if (queue_receive->iov_count > D_ARRAY_SIZE(iov_static)) {
          iov = kmalloc( sizeof(struct iovec) * queue_receive->iov_count, GFP_KERNEL );
          if (!iov) {
               ret = -ENOMEM;
               goto out;
          }
     }

     if (copy_from_user( iov, queue_receive->iov, sizeof(struct iovec) * queue_receive->iov_count )) {
          ret = -EFAULT;
          goto out;
     }

     /*
      * Actual Receive implementation
      */
     ret = OneApp_Receive( oneapp, ids, queue_receive->ids_count, iov, queue_receive->iov_count, &queue_receive->ret_received );


out:
     if (iov != iov_static)
          kfree( iov );

     if (ids != ids_static)
          kfree( ids );

     return ret;
}

int
one_queue_dispatch_receive( OneApp                  *oneapp,
                            OneQueueDispatchReceive *queue_dispatch_receive )
{
     int               ret = 0;
     size_t            i;
     OneQueueDispatch  dispatches_static[30];
     OneQueueDispatch *dispatches = dispatches_static;

     /*
      * Allocate/copy Queue Dispatches
      */
     if (queue_dispatch_receive->dispatch_count > D_ARRAY_SIZE(dispatches_static)) {
          dispatches = kmalloc( sizeof(OneQueueDispatch) * queue_dispatch_receive->dispatch_count, GFP_KERNEL );
          if (!dispatches) {
               ret = -ENOMEM;
               goto out;
          }
     }

     if (copy_from_user( dispatches, queue_dispatch_receive->dispatch, sizeof(OneQueueDispatch) * queue_dispatch_receive->dispatch_count )) {
          ret = -EFAULT;
          goto out;
     }

     /*
      * Execute Queue Dispatches
      */
     for (i=0; i<queue_dispatch_receive->dispatch_count; i++) {
          ret = one_queue_dispatch( oneapp, &dispatches[i] );
          if (ret) {
               queue_dispatch_receive->dispatch_count = i;
               goto out;
          }
     }

     /*
      * Execute Queue Receive
      */
     ret = one_queue_receive( oneapp, &queue_dispatch_receive->receive );


out:
     if (dispatches != dispatches_static)
          kfree( dispatches );

     return ret;
}

int
one_queue_wakeup( OneApp         *oneapp,
                  OneQueueWakeUp *queue_wakeup )
{
     int           ret = 0;
     OneQID        ids_static[100];
     OneQID       *ids = ids_static;
     unsigned int  i;

     /*
      * Allocate/copy Queue IDs
      */
     if (queue_wakeup->ids_count > D_ARRAY_SIZE(ids_static)) {
          ids = kmalloc( sizeof(OneQID) * queue_wakeup->ids_count, GFP_KERNEL );
          if (!ids) {
               ret = -ENOMEM;
               goto out;
          }
     }

     if (copy_from_user( ids, queue_wakeup->ids, sizeof(OneQID) * queue_wakeup->ids_count )) {
          ret = -EFAULT;
          goto out;
     }

     /*
      * Actual WakeUp implementation
      */
     for (i=0; i<queue_wakeup->ids_count; i++) {
          OneQueue *queue;

          if (!one_core_is_local( one_core, ids[i] )) {
               ret = -EINVAL;
               break;
          }

          ret = one_queue_lookup( &oneapp->dev->queue, ids[i], &queue );
          if (ret)
               break;

          if (queue->target && queue->target->WakeUp)
               queue->target->WakeUp( queue->target );
     }


out:
     if (ids != ids_static)
          kfree( ids );

     return ret;
}

void
one_queue_destroy_all( OneApp *oneapp )
{
     OneDev   *dev = oneapp->dev;
     OneQueue *queue, *next;

     ONE_DEBUG( "%s( dev %p, one_id %lu )\n", __FUNCTION__, oneapp->dev, oneapp->one_id );

     direct_list_foreach_safe (queue, next, dev->queue.list) {
          if (queue->one_id == oneapp->one_id) {
               ONE_DEBUG( "  -> destroying QID %u\n", queue->entry.id );

               one_entry_destroy_locked( &dev->queue, &queue->entry );
          }
     }
}

void
one_queue_detach_all( OneDev *dev,
                      OneQID  queue_id )
{
     OneQueue *queue, *next;

     ONE_DEBUG( "%s( dev %p, QID %lu )\n", __FUNCTION__, dev, queue_id );

     direct_list_foreach_safe (queue, next, dev->queue.list) {
          QueueNode *node;

          ONE_DEBUG( "  -> checking QID %u... (queue %p, next %p)\n", queue->entry.id, queue, next );

          direct_list_foreach (node, queue->nodes) {
               if (node->target_id == queue_id) {
                    ONE_DEBUG( "  -> detaching from QID %u\n", queue->entry.id );

                    direct_list_remove( &queue->nodes, &node->link );
                    one_core_free( one_core, node );
                    break;
               }
          }

          if (queue->destroyed && !queue->nodes) {
               ONE_DEBUG( "  -> destroying QID %u\n", queue->entry.id );

               one_entry_destroy_locked( &dev->queue, &queue->entry );
          }
     }
}

/*********************************************************************************************************************/

int
one_queue_add_target( OneQID     queue_id,
                      OneQID     target_id,
                      OneTarget *target )
{
     int        ret;
     QueueNode *node;
     OneQueue  *queue;
     OneDev    *dev = &one_devs[0];

     if (!one_core_is_local( one_core, queue_id ))
          return -EINVAL;

     ret = one_queue_lookup( &dev->queue, queue_id, &queue );
     if (ret)
          return ret;

     /* may only attach to virtual queues */
     if (!(queue->flags & ONE_QUEUE_VIRTUAL))
          return -EINVAL;

     if (queue->destroyed)
          return -EIDRM;

     node = get_node(queue, target_id);
     if (!node) {
          node = one_core_malloc( one_core, sizeof(QueueNode) );
          if (!node)
               return -ENOMEM;

          node->target_id = target_id;
          node->target    = target;
          node->count     = 1;

          direct_list_prepend(&queue->nodes, &node->link);
     }
     else {
          node->count++;
     }

     return 0;
}

int
one_queue_remove_target( OneQID     queue_id,
                         OneQID     target_id,
                         OneTarget *target )
{
     int        ret;
     QueueNode *node;
     OneQueue  *queue;
     OneDev    *dev = &one_devs[0];

     if (!one_core_is_local( one_core, queue_id ))
          return -EINVAL;

     ret = one_queue_lookup( &dev->queue, queue_id, &queue );
     if (ret)
          return ret;

     node = get_node(queue, target_id);
     if (!node)
          return -EIO;

     if (!--node->count) {
          direct_list_remove(&queue->nodes, &node->link);

          one_core_free( one_core, node );
     }

     if (queue->destroyed && !queue->nodes)
          one_entry_destroy_locked(&dev->queue, &queue->entry);

     return 0;
}

