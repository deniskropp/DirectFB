/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE_QUEUE_H__
#define __ONE_QUEUE_H__

#include <direct/types.h>

#include <One/OneTypes.h>


/*
 * Create a new Queue
 *
 * If queue_id is ONE_QID_NONE, it will be generated.
 *
 * Otherwise, it will fail if the ID is already in use.
 */
DirectResult OneQueue_New      ( OneQueueFlags   flags,
                                 OneQID          queue_id,
                                 OneQID         *ret_id );

DirectResult OneQueue_Destroy  ( OneQID          queue_id );

/*********************************************************************************************************************/

DirectResult OneQueue_Attach   ( OneQID          queue_id,
                                 OneQID          target_id );

DirectResult OneQueue_Detach   ( OneQID          queue_id,
                                 OneQID          target_id );

/*********************************************************************************************************************/

DirectResult OneQueue_Dispatch ( OneQID          queue_id,
                                 void           *data,
                                 size_t          length );

DirectResult OneQueue_DispatchV( OneQID          queue_id,
                                 void          **data,
                                 size_t         *length,
                                 size_t          count );

/*********************************************************************************************************************/

DirectResult OneQueue_Receive  ( const OneQID   *queue_ids,
                                 unsigned int    queue_count,
                                 void           *buf,
                                 size_t          length,
                                 size_t         *ret_received,
                                 bool            headerless );

DirectResult OneQueue_ReceiveV ( const OneQID   *queue_ids,
                                 unsigned int    queue_count,
                                 void          **buf,
                                 size_t         *length,
                                 size_t          count,
                                 size_t         *ret_received,
                                 bool            headerless );

/*********************************************************************************************************************/

DirectResult OneQueue_DispatchReceive( OneQID          queue_id,
                                       void           *data,
                                       size_t          data_length,
                                       const OneQID   *queue_ids,
                                       unsigned int    queue_count,
                                       void           *buf,
                                       size_t          buf_length,
                                       size_t         *ret_received,
                                       bool            headerless );

/*********************************************************************************************************************/

DirectResult OneQueue_WakeUp   ( const OneQID   *queue_ids,
                                 unsigned int    queue_count );


#endif

