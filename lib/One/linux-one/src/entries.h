/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__ENTRIES_H__
#define __ONE__ENTRIES_H__

#include <linux/mutex.h>
#include <linux/seq_file.h>

#include "types.h"
#include "list.h"

#include "onecore.h"


typedef struct __FD_OneEntry OneEntry;

typedef const struct {
     int object_size;

     int (*Init)     (OneEntry * entry, void *ctx, void *create_ctx);
     void (*Destroy) (OneEntry * entry, void *ctx);
     void (*Print)   (OneEntry * entry, void *ctx, struct seq_file * p);
} OneEntryClass;

typedef struct {
     unsigned int class_index;
     void *ctx;

     DirectLink *list;

     OneDev *dev;

     struct timeval now; /* temporary for /proc code (seq start/show) */
} OneEntries;

struct __FD_OneEntry {
     DirectLink link;

     OneEntries *entries;

     u32 id;
     pid_t pid;

     OneWaitQueue wait;
     int waiters;

     struct timeval last_lock;

     char name[ONE_ENTRY_INFO_NAME_LENGTH];
};

/* Entries Init & DeInit */

void one_entries_init( OneEntries    *entries,
                       OneEntryClass *class,
                       void          *ctx,
                       OneDev        *dev );

void one_entries_deinit(OneEntries * entries);

/* '/proc' support */

void one_entries_create_proc_entry(OneDev * dev, const char *name,
                                      OneEntries * data);

void one_entries_destroy_proc_entry(OneDev * dev, const char *name);

/* Create & Destroy */

int one_entry_create(OneEntries * entries, u32 *ret_id, void *create_ctx);

int one_entry_destroy(OneEntries * entries, u32 id);

void one_entry_destroy_locked(OneEntries * entries, OneEntry * entry);

/* Information */

int one_entry_set_info(OneEntries * entries,
                          const OneEntryInfo * info);

int one_entry_get_info(OneEntries * entries, OneEntryInfo * info);

/* Lookup */

int one_entry_lookup(OneEntries * entries, u32 id, OneEntry ** ret_entry);

/** Wait & Notify **/

/*
 * Wait for the entry to be notified with an optional timeout.
 *
 * The entry
 *   (1) has to be locked prior to calling this function.
 *   (2) is temporarily unlocked while being waited for.
 *
 * If this function returns an error, the entry is not locked again!
 *
 * Possible errors are:
 *   -EIDRM      Entry has been removed while being waited for.
 *   -ETIMEDOUT  Timeout occured.
 *   -EINTR      A signal has been received.
 */
int one_entry_wait(OneEntry * entry, int *timeout);

/*
 * Wake up one or all processes waiting for the entry to be notified.
 *
 * The entry has to be locked prior to calling this function.
 */
void one_entry_notify(OneEntry * entry);

#define ONE_ENTRY_CLASS( Type, name, init_func, destroy_func, print_func )   \
                                                                                \
     static OneEntryClass name##_class = {                                   \
          .object_size = sizeof(Type),                                          \
          .Init        = init_func,                                             \
          .Destroy     = destroy_func,                                          \
          .Print       = print_func                                             \
     };                                                                         \
                                                                                \
     static inline int one_##name##_lookup( OneEntries  *entries,         \
                                               int             id,              \
                                               Type          **ret_##name )     \
     {                                                                          \
          int          ret;                                                     \
          OneEntry *entry;                                                   \
                                                                                \
          ret = one_entry_lookup( entries, id, &entry );                     \
                                                                                \
          if (!ret)                                                             \
               *ret_##name = (Type *) entry;                                    \
                                                                                \
          return ret;                                                           \
     }                                                                          \
                                                                                \
     static inline int one_##name##_wait( Type *name, int *timeout )         \
     {                                                                          \
          return one_entry_wait( (OneEntry*) name, timeout );             \
     }                                                                          \
                                                                                \
     static inline void one_##name##_notify( Type *name )                    \
     {                                                                          \
          one_entry_notify( (OneEntry*) name );                           \
     }

#endif
