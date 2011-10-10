/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__TYPES_H__
#define __ONE__TYPES_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
typedef enum {
     false = 0,
     true = !false
} bool;
#endif

typedef struct __One_OneApp    OneApp;
typedef struct __One_OneDev    OneDev;
typedef struct __One_OnePacket OnePacket;
typedef struct __One_OneQueue  OneQueue;
typedef struct __One_OneTarget OneTarget;
typedef struct __One_OneUDP    OneUDP;

#endif
