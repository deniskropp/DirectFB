/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE_THREAD_H__
#define __ONE_THREAD_H__

#include <direct/types.h>

#include <One/OneTypes.h>


typedef struct __One_OneThread OneThread;


typedef void (*OneThreadDispatch)( void                  *context,
                                   const OnePacketHeader *header,
                                   void                  *data,
                                   OneThread             *thread );


DirectResult OneThread_Create     ( const char           *name,
                                    OneThread           **ret_thread );

void         OneThread_Destroy    ( OneThread            *thread );

DirectResult OneThread_AddQueue   ( OneThread            *thread,
                                    OneQID                queue_id,
                                    OneThreadDispatch     dispatch,
                                    void                 *context );

DirectResult OneThread_RemoveQueue( OneThread            *thread,
                                    OneQID                queue_id );


#endif

