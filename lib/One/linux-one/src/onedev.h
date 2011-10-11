/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__ONEDEV_H__
#define __ONE__ONEDEV_H__

#include <linux/proc_fs.h>

#include "debug.h"
#include "entries.h"
#include "list.h"
#include "types.h"

#define D_ARRAY_SIZE(array)        ((int)(sizeof(array) / sizeof((array)[0])))

#define NUM_MINORS  8
#define NUM_CLASSES 8

struct __One_OneDev {
     OneID         one_ids;

     int           refs;
     int           index;

     DirectLink   *apps;

     struct {
          int major;
          int minor;
     } api;

     struct {
          int queue_attach;
          int queue_detach;
          int queue_dispatch;
     } stat;

     OneEntries   queue;

     unsigned int next_class_index;
};


extern OneDev                 one_devs[NUM_MINORS];
extern OneCore               *one_core;
extern struct proc_dir_entry *one_proc_dir[NUM_MINORS];

#endif
