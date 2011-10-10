/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Target is an abstracted storage for packets sent via a Queue
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
#include <linux/socket.h>
#include <linux/proc_fs.h>

#include <linux/one.h>

#include "debug.h"
#include "onedev.h"
#include "target.h"


/******************************************************************************/

int
OneTarget_New( OneTarget_InitFunc   Init,
               void                *ctx,
               size_t               data_size,
               OneTarget          **ret_target )
{
     int        ret;
     OneTarget *target;

     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     target = one_core_malloc( one_core, sizeof(OneTarget) + data_size );
     if (!target)
          return -ENOMEM;

     memset( target, 0, sizeof(OneTarget) + data_size );

     target->data = target + 1;

     ret = Init( target, ctx );
     if (ret) {
          one_core_free( one_core, target );
          return ret;
     }

     D_MAGIC_SET( target, OneTarget );

     *ret_target = target;

     return 0;
}

void
OneTarget_Destroy( OneTarget *target )
{
     ONE_DEBUG( "%s( %p )\n", __FUNCTION__, target );

     D_MAGIC_ASSERT( target, OneTarget );


     D_MAGIC_CLEAR( target );

     one_core_free( one_core, target );
}

