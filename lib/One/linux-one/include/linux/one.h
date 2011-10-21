/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __LINUX__ONE_H__
#define __LINUX__ONE_H__

#include <asm/ioctl.h>

#ifndef __KERNEL__
#include <stdint.h> 
#endif

#include <OneTypes.h>

/* One supports all API versions up to this version */
#define ONE_API_MAJOR_PROVIDED 1
#define ONE_API_MINOR_PROVIDED 0
#define ONE_API_MICRO_PROVIDED 0

/*
 * One Kernel Device API Version
 * Default behaviour: 1.0.0
 */
#ifndef ONE_API_MAJOR
#undef ONE_API_MAJOR
#undef ONE_API_MINOR
#undef ONE_API_MICRO
#define ONE_API_MAJOR      1
#define ONE_API_MINOR      0
#define ONE_API_MICRO      0
#endif


#define ONE_MAX_PACKET_SIZE   16384     // FIXME: move to kernel module and report to user space via ENTER ioctl


/*
 * The One ID is a unique identifier for one process.
 */
typedef uint32_t OneID;


/*
 * Entering a world
 * API version (major/minor): master determines API of this world. slave has to follow.
 * supported are API 3.x for DirectFB 1.0.x, API 4.x for DirectFB 1.1.x, and API 8.x for DirectFB 1.2.x and beyond.
 */
typedef struct {
     struct {
          int major;
          int minor;
     } api;

     OneID     one_id;
} OneEnter;



/*
 * Create a queue
 */
typedef struct {
     OneQueueFlags            flags;         /* Queue Flags */

     OneQID                   queue_id;      /* New Queue ID, if zero the new ID will be allocated, otherwise it fails if the ID is already used */
} OneQueueNew;


/*
 * Destroy a queue
 */
typedef struct {
     OneQID                   queue_id;           /* Queue ID */
} OneQueueDestroy;


/*
 * Attach to a virtual queue
 */
typedef struct {
     OneQID                   queue_id;      /* (Virtual) Queue ID, the one to attach to */

     OneQID                   target_id;     /* Target Queue ID, the one being attached */
} OneQueueAttach;

/*
 * Detach from a virtual queue
 */
typedef struct {
     OneQID                   queue_id;      /* (Virtual) Queue ID, the one to detach from */

     OneQID                   target_id;     /* Target Queue ID, the one being detached */
} OneQueueDetach;


/*
 * Dispatch a packet
 */
typedef struct {
     OnePacketHeader          header;        /* Packet Header */

     const struct iovec      *iov;           /* iovec as for writev() */
     size_t                   iov_count;     /* iovec count as for writev() */
} OneQueueDispatch;


/*
 * Receive packets
 */
typedef struct {
     const OneQID            *ids;           /* Queue IDs for which to receive */
     size_t                   ids_count;     /* Number of IDs */

     const struct iovec      *iov;           /* iovec as for readv() */
     size_t                   iov_count;     /* iovec count as for readv() */

     int                      timeout_ms;    /* Optional timeout, 0 = no timeout, -1 = return immediately if nothing to receive */

     size_t                   ret_received;  /* Number of bytes received in total (headers and data) */
} OneQueueReceive;


/*
 * Dispatch and receive packets
 */
typedef struct {
     OneQueueDispatch        *dispatch;           /* Dispatch commands (like multiple OneQueueDispatch) */
     size_t                   dispatch_count;     /* Number of Dispatch (in case of error changed to number of successful dispatch) */

     OneQueueReceive          receive;            /* Receive command (like OneQueueReceive after dispatch is done) */
} OneQueueDispatchReceive;


/*
 * Wake up receivers of queues (threads of calling process)
 */
typedef struct {
     const OneQID            *ids;           /* Queue IDs for which to wake up any receivers */
     size_t                   ids_count;     /* Number of IDs */
} OneQueueWakeUp;



/*
 * One types
 */
typedef enum {
     ONE_LOUNGE,
     ONE_QUEUE,
} OneType;

/*
 * Set attributes like 'name' for an entry of the specified type.
 */
#define ONE_ENTRY_INFO_NAME_LENGTH   24

typedef struct {
     OneType                  type;
     uint32_t                 id;

     char                     name[ONE_ENTRY_INFO_NAME_LENGTH];
} OneEntryInfo;


#define ONE_ENTER                         _IOR (ONE_LOUNGE,    0x00, OneEnter)
#define ONE_SYNC                          _IO  (ONE_LOUNGE,    0x01)

#define ONE_ENTRY_SET_INFO                _IOW (ONE_LOUNGE,    0x02, OneEntryInfo)
#define ONE_ENTRY_GET_INFO                _IOW (ONE_LOUNGE,    0x03, OneEntryInfo)


#define ONE_QUEUE_NEW                     _IOWR(ONE_QUEUE,     0x00, OneQueueNew)
#define ONE_QUEUE_DESTROY                 _IOW (ONE_QUEUE,     0x01, OneQueueDestroy)
#define ONE_QUEUE_ATTACH                  _IOW (ONE_QUEUE,     0x02, OneQueueAttach)
#define ONE_QUEUE_DETACH                  _IOW (ONE_QUEUE,     0x03, OneQueueDetach)
#define ONE_QUEUE_DISPATCH                _IOW (ONE_QUEUE,     0x04, OneQueueDispatch)
#define ONE_QUEUE_RECEIVE                 _IOWR(ONE_QUEUE,     0x05, OneQueueReceive)
#define ONE_QUEUE_DISPATCH_RECEIVE        _IOWR(ONE_QUEUE,     0x06, OneQueueDispatchReceive)
#define ONE_QUEUE_WAKEUP                  _IOW (ONE_QUEUE,     0x07, OneQueueWakeUp)

#endif

