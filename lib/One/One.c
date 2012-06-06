/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/hash.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <linux/one.h>

#include "One.h"

#ifndef ONEDEV
#define ONEDEV "/dev/one0"
#endif


D_DEBUG_DOMAIN( One_Main,   "One/Main",   "One Main" );
D_DEBUG_DOMAIN( One_Queue,  "One/Queue",  "One Queue" );
D_DEBUG_DOMAIN( One_Thread, "One/Thread", "One Thread" );

/*********************************************************************************************************************/

static DirectMutex  one_lock;
static unsigned int one_refs;
static int          one_fd = -1;   /* File descriptor of the One Kernel Device */

__attribute__((constructor))
static void
__One_Init( void )
{
     direct_mutex_init( &one_lock );
}

/*********************************************************************************************************************/

DirectResult
One_Initialize()
{
     DirectResult ret = DR_OK;

     D_DEBUG_AT( One_Main, "%s()\n", __FUNCTION__ );

     ret = direct_initialize();
     if (ret)
          return ret;

     direct_mutex_lock( &one_lock );

     if (!one_refs) {
          /* Open the One Kernel Device. */
          one_fd = open( ONEDEV, O_RDWR );
          if (one_fd < 0) {
               ret = errno2result( errno );
               D_PERROR( "One/Main: Opening " ONEDEV " failed!\n" );
          }
     }

     if (ret == DR_OK)
          one_refs++;

     direct_mutex_unlock( &one_lock );

     return ret;
}

DirectResult
One_Shutdown()
{
     D_DEBUG_AT( One_Main, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &one_lock );

     if (!--one_refs) {
          /* Close the One Kernel Device. */
          close( one_fd );
          one_fd = -1;
     }

     direct_mutex_unlock( &one_lock );

     direct_shutdown();

     return DR_OK;
}

/*********************************************************************************************************************/

DirectResult
OneQueue_New( OneQueueFlags  flags,
              OneQID         queue_id,
              OneQID        *ret_id )
{
     DirectResult ret = DR_OK;
     OneQueueNew  queue_new;

     D_DEBUG_AT( One_Queue, "%s( flags 0x%08x )\n", __FUNCTION__, flags );

     D_ASSERT( ret_id != NULL );

     queue_new.flags    = flags;
     queue_new.queue_id = queue_id;

     while (ioctl( one_fd, ONE_QUEUE_NEW, &queue_new )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_NEW failed!\n" );

          goto error;
     }

     D_DEBUG_AT( One_Queue, "  -> QID 0x%08x\n", queue_new.queue_id );

     *ret_id = queue_new.queue_id;

error:
     return ret;
}

DirectResult
OneQueue_Destroy( OneQID queue_id )
{
     DirectResult    ret = DR_OK;
     OneQueueDestroy queue_destroy;

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x )\n", __FUNCTION__, queue_id );

     queue_destroy.queue_id = queue_id;

     while (ioctl( one_fd, ONE_QUEUE_DESTROY, &queue_destroy )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_DESTROY failed!\n" );

          goto error;
     }

error:
     return ret;
}

DirectResult
OneQueue_Attach( OneQID queue_id,
                 OneQID target_id )
{
     DirectResult   ret = DR_OK;
     OneQueueAttach queue_attach;

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x, target QID 0x%08x )\n", __FUNCTION__, queue_id, target_id );

     queue_attach.queue_id  = queue_id;
     queue_attach.target_id = target_id;

     while (ioctl( one_fd, ONE_QUEUE_ATTACH, &queue_attach )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_ATTACH failed!\n" );

          goto error;
     }

error:
     return ret;
}

DirectResult
OneQueue_Detach( OneQID queue_id,
                 OneQID target_id )
{
     DirectResult   ret = DR_OK;
     OneQueueDetach queue_detach;

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x, target QID 0x%08x )\n", __FUNCTION__, queue_id, target_id );

     queue_detach.queue_id  = queue_id;
     queue_detach.target_id = target_id;

     while (ioctl( one_fd, ONE_QUEUE_DETACH, &queue_detach )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_DETACH failed!\n" );

          goto error;
     }

error:
     return ret;
}

DirectResult
OneQueue_Dispatch( OneQID  queue_id,
                   void   *data,
                   size_t  length )
{
     DirectResult     ret = DR_OK;
     OneQueueDispatch queue_dispatch;
     struct iovec     iov[1];

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x, data %p, length %zu )\n", __FUNCTION__, queue_id, data, length );

     D_ASSERT( data != NULL );

     iov[0].iov_base = data;
     iov[0].iov_len  = length;

     queue_dispatch.header.queue_id     = queue_id;
     queue_dispatch.header.flags        = ONE_PACKET_NO_FLAGS;
     queue_dispatch.header.size         = length;
     queue_dispatch.header.uncompressed = length;

     queue_dispatch.iov       = iov;
     queue_dispatch.iov_count = 1;

     while (ioctl( one_fd, ONE_QUEUE_DISPATCH, &queue_dispatch )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_DISPATCH failed!\n" );

          goto error;
     }

error:
     return ret;
}

DirectResult
OneQueue_DispatchV( OneQID         queue_id,
                    void         **datas,
                    size_t        *lengths,
                    size_t         count )
{
     DirectResult     ret = DR_OK;
     OneQueueDispatch queue_dispatch;
     size_t           i;
     size_t           length = 0;
     struct iovec     iov[count];

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x, datas %p, lengths %p, count %zu )\n", __FUNCTION__, queue_id, datas, lengths, count );

     D_ASSERT( datas != NULL );
     D_ASSERT( lengths != NULL );
     D_ASSERT( count > 0 );

     for (i=0; i<count; i++) {
          iov[i].iov_base = datas[i];
          iov[i].iov_len  = lengths[i];

          length += lengths[i];
     }

     D_DEBUG_AT( One_Queue, "  -> total length %zu\n", length );

     queue_dispatch.header.queue_id     = queue_id;
     queue_dispatch.header.flags        = ONE_PACKET_NO_FLAGS;
     queue_dispatch.header.size         = length;
     queue_dispatch.header.uncompressed = length;

     queue_dispatch.iov       = iov;
     queue_dispatch.iov_count = count;

     while (ioctl( one_fd, ONE_QUEUE_DISPATCH, &queue_dispatch )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_DISPATCH failed!\n" );

          goto error;
     }

error:
     return ret;
}

DirectResult
OneQueue_Receive( const OneQID *queue_ids,
                  unsigned int  queue_count,
                  void         *buf,
                  size_t        length,
                  size_t       *ret_received,
                  bool          headerless,
                  int           timeout_ms )
{
     DirectResult    ret = DR_OK;
     OneQueueReceive queue_receive;
     struct iovec    iov[2];
     OnePacketHeader header;

#if D_DEBUG_ENABLED
     unsigned int    i;

     D_DEBUG_AT( One_Queue, "%s( ids %p, count %u, buf %p, length %zu )\n", __FUNCTION__, queue_ids, queue_count, buf, length );

     for (i=0; i<queue_count; i++)
          D_DEBUG_AT( One_Queue, "  -> QID 0x%08x\n", queue_ids[i] );
#endif

     D_ASSERT( queue_ids != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_received != NULL );

     if (headerless) {
          iov[0].iov_base = &header;
          iov[0].iov_len  = sizeof(header);

          iov[1].iov_base = buf;
          iov[1].iov_len  = length;
     }
     else {
          iov[0].iov_base = buf;
          iov[0].iov_len  = length;
     }

     queue_receive.ids       = queue_ids;
     queue_receive.ids_count = queue_count;

     queue_receive.iov       = iov;
     queue_receive.iov_count = headerless ? 2 : 1;

     queue_receive.timeout_ms = timeout_ms;

     while (ioctl( one_fd, ONE_QUEUE_RECEIVE, &queue_receive )) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    D_DEBUG_AT( One_Queue, "  -> TIMEOUT\n" );
                    ret = DR_TIMEOUT;
                    goto error;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_RECEIVE failed!\n" );

          goto error;
     }

     D_DEBUG_AT( One_Queue, "  -> received %zu\n", queue_receive.ret_received );

     if (headerless)
          *ret_received = queue_receive.ret_received - sizeof(header);
     else
          *ret_received = queue_receive.ret_received;

error:
     return ret;
}

DirectResult
OneQueue_ReceiveV( const OneQID  *queue_ids,
                   unsigned int   queue_count,
                   void         **buf,
                   size_t        *length,
                   size_t         count,
                   size_t        *ret_received,
                   bool           headerless,
                   int            timeout_ms )
{
     DirectResult    ret = DR_OK;
     unsigned int    i;
     OneQueueReceive queue_receive;
     struct iovec    iov[count+1];
     OnePacketHeader header;

#if D_DEBUG_ENABLED
     D_DEBUG_AT( One_Queue, "%s( ids %p, count %u, buf %p, length %p, count %zu )\n", __FUNCTION__, queue_ids, queue_count, buf, length, count );

     for (i=0; i<queue_count; i++)
          D_DEBUG_AT( One_Queue, "  -> QID 0x%08x\n", queue_ids[i] );
#endif

     D_ASSERT( queue_ids != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_received != NULL );

     if (headerless) {
          iov[0].iov_base = &header;
          iov[0].iov_len  = sizeof(header);

          for (i=0; i<count; i++) {
               iov[i+1].iov_base = buf[i];
               iov[i+1].iov_len  = length[i];
          }
     }
     else {
          for (i=0; i<count; i++) {
               iov[i+0].iov_base = buf[i];
               iov[i+0].iov_len  = length[i];
          }
     }

     queue_receive.ids       = queue_ids;
     queue_receive.ids_count = queue_count;

     queue_receive.iov       = iov;
     queue_receive.iov_count = headerless ? (count+1) : count;

     queue_receive.timeout_ms = timeout_ms;

     while (ioctl( one_fd, ONE_QUEUE_RECEIVE, &queue_receive )) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    D_DEBUG_AT( One_Queue, "  -> TIMEOUT\n" );
                    ret = DR_TIMEOUT;
                    goto error;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_RECEIVE failed!\n" );

          goto error;
     }

     D_DEBUG_AT( One_Queue, "  -> received %zu\n", queue_receive.ret_received );

     if (headerless)
          *ret_received = queue_receive.ret_received - sizeof(header);
     else
          *ret_received = queue_receive.ret_received;

error:
     return ret;
}

DirectResult
OneQueue_DispatchReceive( OneQID          queue_id,
                          void           *data,
                          size_t          data_length,
                          const OneQID   *queue_ids,
                          unsigned int    queue_count,
                          void           *buf,
                          size_t          buf_length,
                          size_t         *ret_received,
                          bool            headerless,
                          int             timeout_ms )
{
     DirectResult            ret = DR_OK;
     OneQueueDispatch        queue_dispatch;
     OneQueueDispatchReceive queue_dispatch_receive;
     struct iovec            data_iov[1];
     struct iovec            buf_iov[2];
     OnePacketHeader         header;

#if D_DEBUG_ENABLED
     unsigned int    i;

     D_DEBUG_AT( One_Queue, "%s( QID 0x%08x, data %p, length %zu, ids %p, count %u, buf %p, length %zu )\n",
                 __FUNCTION__, queue_id, data, data_length, queue_ids, queue_count, buf, buf_length );

     for (i=0; i<queue_count; i++)
          D_DEBUG_AT( One_Queue, "  -> QID 0x%08x\n", queue_ids[i] );
#endif

     D_ASSERT( data != NULL );
     D_ASSERT( queue_ids != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_received != NULL );

     queue_dispatch_receive.dispatch       = &queue_dispatch;
     queue_dispatch_receive.dispatch_count = 1;

     data_iov[0].iov_base = data;
     data_iov[0].iov_len  = data_length;

     queue_dispatch.header.queue_id     = queue_id;
     queue_dispatch.header.flags        = ONE_PACKET_NO_FLAGS;
     queue_dispatch.header.size         = data_length;
     queue_dispatch.header.uncompressed = data_length;

     queue_dispatch.iov       = data_iov;
     queue_dispatch.iov_count = 1;

     if (headerless) {
          buf_iov[0].iov_base = &header;
          buf_iov[0].iov_len  = sizeof(header);

          buf_iov[1].iov_base = buf;
          buf_iov[1].iov_len  = buf_length;
     }
     else {
          buf_iov[0].iov_base = buf;
          buf_iov[0].iov_len  = buf_length;
     }

     queue_dispatch_receive.receive.ids       = queue_ids;
     queue_dispatch_receive.receive.ids_count = queue_count;

     queue_dispatch_receive.receive.iov       = buf_iov;
     queue_dispatch_receive.receive.iov_count = headerless ? 2 : 1;

     queue_dispatch_receive.receive.timeout_ms = timeout_ms;

     while (ioctl( one_fd, ONE_QUEUE_DISPATCH_RECEIVE, &queue_dispatch_receive )) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    D_DEBUG_AT( One_Queue, "  -> TIMEOUT\n" );
                    ret = DR_TIMEOUT;
                    goto error;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_DISPATCH_RECEIVE failed!\n" );

          goto error;
     }

     D_DEBUG_AT( One_Queue, "  -> received %zu\n", queue_dispatch_receive.receive.ret_received );

     if (headerless)
          *ret_received = queue_dispatch_receive.receive.ret_received - sizeof(header);
     else
          *ret_received = queue_dispatch_receive.receive.ret_received;

error:
     return ret;
}

DirectResult
OneQueue_WakeUp( const OneQID *queue_ids,
                 unsigned int  queue_count )
{
     DirectResult   ret = DR_OK;
     OneQueueWakeUp queue_wakeup;

#if D_DEBUG_ENABLED
     unsigned int   i;

     D_DEBUG_AT( One_Queue, "%s( ids %p, count %u )\n", __FUNCTION__, queue_ids, queue_count );

     for (i=0; i<queue_count; i++)
          D_DEBUG_AT( One_Queue, "  -> QID 0x%08x\n", queue_ids[i] );
#endif

     D_ASSERT( queue_ids != NULL );

     queue_wakeup.ids       = queue_ids;
     queue_wakeup.ids_count = queue_count;

     while (ioctl( one_fd, ONE_QUEUE_WAKEUP, &queue_wakeup )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_QUEUE_WAKEUP failed!\n" );

          break;
     }

     return ret;
}

DirectResult
OneQueue_SetName( OneQID      queue_id,
                  const char *name )
{
     DirectResult ret = DR_OK;
     OneEntryInfo info;

     D_DEBUG_AT( One_Queue, "%s( id %u, name '%s' )\n", __FUNCTION__, queue_id, name );

     D_ASSERT( name != NULL );

     info.type = ONE_QUEUE;
     info.id   = queue_id;

     direct_snputs( info.name, name, ONE_ENTRY_INFO_NAME_LENGTH );
     
     while (ioctl( one_fd, ONE_ENTRY_SET_INFO, &info )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          ret = errno2result( errno );

          D_PERROR( "One/Queue: ONE_ENTRY_SET_INFO failed!\n" );

          break;
     }

     return ret;
}

/*********************************************************************************************************************/

#define RECEIVE_BUFFER_SIZE   65536

/**********************************************************************************************************************/

typedef struct {
     int                 magic;

     OneQID              queue_id;

     OneThreadDispatch   dispatch;
     void               *context;
} AddedQueue;

struct __One_OneThread {
     int                 magic;

     DirectMutex         lock;

     bool                stop;

     DirectThread       *thread;

     DirectHash         *queues;

     OneQID             *queue_ids;
     unsigned int        queue_count;

     unsigned int        queues_age;
};

static void *
OneThread_Dispatcher( DirectThread *thread,
                      void         *arg )
{
     DirectResult  ret;
     OneThread    *data = arg;
     char         *buf;
     OneQID       *ids          = NULL;
     unsigned int  ids_capacity = 1000;
     unsigned int  ids_count    = 0;
     unsigned int  ids_age      = 0;

     D_DEBUG_AT( One_Thread, "%s()\n", __FUNCTION__ );

     buf = D_MALLOC( RECEIVE_BUFFER_SIZE );
     if (!buf) {
          D_OOM();
          return NULL;
     }

     ids = D_MALLOC( sizeof(OneQID) * ids_capacity );
     if (!ids) {
          D_OOM();
          return NULL;
     }

     while (!data->stop) {
          size_t length;
          size_t offset;

          direct_mutex_lock( &thread->lock );

          if (data->queues_age != ids_age) {
               if (!data->queue_count) {
                    // FIXME: use wait queue
                    direct_thread_sleep( 10000 );


                    direct_mutex_unlock( &thread->lock );
                    continue;
               }

               ids_age   = data->queues_age;
               ids_count = data->queue_count;

               if (ids_count > ids_capacity) {
                    while (ids_count > ids_capacity)
                         ids_capacity *= 2;

                    D_FREE( ids );

                    D_ASSERT( data->queue_count > 0 );

                    ids = D_MALLOC( sizeof(OneQID) * ids_capacity );
                    if (!ids) {
                         D_OOM();
                         return NULL;
                    }
               }

               direct_memcpy( ids, data->queue_ids, sizeof(OneQID) * data->queue_count );
          }

          direct_mutex_unlock( &thread->lock );

//#define ONE_QUEUE_WORKAROUND
#ifndef ONE_QUEUE_WORKAROUND
          ret = OneQueue_Receive( ids, ids_count, buf, RECEIVE_BUFFER_SIZE, &length, false, 0 );
#else
          ret = OneQueue_Receive( ids, ids_count, buf, RECEIVE_BUFFER_SIZE, &length, false, 100 );
          if (ret == DR_TIMEOUT )
               continue;
#endif
          if (ret) {
               D_DERROR( ret, "IComaComponent/One: Could not receive from Component Queue!\n" );
               break;
          }

          D_DEBUG_AT( One_Thread, "%s() -> received %zu bytes\n", __FUNCTION__, length );

          for (offset=0; offset < length; ) {
               AddedQueue      *queue;
               OnePacketHeader *header = (OnePacketHeader *)(buf + offset);
               size_t           size   = header->uncompressed;

               D_DEBUG_AT( One_Thread, "  -> size %zu\n", size );

               offset += sizeof(OnePacketHeader) + size;

               if (offset > length) {
                    D_WARN( "invalid packet (offset %zu, length %zu)", offset, length );
                    continue;
               }

               direct_mutex_lock( &thread->lock );

               queue = direct_hash_lookup( data->queues, header->queue_id );

               direct_mutex_unlock( &thread->lock );

               if (queue) {
                    D_MAGIC_ASSERT( queue, AddedQueue );

                    D_DEBUG_AT( One_Thread, "  -> added queue %p, dispatch %p\n", queue, queue->dispatch );

                    queue->dispatch( queue->context, header, header + 1, data );
               }
          }
     }

     D_FREE( buf );

     return NULL;
}

DirectResult
OneThread_Create( const char  *name,
                  OneThread  **ret_thread )
{
     DirectResult  ret;
     OneThread    *thread;

     D_DEBUG_AT( One_Thread, "%s( '%s' )\n", __FUNCTION__, name );

     D_ASSERT( name != NULL );
     D_ASSERT( ret_thread != NULL );

     thread = D_CALLOC( 1, sizeof(OneThread) );
     if (!thread)
          return D_OOM();


     // FIXME: Get rid of id array management when adding more queue connection support (attach)

     thread->queue_ids = D_MALLOC( sizeof(OneQID) );
     if (!thread->queue_ids) {
          ret = D_OOM();
          goto error;
     }

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &thread->queue_ids[0] );
     if (ret)
          goto error;

     thread->queue_count = 1;
     thread->queues_age  = 1;


     ret = direct_hash_create( 23, &thread->queues );
     if (ret)
          goto error_hash_create;

     direct_mutex_init( &thread->lock );

     *ret_thread = thread;

     thread->thread = direct_thread_create( DTT_DEFAULT, OneThread_Dispatcher, thread, name );

     return DR_OK;


error_hash_create:
     OneQueue_Destroy( thread->queue_ids[0] );

error:
     if (thread->queue_ids)
          D_FREE( thread->queue_ids );
          
     D_FREE( thread );

     return ret;
}

void
OneThread_Destroy( OneThread *thread )
{
     D_DEBUG_AT( One_Thread, "%s()\n", __FUNCTION__ );

}

DirectResult
OneThread_AddQueue( OneThread         *thread,
                    OneQID             queue_id,
                    OneThreadDispatch  dispatch,
                    void              *context )
{
     AddedQueue *queue;

     D_DEBUG_AT( One_Thread, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &thread->lock );

     queue = direct_hash_lookup( thread->queues, queue_id );
     if (queue) {
          direct_mutex_unlock( &thread->lock );
          return DR_BUSY;
     }

     queue = D_CALLOC( 1, sizeof(AddedQueue) );
     if (!queue) {
          direct_mutex_unlock( &thread->lock );
          return D_OOM();
     }

     queue->queue_id = queue_id;
     queue->dispatch = dispatch;
     queue->context  = context;

     D_MAGIC_SET( queue, AddedQueue );

     direct_hash_insert( thread->queues, queue_id, queue );

     thread->queue_ids = D_REALLOC( thread->queue_ids, sizeof(OneQID) * (thread->queue_count + 1) );

     thread->queue_ids[thread->queue_count] = queue_id;

     thread->queue_count++;

     thread->queues_age++;

     direct_mutex_unlock( &thread->lock );

     OneQueue_WakeUp( &thread->queue_ids[0], 1 );

     return DR_OK;
}

DirectResult
OneThread_RemoveQueue( OneThread *thread,
                       OneQID     queue_id )
{
     AddedQueue   *queue;
     unsigned int  i;

     D_DEBUG_AT( One_Thread, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &thread->lock );

     queue = direct_hash_lookup( thread->queues, queue_id );
     if (!queue) {
          direct_mutex_unlock( &thread->lock );
          return DR_ITEMNOTFOUND;
     }

     direct_hash_remove( thread->queues, queue_id );

     for (i=0; i<thread->queue_count; i++) {
          if (thread->queue_ids[i] == queue_id)
               break;
     }

     if (i >= thread->queue_count)
          D_BUG( "queue_ids are missing element" );

     for (i++; i<thread->queue_count; i++)
          thread->queue_ids[i-1] = thread->queue_ids[i];

     thread->queue_ids = D_REALLOC( thread->queue_ids, sizeof(OneQID) * thread->queue_count );

     thread->queue_count--;

     thread->queues_age++;

     direct_mutex_unlock( &thread->lock );

     OneQueue_WakeUp( &thread->queue_ids[0], 1 );

     D_MAGIC_CLEAR( queue );

     D_FREE( queue );

     return DR_OK;
}

