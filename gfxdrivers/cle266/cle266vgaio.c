/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.
   Kernel v2.6 code by André Kriehn.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

/* Standalone CLE266 IO-registers mmap driver.
 * Tested on a Linux 2.4.20 and 2.6.0-test3. Needs devfs to run.
 * 
 * Compilation:
 *
 * make -f cle266vgaio.mk v24
 * make -f cle266vgaio.mk v26
 *
 * Note that the 2.6 version allocates a major device number, 245.
 * This is for experimental use only, but can possibly conflict with
 * something else if you have other experimental drivers installed.
 * If you need to change it, edit VIADEV_MAJOR below.
 *
 * Usage:
 *
 * Install the module, open the file /dev/cle266vgaio,
 * and mmap the area you want access to.
 * Only tested mapping 0 - 16Mbyte area yet.
 *
 * Help wanted:
 *
 * Someone with more knowledge of the virtual memory system
 * and the PCI subsystem should take a look at the code in
 * init_module() and viadev_mmap().
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/pci.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#include <linux/vermagic.h>
#endif

// VIA register declarations -------------------------------------------------

/* defines for VIA 3D registers */
#define VIA_REG_STATUS          0x400

/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000 /* Virtual Queue is busy */

#define VIA_IN(hwregs, reg)  *(volatile u32 *)((hwregs) + (reg))
#define MAXLOOP 0xffffff

// Private declarations ------------------------------------------------------

#define MODULE_NAME "cle266vgaio"
#define _PCI_DEVICE_ID_VIA_CLE3122  0x3122
#define VIADEV_MAJOR 245    /* 2.6 kernel only. */
#define MY_ASSERT(test, msg, exitcode) if (!test) { printk(msg); return exitcode; }


struct via_devinfo
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
    devfs_handle_t devhnd;
//    struct file_operations fops;
#endif
    struct pci_dev* pcidev;     // PCI device (see linux/pci.h)
    u32 io_base_phy;            // Physical IO register address
    u8* iobase;                 // Kernel-mapped IO register address
    u32 size;                   // IO memory size in bytes

    int busy;
};

// ----------------------------------------------------------------------------

/// Global device info
static struct via_devinfo di;

/// Get chipset revision

static int via_get_revision(void)
{
    u8 rev;
    struct pci_dev* dev;

    dev = pci_find_slot(0,0);
    if (dev == NULL) return 0;

    pci_read_config_byte(dev, 0xf6, &rev);

    return rev;
}

static void via_enable_mmio(void)
{
    // in/out only works when the CLE266 VGA is the primary device.
    // Refer to the XFree86 VIA driver for info on how to set it up
    // when used as secondary driver.

    outb(inb(0x3c3) | 0x01, 0x3c3);
    outb(inb(0x3cc) | 0x01, 0x3c2);

    // Unlock Extended IO Space

    outb(0x10, 0x3c4);
    outb(0x01, 0x3c5);

    // Enable MMIO

    // If enabled, this crashes DirectFB. Strange - X uses this setting.
    // Fortunately, it seems to work anyway.

    //outb(0x1a, 0x3c4);
    //outb(inb(0x3c5) | 0x68, 0x3c5);
}

static void via_disable_mmio(void)
{
//  outb(0x1a, 0x3c4);
//  outb(inb(0x3c5) & 0x97, 0x3c5);
}

static int via_wait_idle(void)
{
    int loop = 0;

    while (!(VIA_IN(di.iobase, VIA_REG_STATUS) 
        & VIA_VR_QUEUE_BUSY) && 
        (loop++ < MAXLOOP));

    while ((VIA_IN(di.iobase, VIA_REG_STATUS) &
      (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)) &&
      (loop++ < MAXLOOP));

    return loop >= MAXLOOP;
}

// Module and device file I/O functions --------------------------------------

static int viadev_open(struct inode *inode, struct file *filp)
{
// we need to open the device more than once for multi application core
/*
    if (di.busy) return -EBUSY;
    di.busy = 1
*/
    return 0; 
}
        
static int viadev_release(struct inode *inode, struct file *filp)
{
    di.busy = 0;
    return 0;
}   

static int viadev_mmap(struct file* filp, struct vm_area_struct* vma)
{
    unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long phy = di.io_base_phy + off;
    unsigned long vsize = vma->vm_end - vma->vm_start;
    unsigned long psize = di.size - off;

    if (vsize > psize) return -EINVAL;

    vma->vm_pgoff = phy >> PAGE_SHIFT;

    if (boot_cpu_data.x86 > 3)
        pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;

    vma->vm_flags |= VM_IO | VM_RESERVED | VM_DONTEXPAND;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
    if (io_remap_page_range(vma,vma->vm_start, phy, vsize, vma->vm_page_prot))
       return -EAGAIN;
#else
    if (io_remap_page_range(vma->vm_start, phy, vsize, vma->vm_page_prot))
       return -EAGAIN;
#endif

    return 0;
}

static struct file_operations viadev_fops = {
    .owner = THIS_MODULE,
    .mmap = viadev_mmap,
    .open = viadev_open,
    .release = viadev_release,
};

static int __init viadev_init(void)
{
    // Find the device

    di.pcidev = pci_find_device(PCI_VENDOR_ID_VIA,
        _PCI_DEVICE_ID_VIA_CLE3122, NULL);
    MY_ASSERT(di.pcidev,
        MODULE_NAME ": VIA CLE266 graphics device not found.", -ENODEV);
    MY_ASSERT(!di.pcidev->driver,
        MODULE_NAME ": There is already a driver installed.", -EBUSY);

    // Map physical IO memory address into kernel space.

    di.io_base_phy = pci_resource_start(di.pcidev, 1);
    di.size = pci_resource_len(di.pcidev, 1);
    MY_ASSERT(request_mem_region(di.io_base_phy, di.size, MODULE_NAME),
        MODULE_NAME ": Memory mapping failed (1).", -EBUSY);

    di.iobase = ioremap(di.io_base_phy, di.size);
    MY_ASSERT(di.iobase, MODULE_NAME ": Memory mapping failed (2).", -EBUSY);

    di.busy = 0;

    // TODO: Register PCI device driver. Or not. We don't really need to.

    // Register a character device. Uses devfs

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
    int result;
    result = register_chrdev(VIADEV_MAJOR, MODULE_NAME, &viadev_fops);
    MY_ASSERT(!result, MODULE_NAME " Unable to register driver\n", result);
    devfs_mk_cdev(MKDEV(VIADEV_MAJOR, 0), S_IFCHR | S_IRUSR | S_IWUSR, MODULE_NAME);
#else
    di.devhnd = devfs_register(NULL, MODULE_NAME, DEVFS_FL_AUTO_DEVNUM,
        0, 0, S_IFCHR | S_IRUGO | S_IWUGO, &viadev_fops, &di);
    MY_ASSERT(di.devhnd, MODULE_NAME ": Could not register a /dev entry.", -EAGAIN);
#endif

printk(MODULE_NAME " installed. Hardware rev %d detected.\n", via_get_revision());
    via_enable_mmio();
    
    return 0;
}

static void __exit viadev_exit(void)
{

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
    unregister_chrdev(VIADEV_MAJOR, MODULE_NAME);
    devfs_remove(MODULE_NAME);
#else
    devfs_unregister(di.devhnd);
#endif

    via_wait_idle();
    via_disable_mmio();
    iounmap(di.iobase);
    release_mem_region(di.io_base_phy, di.size);

    printk(MODULE_NAME " removed.\n");
}

module_init(viadev_init);
module_exit(viadev_exit);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)

MODULE_INFO(vermagic, VERMAGIC_STRING);

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";

#endif

MODULE_AUTHOR("Andreas Robinson");
MODULE_DESCRIPTION("VIA UniChrome Driver");
MODULE_LICENSE("GPL");
