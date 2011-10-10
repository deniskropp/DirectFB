/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__TARGET_H__
#define __ONE__TARGET_H__

#include "onedev.h"
#include "types.h"



typedef void (*OneTarget_DestroyFunc) ( OneTarget          *target );

typedef int  (*OneTarget_DispatchFunc)( OneTarget          *target,
                                        OnePacketHeader    *header,
                                        const struct iovec *iov,
                                        size_t              iov_count );

typedef int  (*OneTarget_WakeUpFunc)  ( OneTarget          *target );

typedef int  (*OneTarget_InitFunc)    ( OneTarget          *target,
                                        void               *ctx );


struct __One_OneTarget {
     int                      magic;

     void                    *data;

     OneTarget_DestroyFunc    Destroy;
     OneTarget_DispatchFunc   Dispatch;
     OneTarget_WakeUpFunc     WakeUp;
};


int  OneTarget_New    ( OneTarget_InitFunc   Init,
                        void                *ctx,
                        size_t               data_size,
                        OneTarget          **ret_target );

void OneTarget_Destroy( OneTarget           *target );


#endif

