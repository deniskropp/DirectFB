/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__ONECORE_H__
#define __ONE__ONECORE_H__

#include <linux/stddef.h>

#include <linux/one.h>

#include "onecore_impl.h"
#include "app.h"


typedef struct __One_OneCore OneCore;


int               one_core_enter      ( OneCore        **ret_core );

void              one_core_cleanup    ( OneCore         *core );

void              one_core_exit       ( OneCore         *core );

pid_t             one_core_pid        ( OneCore         *core );


/*
 * Returns a new ID
 */
u32               one_core_new_id     ( OneCore         *core );

bool              one_core_is_local   ( OneCore         *core,
                                        u32              id );

int               one_core_dispatch   ( OneCore               *core,
                                        OneApp                *app,
                                        const OnePacketHeader *header,
                                        const struct iovec    *iov,
                                        size_t                 iov_count );

int               one_core_attach     ( OneCore          *core,
                                        OneApp           *app,
                                        OneQueueAttach   *queue_attach );

int               one_core_detach     ( OneCore          *core,
                                        OneApp           *app,
                                        OneQueueDetach   *queue_detach );


void             *one_core_malloc     ( OneCore         *core,
                                        size_t           size );

void              one_core_free       ( OneCore         *core,
                                        void            *ptr );


void              one_core_lock       ( OneCore         *core );
void              one_core_unlock     ( OneCore         *core );


int               one_core_wq_init    ( OneCore         *core,
                                        OneWaitQueue    *queue );

void              one_core_wq_deinit  ( OneCore         *core,
                                        OneWaitQueue    *queue );

void              one_core_wq_wait    ( OneCore         *core,
                                        OneWaitQueue    *queue,
                                        int             *timeout_ms );

void              one_core_wq_wake    ( OneCore         *core,
                                        OneWaitQueue    *queue );


#endif
