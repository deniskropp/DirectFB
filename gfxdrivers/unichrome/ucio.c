/*
   Copyright (c) 2003, 2004 Andreas Robinson, All rights reserved.
   Kernel v2.6 code by André Kriehn.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

/* Standalone Unichrome IO-registers mmap driver.
 * Tested on Linux 2.4.20 and 2.6.0-test3.
 * 
 * Compilation:
 *
 * make -f ucio.mk v24-devfs
 * make -f ucio.mk v24
 * make -f ucio.mk v26-devfs
 * make -f ucio.mk v26
 *
 * Explanation
 *
 * v24-devfs compiles for a 2.4 kernel, relying on devfs.
 * v24 compiles for a 2.4 kernel, without relying on devfs.
 * v26-devfs compiles for a 2.6 kernel, relying on devfs.
 * v26 compiles for a 2.6 kernel, without relying on devfs.
 *
 * If in doubt use v24 or v26, since devfs is not a standard
 * kernel feature.
 *
 * If you select v24 or v26, you must create a device node in
 * your /dev directory, by executing the following  command:
 * (If you have devfs and select v24 or v26, you should
 * not do this.)
 *
 * mknod -m 666 /dev/ucio c 245 0
 * (You need to have root privileges)
 *
 * IMPORTANT: If you forget to do this, DirectFB programs will
 * run, but won't be accelerated, even if you install the module
 * properly.
 *
 * The non-devfs options allocates a major device number, 245.
 * This number is for experimental use only, and can possibly conflict
 * with something else if you have other experimental drivers installed.
 * If you need to change it, edit VIADEV_MAJOR below and adjust your
 * mknod command if you use it.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/pci.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <asm/uaccess.h> /* copy_from_user() */

#include "uc_probe.h"

// Test
#define FBIO_WAITFORVSYNC       _IOW('F', 0x20, u_int32_t)

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#include <linux/vermagic.h>
#endif


// Module configuration ------------------------------------------------------

#define VIADEV_MAJOR 245

// VIA register declarations -------------------------------------------------

/* defines for VIA 3D registers */
#define VIA_REG_INTERRUPT       0x200
#define VIA_REG_STATUS          0x400

/* VIA_REG_INTERRUPT */
#define VIA_IRQ_GLOBAL          (1 << 31)
#define VIA_IRQ_VBI_ENABLE      (1 << 19)
#define VIA_IRQ_VBI_PENDING     (1 << 3)

/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000  /* Virtual Queue is busy */

#define VIA_OUT(hwregs, reg, val) *(volatile u32 *)((hwregs) + (reg)) = (val)
#define VIA_IN(hwregs, reg)       *(volatile u32 *)((hwregs) + (reg))

#define MAXLOOP 0xffffff

// Private declarations ------------------------------------------------------

#define MODULE_NAME "ucio"
#define MY_ASSERT(test, msg, exitcode) if (!test) { printk(msg); return exitcode; }

struct via_devinfo
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#ifndef UC_STATIC_DEVNUM
    devfs_handle_t devhnd;
#endif
#endif
    u16 pciid;                  // PCI id
    char* name;                 // Device name
    struct pci_dev* pcidev;     // PCI device (see linux/pci.h)
    u32 io_base_phy;            // Physical IO register address
    u8* iobase;                 // Kernel-mapped IO register address
    u32 size;                   // IO memory size in bytes

    int busy;

    int irq_num;                // Vertical blanking IRQ number
    int irq_cnt;                // Vertical blanking IRQ counter

    wait_queue_head_t wq;
};

// ----------------------------------------------------------------------------

/// Global device info
static struct via_devinfo di;

/// Get chipset revision.

static int via_get_revision(void)
{
    u8 rev;
    struct pci_dev* dev;
    dev = pci_find_slot(0,0);
    if (dev == NULL) return -1;

    pci_read_config_byte(dev, 0xf6, &rev);
    return rev;
}

/// Probe for Unichrome device

static int via_probe(void)
{
    int i;
    u16 id;

    for (i = 0, id = 0xffff; id > 0; i++) {
        id = uc_via_devices[i].id;
        di.pcidev = pci_find_device(PCI_VENDOR_ID_VIA, id, NULL);
        if (di.pcidev) break;
    }
    di.pciid = id;
    di.name = uc_via_devices[i].name;
    return id;
}

static void via_enable_mmio(void)
{
    // in/out only works when the Unichrome VGA is the primary device.
    // Refer to the XFree86 VIA driver for info on how to set it up
    // when used as secondary driver.

    //outb(inb(0x3c3) | 0x01, 0x3c3);
    //outb(inb(0x3cc) | 0x01, 0x3c2);

    // Unlock Extended IO Space

    outb(0x10, 0x3c4);
    outb(0x01, 0x3c5);

    // Enable MMIO

    outb(0x1a, 0x3c4);
    //outb(inb(0x3c5) | 0x68, 0x3c5);
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

void via_vga_irqhandler(int irq, void *dev_id, struct pt_regs *regs)
{
    struct via_devinfo* vdi = (struct via_devinfo*) dev_id;
    u32 status = VIA_IN(di.iobase, VIA_REG_INTERRUPT);

    if (status & VIA_IRQ_VBI_PENDING) {
        VIA_OUT(di.iobase, VIA_REG_INTERRUPT, status | VIA_IRQ_VBI_PENDING);

        vdi->irq_cnt++;
        wake_up_interruptible(&di.wq);
    }
}

static int install_irq_handler(int irq_num)
{
    int retval;

    retval = request_irq(irq_num, via_vga_irqhandler,
    SA_SHIRQ | SA_INTERRUPT, MODULE_NAME, (void *) &di);

    if (retval == -EINVAL) {
        printk(MODULE_NAME ": No IRQ handler installed, "
            "IRQ number %d is not valid.\n", irq_num);
    }
    else if (retval == -EBUSY) {
        printk(MODULE_NAME ": No IRQ handler installed, "
            "IRQ %d is busy. Check BIOS.\n", irq_num);
    }
    else if (retval < 0) {
        printk(MODULE_NAME ": No IRQ handler installed. "
            "Tried IRQ number %d.\n", irq_num);
    }

    di.irq_cnt = 0;
    if (retval >= 0) {
        di.irq_num = irq_num;

        // Enable vertical sync IRQ.
        // BUG WARNING: May conflict with other VIA drivers using vsync IRQ.
        // TODO: Contact VIA to get the same functionality into viafb.

        VIA_OUT(di.iobase, VIA_REG_INTERRUPT,
            VIA_IN(di.iobase, VIA_REG_INTERRUPT) |
                VIA_IRQ_GLOBAL | VIA_IRQ_VBI_ENABLE | VIA_IRQ_VBI_PENDING);

        outb(0x11, 0x3d4);
        outb(inb(0x3d5) | 0x30, 0x3d5);

        printk("IRQ(0x200) = %x\n", VIA_IN(di.iobase, VIA_REG_INTERRUPT));
    }

    return retval;
}

// Module and device file I/O functions --------------------------------------

static int viadev_open(struct inode *inode, struct file *filp)
{
    return 0; 
}
        
static int viadev_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* BUG warning: This function is probably not thread/process-safe.
 * However, the kernel uses spinlocks in the wq, so maybe it is...? */

static int viadev_ioctl(struct inode *inode, struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
    case FBIO_WAITFORVSYNC:
        interruptible_sleep_on(&di.wq);
        return 0;
    default:
        return -ENOTTY;
    }
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
    .ioctl = viadev_ioctl,
    .mmap = viadev_mmap,
    .open = viadev_open,
    .release = viadev_release,
};

static int __init viadev_init(void)
{
    // Find the device

    via_probe();
    MY_ASSERT(di.pcidev,
        MODULE_NAME ": No VIA Unichrome graphics device found.\n", -ENODEV);
    MY_ASSERT(!di.pcidev->driver,
        MODULE_NAME ": Another driver already controls the Unichrome graphics device.\n", -EBUSY);

    // Map physical IO memory address into kernel space.

    di.io_base_phy = pci_resource_start(di.pcidev, 1);
    di.size = pci_resource_len(di.pcidev, 1);
    MY_ASSERT(request_mem_region(di.io_base_phy, di.size, MODULE_NAME),
        MODULE_NAME ": Memory mapping failed (1).\n", -EBUSY);

    di.iobase = ioremap(di.io_base_phy, di.size);
    MY_ASSERT(di.iobase, MODULE_NAME ": Memory mapping failed (2).\n", -EBUSY);

    init_waitqueue_head(&di.wq);

    // Try to install a vertical blanking interrupt handler.
    // Failure is not fatal.
    install_irq_handler(di.pcidev->irq);

    // TODO: Register PCI device driver. Or not. We don't really have to.

    // Register a character device.

#if UC_STATIC_DEVNUM
    int result;
    result = register_chrdev(VIADEV_MAJOR, MODULE_NAME, &viadev_fops);
    MY_ASSERT(!result, MODULE_NAME " Unable to register driver\n", result);
#else

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
    int result;
    result = register_chrdev(VIADEV_MAJOR, MODULE_NAME, &viadev_fops);
    MY_ASSERT(!result, MODULE_NAME " Unable to register driver\n", result);
    devfs_mk_cdev(MKDEV(VIADEV_MAJOR, 0), S_IFCHR | S_IRUSR | S_IWUSR, MODULE_NAME);
#else
    di.devhnd = devfs_register(NULL, MODULE_NAME, DEVFS_FL_AUTO_DEVNUM,
        0, 0, S_IFCHR | S_IRUGO | S_IWUGO, &viadev_fops, &di);
    MY_ASSERT(di.devhnd, MODULE_NAME ": Could not register a /dev entry.\n", -EAGAIN);
#endif

#endif // UC_STATIC_DEVNUM

    printk(MODULE_NAME " installed. VIA %s rev %d detected. ",
        di.name, via_get_revision());

    if (di.irq_num != 0)
        printk("Using IRQ %d.\n", di.irq_num);
    else
        printk("\n");

    via_enable_mmio();
    
    return 0;
}

static void __exit viadev_exit(void)
{
    if (di.irq_num != 0) {

        // Disable vertical sync IRQ.
        // BUG WARNING: May conflict with other VIA drivers using vsync IRQ.

        VIA_OUT(di.iobase, VIA_REG_INTERRUPT,
            (VIA_IN(di.iobase, VIA_REG_INTERRUPT) & ~VIA_IRQ_VBI_ENABLE)
            | VIA_IRQ_VBI_PENDING);

        outb(0x11, 0x3d4);
        outb(inb(0x3d5) & ~0x30, 0x3d5);

        free_irq(di.irq_num, (void *) &di);
        printk(MODULE_NAME ": %d interrupt requests serviced.\n", di.irq_cnt);
    }
#if UC_STATIC_DEVNUM
    unregister_chrdev(VIADEV_MAJOR, MODULE_NAME);
#else

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
    unregister_chrdev(VIADEV_MAJOR, MODULE_NAME);
    devfs_remove(MODULE_NAME);
#else
    devfs_unregister(di.devhnd);
#endif

#endif // UC_STATIC_DEVNUM

    via_wait_idle();
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
