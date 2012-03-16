/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Device is the entry from user space into One IPC implementation in kernel
   
   Each new open() from a process results in creation of a One Application
*/

//#define ONE_ENABLE_DEBUG

#include <linux/version.h>
#include <linux/module.h>
#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <linux/devfs_fs_kernel.h>
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#include <linux/page-flags.h>
#include <linux/mm.h>
#endif
#include <asm/pgtable.h>
#include <linux/mm.h>

#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
#include <linux/device.h>
#endif

#include <linux/one.h>

#include "app.h"
#include "onedev.h"
#include "queue.h"

#ifndef ONE_MAJOR
#define ONE_MAJOR 250
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Denis Oliver Kropp <dok@directfb.org>");

struct proc_dir_entry *proc_one_dir;

static int one_major = ONE_MAJOR;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static devfs_handle_t devfs_handles[NUM_MINORS];
static inline unsigned iminor(struct inode *inode)
{
     return MINOR(inode->i_rdev);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static struct class *one_class;
#else
static struct class_simple *one_class;
#endif
#endif



OneDev                 one_devs[NUM_MINORS];

OneCore               *one_core;
struct proc_dir_entry *one_proc_dir[NUM_MINORS]   = { 0 };
unsigned int           one_local_refs[NUM_MINORS] = { 0 };

/******************************************************************************/

static int
onedev_apps_read_proc(char *buf, char **start, off_t offset,
                      int len, int *eof, void *private)
{
     OneDev *dev = private;
     int     written = 0;
     OneApp *app;

     written += snprintf(buf + written, offset + len - written,
                         "  pid   OneID       status         queues\n");
     if (written < offset) {
          offset -= written;
          written = 0;
     }

     direct_list_foreach (app, dev->apps) {
          OneAppTargetData *recv_data;

          if (written >= len)
               break;

          written += snprintf(buf + written, offset + len - written,
                              "(%5d) 0x%08x  %s  ", current->pid,
                              app->one_id, app->recv_data ? "RECEIVING" : "not receiving");
          if (written < offset) {
               offset -= written;
               written = 0;
          }

          direct_list_foreach (recv_data, app->recv_data) {
               written += snprintf(buf + written, offset + len - written,
                                   " 0x%08x", recv_data->queue_id);
               if (written < offset) {
                    offset -= written;
                    written = 0;
               }
          }

          written += snprintf(buf + written, offset + len - written, "\n");
          if (written < offset) {
               offset -= written;
               written = 0;
          }
     }

     *start = buf + offset;
     written -= offset;
     if (written > len) {
          *eof = 0;
          return len;
     }

     *eof = 1;
     return(written < 0) ? 0 : written;
}

static int
onedev_stat_read_proc(char *buf, char **start, off_t offset,
                      int len, int *eof, void *private)
{
     OneDev *dev = private;
     int written = 0;

     if ((dev->api.major != 0) || (dev->api.minor != 0))
          written +=
          sprintf(buf, "One API:%d.%d\n", dev->api.major,
                  dev->api.minor);

     written += snprintf(buf + written, offset + len - written,
                         "    attach     detach   dispatch\n");
     if (written < offset) {
          offset -= written;
          written = 0;
     }

     if (written < len) {
          written += snprintf(buf + written, offset + len - written,
                              "%10d %10d %10d\n",
                              dev->stat.queue_attach,
                              dev->stat.queue_detach,
                              dev->stat.queue_dispatch);
          if (written < offset) {
               offset -= written;
               written = 0;
          }
     }

     *start = buf + offset;
     written -= offset;
     if (written > len) {
          *eof = 0;
          return len;
     }

     *eof = 1;
     return(written < 0) ? 0 : written;
}

/******************************************************************************/

static int onedev_init(OneDev * dev)
{
     int ret;

     ret = one_queue_init(dev);
     if (ret)
          goto error_queue;

     create_proc_read_entry("apps", 0, one_proc_dir[dev->index],
                            onedev_apps_read_proc, dev);

     create_proc_read_entry("stat", 0, one_proc_dir[dev->index],
                            onedev_stat_read_proc, dev);

     return 0;

error_queue:
     return ret;
}

static void onedev_deinit(OneDev * dev)
{
     remove_proc_entry("apps", one_proc_dir[dev->index]);
     remove_proc_entry("stat", one_proc_dir[dev->index]);

     one_queue_deinit(dev);

     one_core_cleanup( one_core );
}

/******************************************************************************/

static int one_open(struct inode *inode, struct file *file)
{
     int     ret;
     OneApp *oneapp;
     int     minor = iminor(inode);
     OneDev *dev   = &one_devs[minor];

     ONE_DEBUG("one_open( file %p, f_count %ld ) <- minor %d\n", file, atomic_long_read(&file->f_count), minor);

     one_core_lock( one_core );

     ONE_DEBUG("  -> refs: %d\n", dev->refs);

     if (!dev->refs) {
          memset( dev, 0, sizeof(OneDev) );

          dev->index = minor;
     }
     else {
          if (file->f_flags & O_EXCL) {
               if (dev->one_ids) {
                    one_core_unlock( one_core );
                    return -EBUSY;
               }
          }
     }

     if (!one_local_refs[dev->index]) {
          char buf[4];

          snprintf(buf, 4, "%d", minor);

          one_proc_dir[minor] = proc_mkdir(buf, proc_one_dir);

          ret = onedev_init( dev );
          if (ret) {
               remove_proc_entry(buf, proc_one_dir);
               one_core_unlock( one_core );
               return ret;
          }
     }

     ret = OneApp_New( dev, dev->one_ids + 1, &oneapp );
     if (ret) {
          if (!one_local_refs[dev->index]) {
               onedev_deinit( dev );

               remove_proc_entry( one_proc_dir[minor]->name, proc_one_dir );
          }

          one_core_unlock( one_core );

          return ret;
     }

     direct_list_append( &dev->apps, &oneapp->link );

     dev->one_ids++;

     one_local_refs[dev->index]++;

     dev->refs++;

     one_core_unlock( one_core );


     file->private_data = oneapp;

     return 0;
}

static int one_release(struct inode *inode, struct file *file)
{
     int     minor  = iminor(inode);
     OneApp *oneapp = file->private_data;
     OneDev *dev    = oneapp->dev;

     ONE_DEBUG( "one_release( %p, f_count %ld )\n", file, atomic_long_read(&file->f_count) );

     one_core_lock( one_core );

     direct_list_remove( &dev->apps, &oneapp->link );

     OneApp_Destroy( oneapp );

     dev->refs--;

     one_local_refs[dev->index]--;

     if (!one_local_refs[dev->index]) {
          onedev_deinit( dev );

          remove_proc_entry( one_proc_dir[minor]->name, proc_one_dir );
     }

     one_core_unlock( one_core );

     return 0;
}

static int
lounge_ioctl(OneDev * dev, OneApp * oneapp,
             unsigned int cmd, unsigned long arg)
{
     int          ret;
//     OneEnter     enter;
     OneEntryInfo info;

     switch (_IOC_NR(cmd)) {
#if 0
          case _IOC_NR(ONE_ENTER):
               if (copy_from_user(&enter, (OneEnter *) arg, sizeof(enter)))
                    return -EFAULT;

               ret = oneapp_enter(dev, &enter, oneapp);
               if (ret)
                    return ret;

               if (copy_to_user((OneEnter *) arg, &enter, sizeof(enter)))
                    return -EFAULT;

               return 0;

          case _IOC_NR(ONE_SYNC):
               return oneapp_sync( dev, oneapp );
#endif

          case _IOC_NR(ONE_ENTRY_SET_INFO):
               if (copy_from_user
                   (&info, (OneEntryInfo *) arg, sizeof(info)))
                    return -EFAULT;

               switch (info.type) {
                    case ONE_QUEUE:
                         return one_entry_set_info(&dev->queue, &info);

                    default:
                         return -ENOSYS;
               }

          case _IOC_NR(ONE_ENTRY_GET_INFO):
               if (copy_from_user
                   (&info, (OneEntryInfo *) arg, sizeof(info)))
                    return -EFAULT;

               switch (info.type) {
                    case ONE_QUEUE:
                         ret = one_entry_get_info(&dev->queue, &info);
                         break;

                    default:
                         return -ENOSYS;
               }

               if (ret)
                    return ret;

               if (copy_to_user((OneEntryInfo *) arg, &info, sizeof(info)))
                    return -EFAULT;

               return 0;
     }

     return -ENOSYS;
}

static int
queue_ioctl( OneDev        *dev,
             OneApp        *oneapp,
             unsigned int   cmd,
             unsigned long  arg )
{
     int                     ret;
     OneQueueNew             new_;
     OneQueueDestroy         destroy;
     OneQueueAttach          attach;
     OneQueueDetach          detach;
     OneQueueDispatch        dispatch;
     OneQueueReceive         receive;
     OneQueueDispatchReceive dispatch_receive;
     OneQueueWakeUp          wakeup;

     switch (_IOC_NR(cmd)) {
          case _IOC_NR(ONE_QUEUE_NEW):
               if (copy_from_user(&new_,
                                  (OneQueueNew *) arg,
                                  sizeof(new_)))
                    return -EFAULT;

               ret = one_queue_new( oneapp, &new_ );
               if (ret)
                    return ret;

               if (copy_to_user((OneQueueNew *) arg,
                                &new_,
                                sizeof(new_)))
                    return -EFAULT;
               return 0;

          case _IOC_NR(ONE_QUEUE_DESTROY):
               if (copy_from_user(&destroy,
                                  (OneQueueDestroy *) arg,
                                  sizeof(destroy)))
                    return -EFAULT;

               return one_queue_destroy( oneapp, &destroy );

          case _IOC_NR(ONE_QUEUE_ATTACH):
               if (copy_from_user(&attach,
                                  (OneQueueAttach *) arg,
                                  sizeof(attach)))
                    return -EFAULT;

               return one_queue_attach( oneapp, &attach );

          case _IOC_NR(ONE_QUEUE_DETACH):
               if (copy_from_user(&detach,
                                  (OneQueueDetach *) arg,
                                  sizeof(detach)))
                    return -EFAULT;

               return one_queue_detach( oneapp, &detach );

          case _IOC_NR(ONE_QUEUE_DISPATCH):
               if (copy_from_user(&dispatch,
                                  (OneQueueDispatch *) arg,
                                  sizeof(dispatch)))
                    return -EFAULT;

               return one_queue_dispatch( oneapp, &dispatch );

          case _IOC_NR(ONE_QUEUE_RECEIVE):
               if (copy_from_user(&receive,
                                  (OneQueueReceive *) arg,
                                  sizeof(receive)))
                    return -EFAULT;

               ret = one_queue_receive( oneapp, &receive );

               if (copy_to_user((OneQueueReceive *) arg,
                                &receive,
                                sizeof(receive)))
                    return -EFAULT;
               return ret;

          case _IOC_NR(ONE_QUEUE_DISPATCH_RECEIVE):
               if (copy_from_user(&dispatch_receive,
                                  (OneQueueDispatchReceive *) arg,
                                  sizeof(dispatch_receive)))
                    return -EFAULT;

               ret = one_queue_dispatch_receive( oneapp, &dispatch_receive );

               if (copy_to_user((OneQueueDispatchReceive *) arg,
                                &dispatch_receive,
                                sizeof(dispatch_receive)))
                    return -EFAULT;
               return ret;

          case _IOC_NR(ONE_QUEUE_WAKEUP):
               if (copy_from_user(&wakeup,
                                  (OneQueueWakeUp *) arg,
                                  sizeof(wakeup)))
                    return -EFAULT;

               return one_queue_wakeup( oneapp, &wakeup );
     }

     return -ENOSYS;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long
one_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int
one_ioctl(struct inode *inode, struct file *file,
             unsigned int cmd, unsigned long arg)
#endif
{
     int     ret    = -ENOSYS;
     OneApp *oneapp = file->private_data;
     OneDev *dev    = oneapp->dev;

//     ONE_DEBUG("one_ioctl (0x%08x)\n", cmd);

     one_core_lock( one_core );

     switch (_IOC_TYPE(cmd)) {
          case ONE_LOUNGE:
               ret = lounge_ioctl(dev, oneapp, cmd, arg);
               break;

          case ONE_QUEUE:
               ret = queue_ioctl(dev, oneapp, cmd, arg);
               break;
     }

     one_core_unlock( one_core );

     return ret;
}

static struct file_operations one_fops = {
     .owner = THIS_MODULE,
     .open = one_open,
     .release = one_release,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
     .unlocked_ioctl = one_ioctl,
#else
     .ioctl = one_ioctl,
#endif
};

/******************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int __init register_devices(void)
{
     int i;

     one_major = ONE_MAJOR;

     if (register_chrdev(one_major, "one", &one_fops)) {
          one_major = register_chrdev(0, "one", &one_fops);
          if (one_major <= 0) {
               printk (KERN_ERR "one: unable to register one device\n");
               return -EIO;
          }
          printk(KERN_ERR "one: unable to register major %d. Registered %d instead\n", ONE_MAJOR, one_major);
     }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
     one_class = class_create(THIS_MODULE, "one");
#else
     one_class = class_simple_create(THIS_MODULE, "one");
#endif
     if (IS_ERR(one_class)) {
          unregister_chrdev(one_major, "one");
          return PTR_ERR(one_class);
     }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
     devfs_mk_dir("one");
#endif

     for (i = 0; i < NUM_MINORS; i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
          device_create(one_class,
                        NULL,
                        MKDEV(one_major, i), NULL, "one%d", i);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
          device_create(one_class,
                        NULL, MKDEV(one_major, i), "one%d", i);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
          class_device_create(one_class,
                              NULL,
                              MKDEV(one_major, i),
                              NULL, "one%d", i);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
          class_device_create(one_class,
                              MKDEV(one_major, i),
                              NULL, "one%d", i);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
          class_simple_device_add(one_class,
                                  MKDEV(one_major, i),
                                  NULL, "one%d", i);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
          devfs_mk_cdev(MKDEV(one_major, i),
                        S_IFCHR | S_IRUSR | S_IWUSR, "one/%d", i);
#endif
     }

     return 0;
}
#else
static int __init register_devices(void)
{
     int i;
     char buf[16];

     if (devfs_register_chrdev(one_major, "one", &one_fops)) {
          printk(KERN_ERR "one: unable to get major %d\n",
                 one_major);
          return -EIO;
     }

     for (i = 0; i < NUM_MINORS; i++) {
          snprintf(buf, 16, "one/%d", i);

          devfs_handles[i] = devfs_register(NULL, buf, DEVFS_FL_DEFAULT,
                                            one_major, i,
                                            S_IFCHR | S_IRUSR | S_IWUSR,
                                            &one_fops, NULL);
     }

     return 0;
}
#endif



int __init one_init(void)
{
     int ret;

     printk( KERN_INFO "%s()\n", __FUNCTION__ );

     one_core_enter( &one_core );

     ret = register_devices();
     if (ret)
          return ret;

     proc_one_dir = proc_mkdir("one", NULL);

     return 0;
}

/******************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static void __exit deregister_devices(void)
{
     int i;

     unregister_chrdev(one_major, "one");

     for (i = 0; i < NUM_MINORS; i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
          device_destroy(one_class, MKDEV(one_major, i));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
          class_device_destroy(one_class, MKDEV(one_major, i));
#else
          class_simple_device_remove(MKDEV(one_major, i));
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
          devfs_remove("one/%d", i);
#endif
     }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
     class_destroy(one_class);
#else
     class_simple_destroy(one_class);
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
     devfs_remove("one");
#endif
}
#else
static void __exit deregister_devices(void)
{
     int i;

     devfs_unregister_chrdev(one_major, "one");

     for (i = 0; i < NUM_MINORS; i++)
          devfs_unregister(devfs_handles[i]);
}
#endif

void __exit one_exit(void)
{
     deregister_devices();

     remove_proc_entry("one", NULL);

     one_core_exit( one_core );
}

module_init(one_init);
module_exit(one_exit);

