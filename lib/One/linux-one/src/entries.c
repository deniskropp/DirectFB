/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Abstract entry with ID etc.
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
#include <linux/time.h>
#include <linux/version.h>

#include <linux/one.h>

#include "onedev.h"
#include "entries.h"


static OneEntryClass *entry_classes[NUM_MINORS][NUM_CLASSES];

void
one_entries_init( OneEntries    *entries,
                     OneEntryClass *class,
                     void             *ctx,
                     OneDev        *dev )
{

     ONE_DEBUG( "%s( entries %p, class %p, ctx %p, dev %p )\n",
                   __FUNCTION__, entries, class, ctx, dev );

     ONE_DEBUG( "  -> object_size %d, entry size %zu\n", class->object_size, sizeof(OneEntry) );

     ONE_ASSERT(entries != NULL);
     ONE_ASSERT(class != NULL);
     ONE_ASSERT(class->object_size >= sizeof(OneEntry));

     if (!dev->refs) {
          memset(entries, 0, sizeof(OneEntries));

          entries->class_index = dev->next_class_index++;
          entries->ctx = ctx;
          entries->dev = dev;
     }

     entry_classes[dev->index][entries->class_index] = class;
}

void one_entries_deinit(OneEntries * entries)
{
     ONE_ASSERT(entries != NULL);

     if (!entries->dev->refs) {
          DirectLink *tmp;
          OneEntry *entry;
          OneEntryClass *class;

          class = entry_classes[entries->dev->index][entries->class_index];

          direct_list_foreach_safe(entry, tmp, entries->list) {
               if (class->Destroy)
                    class->Destroy(entry, entries->ctx);

               direct_list_remove( &entries->list, &entry->link );

               one_core_free( one_core, entry);
          }
     }
}

/* reading PROC entries */

static void *one_entries_seq_start(struct seq_file *f, loff_t * pos)
{
     int i = *pos;

     OneEntry *entry;
     OneEntries *entries;
     OneEntryClass *class;

     entries = f->private;

     one_core_lock( one_core );

     entry = (void *)(entries->list);
     while (i && entry) {
          entry = (void *)(entry->link.next);
          i--;
     }

     ONE_ASSERT(entries != NULL);

     class = entry_classes[entries->dev->index][entries->class_index];
     if (!class->Print)
          return NULL;

     do_gettimeofday(&entries->now);

     return entry;
}

static void *one_entries_seq_next(struct seq_file *f, void *v, loff_t * pos)
{
     OneEntry *entry = v;

     (*pos)++;
     return entry->link.next;
}

static void one_entries_seq_stop(struct seq_file *f, void *v)
{
     OneEntries *entries;

     entries = f->private;
     (void)v;

     one_core_unlock( one_core );
}

int one_entries_show(struct seq_file *p, void *v)
{
     OneEntry *entry;
     OneEntries *entries;
     OneEntryClass *class;

     entries = p->private;

     entry = v;

     class = entry_classes[entries->dev->index][entries->class_index];

     if (entry->last_lock.tv_sec) {
          int diff = ((entry->entries->now.tv_sec - entry->last_lock.tv_sec) * 1000 +
                      (entry->entries->now.tv_usec - entry->last_lock.tv_usec) / 1000);

          if (diff < 1000) {
               seq_printf(p, "%3d  ms  ", diff);
          } else if (diff < 1000000) {
               seq_printf(p, "%3d.%d s  ", diff / 1000,
                          (diff % 1000) / 100);
          } else {
               diff = (entry->entries->now.tv_sec - entry->last_lock.tv_sec +
                       (entry->entries->now.tv_usec -
                        entry->last_lock.tv_usec) / 1000000);

               seq_printf(p, "%3d.%d h  ", diff / 3600,
                          (diff % 3600) / 360);
          }
     } else
          seq_printf(p, "  -.-    ");

     seq_printf(p, "(%5d) 0x%08x  ", entry->pid, entry->id);
     seq_printf(p, "%-24s  ", entry->name[0] ? entry->name : "");

     class->Print(entry, entry->entries->ctx, p);

     return 0;
}

static const struct seq_operations one_entries_seq_ops = {
     .start = one_entries_seq_start,
     .next  = one_entries_seq_next,
     .stop  = one_entries_seq_stop,
     .show  = one_entries_show
};

static int one_entries_open(struct inode *inode, struct file *file)
{
     struct seq_file *sf;
     int ret;

     ret = seq_open(file, &one_entries_seq_ops);
     if (ret < 0)
          return ret;

     sf = file->private_data;
     sf->private = PDE(inode)->data;

     return 0;
}

static const struct file_operations proc_one_entries_operations = {
     .open    = one_entries_open,
     .read    = seq_read,
     .llseek  = seq_lseek,
     .release = seq_release,
};

void one_entries_create_proc_entry(OneDev * dev, const char *name,
                                      OneEntries * data)
{
     struct proc_dir_entry *pde;

     pde = create_proc_entry(name, 0, one_proc_dir[dev->index]);
     if (pde) {
          pde->proc_fops = &proc_one_entries_operations;
          pde->data = data;
     }
}

void one_entries_destroy_proc_entry(OneDev * dev, const char *name)
{
     remove_proc_entry(name, one_proc_dir[dev->index]);
}

int one_entry_create(OneEntries * entries, u32 *ret_id, void *create_ctx)
{
     int ret;
     OneEntry *entry;
     OneEntryClass *class;

     ONE_ASSERT(entries != NULL);
     ONE_ASSERT(ret_id != NULL);

     class = entry_classes[entries->dev->index][entries->class_index];

     entry = one_core_malloc( one_core, class->object_size );
     if (!entry)
          return -ENOMEM;

     memset(entry, 0, class->object_size);

     entry->entries = entries;
     entry->id = one_core_new_id( one_core );
     entry->pid = one_core_pid( one_core );

     one_core_wq_init( one_core, &entry->wait);

     if (class->Init) {
          ret = class->Init(entry, entries->ctx, create_ctx);
          if (ret) {
               one_core_free( one_core, entry);
               return ret;
          }
     }

     direct_list_prepend(&entries->list, &entry->link);

     *ret_id = entry->id;

     return 0;
}

int one_entry_destroy(OneEntries * entries, u32 id)
{
     OneEntry *entry;
     OneEntryClass *class;

     ONE_ASSERT(entries != NULL);

     class = entry_classes[entries->dev->index][entries->class_index];

     /* Lookup the entry. */
     direct_list_foreach(entry, entries->list) {
          if (entry->id == id)
               break;
     }

     /* Check if no entry was found. */
     if (!entry) {
          return -EINVAL;
     }

     /* Destroy it now. */
     one_entry_destroy_locked(entries, entry);

     return 0;
}

void one_entry_destroy_locked(OneEntries * entries, OneEntry * entry)
{
     OneEntryClass *class;

     ONE_ASSERT(entries != NULL);

     class = entry_classes[entries->dev->index][entries->class_index];

     /* Remove the entry from the list. */
     direct_list_remove(&entries->list, &entry->link);

     /* Wake up any waiting process. */
     one_core_wq_wake( one_core, &entry->wait);

     /* Call the destroy function. */
     if (class->Destroy)
          class->Destroy(entry, entries->ctx);

          /* Deallocate the entry. */
          one_core_free( one_core, entry);
}

int one_entry_set_info(OneEntries * entries, const OneEntryInfo * info)
{
     int ret;
     OneEntry *entry;

     ONE_ASSERT(entries != NULL);
     ONE_ASSERT(info != NULL);

     ret = one_entry_lookup(entries, info->id, &entry);
     if (ret)
          return ret;

     snprintf(entry->name, ONE_ENTRY_INFO_NAME_LENGTH, info->name);

     return 0;
}

int one_entry_get_info(OneEntries * entries, OneEntryInfo * info)
{
     int ret;
     OneEntry *entry;

     ONE_ASSERT(entries != NULL);
     ONE_ASSERT(info != NULL);

     ret = one_entry_lookup(entries, info->id, &entry);
     if (ret)
          return ret;

     snprintf(info->name, ONE_ENTRY_INFO_NAME_LENGTH, entry->name);

     return 0;
}


int
one_entry_lookup(OneEntries * entries,
                    u32 id, OneEntry ** ret_entry)
{
     OneEntry *entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
     struct timespec xtime;
#endif

     ONE_ASSERT(entries != NULL);
     ONE_ASSERT(ret_entry != NULL);

     /* Lookup the entry. */
     direct_list_foreach(entry, entries->list) {
          if (entry->id == id)
               break;
     }

     /* Check if no entry was found. */
     if (!entry) {
          return -EINVAL;
     }

     /* Move the entry to the front of all entries. */
     direct_list_move_to_front(&entries->list, &entry->link);

     /* Keep timestamp, but use the slightly
        inexact version to avoid performance impacts. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && defined _STRUCT_TIMESPEC
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
     xtime = current_kernel_time();
#endif
     entry->last_lock.tv_sec = xtime.tv_sec;
     entry->last_lock.tv_usec = xtime.tv_nsec / 1000;
#else
     entry->last_lock = xtime;
#endif

     /* Return the locked entry. */
     *ret_entry = entry;

     return 0;
}

int one_entry_wait(OneEntry * entry, int *timeout)
{
     int ret;
     int id;
     OneEntries *entries;
     OneEntry *entry2;

     ONE_ASSERT(entry != NULL);
     ONE_ASSERT(entry->entries != NULL);

     id = entry->id;
     entries = entry->entries;

     entry->waiters++;

     one_core_wq_wait( one_core, &entry->wait, timeout );

     entry->waiters--;

     if (signal_pending(current))
          return -EINTR;

     if (timeout && !*timeout)
          return -ETIMEDOUT;

     ret = one_entry_lookup(entries, id, &entry2);
     switch (ret) {
          case -EINVAL:
               return -EIDRM;

          case 0:
               if (entry != entry2)
                    BUG();
     }

     return ret;
}

void one_entry_notify(OneEntry * entry)
{
     ONE_ASSERT(entry != NULL);

     one_core_wq_wake( one_core, &entry->wait);
}

