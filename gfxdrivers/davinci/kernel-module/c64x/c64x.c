/*
   TI Davinci driver - C64X+ DSP Kernel Module

   (c) Copyright 2007  Telio AG

   Written by Olaf Dreesen <dreesen@qarx.de>.

   All rights reserved.

   This module is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>


MODULE_LICENSE("GPL v2");
//MODULE_LICENSE("Propietary");
MODULE_AUTHOR("Olaf Dreesen <dreesen@qarx.de>");
MODULE_DESCRIPTION("A little c64+ handling module.");

#define C_MOD_MAJOR	400
#define C_MOD_NUM_DEV	1
#define C_MOD_NAME	"c64x"
#define F_NAME		"c64x_drv.bin"

#define CODE_BASE	0x00800000

/* DDR2:
 *
 * transfer buffer
 */
#define R_BASE		0x8e000000
#define R_LEN		0x02000000

/* L2RAM:
 *
 * 0x00800000 - 0x0080FFFF C64x+
 * 0x11800000 - 0x1180FFFF ARM
 */
#define D_BASE		0x11800000
#define D_LEN		0x00010000

/* L1DRAM:
 *
 * 0x00F04000 - 0x00F0FFFF C64x+
 * 0x11F04000 - 0x11F0FFFF ARM
 *
 * Queue controls	@ 0x00F04000	(4096 Bytes)
 */
#define Q_BASE		0x11F04000
#define Q_LEN		0x00001000

#define HQueueDSP	(l1dram[0x00>>2])
#define HQueueARM	(l1dram[0x04>>2])
#define LQueueDSP	(l1dram[0x08>>2])
#define LQueueARM	(l1dram[0x0C>>2])
#define DSPidle		(l1dram[0x10>>2])

/* IO Register needed:
 *
 *	0x01C40008	DSPBOOTADDR		DSP Boot Address
 *	0x01C40010	INTGEN			Interrupt Generator
 *	0x01C40038	CHP_SHRTSW		DSP Power
 *	0x01C4169C	MDCFG39			DSP Module config
 *	0x01C41A9C	MDCTL39			DSP Module control
 */
#define IO_BASE		0x01c40000
#define IO_LEN		0x00010000

#define DSPBOOTADDR	(mmr[0x0008>>2])
#define INTGEN		(mmr[0x0010>>2])
#define CHP_SHRTSW	(mmr[0x0038>>2])
#define MDCFG39		(mmr[0x169C>>2])
#define MDCTL39		(mmr[0x1A9C>>2])

MODULE_FIRMWARE(F_NAME);

static dev_t dev_major;
static struct cdev*dev_cdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
static struct class*dev_class;
#else
static struct class_simple*dev_class;
#endif

static volatile unsigned int*mmr=0;
static unsigned char*l2ram=0;
static volatile unsigned int*l1dram=0;

#ifdef C64X_IRQ
static int dev_irq=7;

/* IRQ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)
static irqreturn_t dev_irq_handler(int irq,void*dev_id) {
#else
static irqreturn_t dev_irq_handler(int irq,void*dev_id,struct pt_regs*regs) {
#endif
  return IRQ_HANDLED;
}
#endif

static u32 opencnt=0;

/* char-dev */
static int dev_open(struct inode*inode,struct file*filp) {
  if (opencnt++==0) {
    DSPidle=0;
    MDCTL39=0x00000103;		/* Go! Go, go Go! */
    while(DSPidle==0);
  }
  return 0;
}
static int dev_release(struct inode*inode,struct file*filp) {
  if (--opencnt==0) {
    MDCTL39=0x00000000;		/* local reset */
  }
  return 0;
}

static ssize_t dev_write(struct file*filp,const char __user*buffer,size_t len,loff_t*off) {
  long ret=0;
  unsigned long offset=*off;
  if (offset<D_LEN) {
    if ((offset+len)>=D_LEN) { len=D_LEN-offset; }
//    printk(KERN_INFO "c64x+ : read got offset %08lx %08lx\n",offset,(long)len);
    ret=len;
    *off+=len;
  }
  return ret;
}
static ssize_t dev_read(struct file*filp,char __user*buffer,size_t len,loff_t*off) {
  long ret=0;
  unsigned long offset=*off;
  if (offset<D_LEN) {
    if ((offset+len)>=D_LEN) { len=D_LEN-offset; }
//    printk(KERN_INFO "c64x+ : read got offset %08lx %08lx\n",offset,(long)len);
    ret=len;
    ret-=copy_to_user(buffer,(l2ram+offset),len);
    *off+=len;
  }
  return ret;
}

static int dev_mmap(struct file * file, struct vm_area_struct * vma) {
  size_t size=vma->vm_end-vma->vm_start;
  if (vma->vm_pgoff) {
    if (size!=R_LEN) return -EINVAL;
#if defined(pgprot_writecombine)
    vma->vm_page_prot=pgprot_writecombine(vma->vm_page_prot);
#endif
    if (remap_pfn_range(vma,
			  vma->vm_start,
			  R_BASE>>PAGE_SHIFT,
			  size,
			  vma->vm_page_prot))
      return -EAGAIN;
  }
  else {
    if (size!=Q_LEN) return -EINVAL;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if (remap_pfn_range(vma,
			  vma->vm_start,
			  Q_BASE>>PAGE_SHIFT,
			  size,
			  vma->vm_page_prot))
      return -EAGAIN;
  }
  return 0;
}

static int dev_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case 1: // RESET
    MDCTL39=0x00000000;		/* local reset */
    mdelay(10);
    DSPidle=0;
    MDCTL39=0x00000103;
    break;
  default:
    printk(KERN_INFO "c64x+ : unknown ioctl : cmd=%08x\n",cmd);
    return -EAGAIN;
    break;
  }
  return 0;
}
static struct file_operations dev_file_ops={
  .owner	= THIS_MODULE,
  .open 	= dev_open,
  .release	= dev_release,
  .read 	= dev_read,
  .write	= dev_write,
  .mmap		= dev_mmap,
  .ioctl	= dev_ioctl,
};

/* INIT */
static __initdata struct device dev_device = {
  .bus_id = "c64x0",
};
static int __init dev_init(void) {
  int ret=-EIO;
  const struct firmware*fw = NULL;

  printk(KERN_INFO "c64x+ : module load\n");

  /* get the 'device' memory */
  if ((mmr=ioremap(IO_BASE,IO_LEN))==0) {
    printk(KERN_ERR "c64x+ : module couldn't get IO-MMR\n");
    goto err0;
  }
  printk(KERN_INFO "c64x+ : DSP bootaddr: %08x\n",DSPBOOTADDR);
  printk(KERN_INFO "c64x+ : got mmr %p %08x %08x\n",mmr,MDCTL39,MDCFG39);

  printk(KERN_INFO "c64x+ : switch state: %08x\n",CHP_SHRTSW);

  MDCTL39=0x00000000;		/* local reset */
  mdelay(10);
  DSPBOOTADDR=CODE_BASE;	/* set DSP base address */

//  printk(KERN_INFO "c64x+ : check0: %p %08x %08x\n",mmr,MDCTL39,MDCFG39);

  /* get the 'device' memory */
  if ((l1dram=ioremap(Q_BASE,Q_LEN))==0) {
    printk(KERN_ERR "c64x+ : module couldn't get L1 dsp-memory\n");
    goto err1;
  }
  printk(KERN_INFO "c64x+ : module got L1D @ %p\n",l1dram);

  if ((l2ram=ioremap(D_BASE,D_LEN))==0) {
    printk(KERN_ERR "c64x+ : module couldn't get L2 dsp-memory\n");
    goto err2;
  }
  printk(KERN_INFO "c64x+ : module got L2 @ %p\n",l2ram);

  /* request firmware */
  device_initialize(&dev_device);
  ret=device_add(&dev_device);
  if (ret) {
       printk(KERN_ERR "c64x+ : device_add failed\n");
       goto err3;
  }
  printk(KERN_INFO "c64x+ : module requesting firmware '%s'\n",F_NAME);
  ret=request_firmware(&fw,F_NAME,&dev_device);
  printk(KERN_INFO "c64x+ : module got fw %p\n",fw);
  if (ret) {
    printk(KERN_ERR "c64x+ : no firmware upload (timeout or file not found?)\n");
    device_del(&dev_device);
    goto err3;
  }
  printk(KERN_INFO "c64x+ : firmware upload %p %zd\n",fw->data,fw->size);
  if (fw->size>32768) {
    printk(KERN_ERR "c64x+ : firmware too big! 32768 is maximum (for now)\n");
    release_firmware(fw);
    device_del(&dev_device);
    goto err3;
  }
  if (memcmp(fw->data+8,"C64x+DV",8)) {
    printk(KERN_ERR "c64x+ : firmware signature missing\n");
    release_firmware(fw);
    device_del(&dev_device);
    goto err3;
  }
  /* move firmware into the hardware buffer here. */
  memcpy(l2ram,fw->data,fw->size);
  release_firmware(fw);
  device_del(&dev_device);

#if 0
  /* release DSP */
  printk(KERN_INFO "c64x+ : check1: %p %08x %08x\n",mmr,MDCTL39,MDCFG39);
  MDCTL39=0x00000103;		/* Hopefully run... */
  printk(KERN_INFO "c64x+ : check2: %p %08x %08x\n",mmr,MDCTL39,MDCFG39);
  printk(KERN_INFO "c64x+ : check3: %08x\n",DSPBOOTADDR);
#endif

  /* register char-dev */
  dev_major=MKDEV(C_MOD_MAJOR,0);
  ret=register_chrdev_region(dev_major,C_MOD_NUM_DEV,C_MOD_NAME);
  if (ret) {
    printk(KERN_ERR "c64x+ : can't get chrdev %d\n",C_MOD_MAJOR);
    goto err3;
  }

  /* allocate cdev */
  dev_cdev=cdev_alloc();
  dev_cdev->ops=&dev_file_ops;
  	/* cdev_init(&dev_data.cdev,&dev_file_ops); */
  ret=cdev_add(dev_cdev,dev_major,1);
  if (ret) {
    printk(KERN_ERR "c64x+ : can't allocate cdev\n");
    goto err4;
  }

#ifdef C64X_IRQ
  /* allocate interrupt slot */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)
  ret=request_irq(dev_irq,dev_irq_handler,IRQF_DISABLED,C_MOD_NAME,NULL);
#else
  ret=request_irq(dev_irq,dev_irq_handler,SA_INTERRUPT ,C_MOD_NAME,NULL);
#endif
  if (ret) {
    printk(KERN_ERR "c64x+ : can't get IRQ %d\n",dev_irq);
    goto err5;
  }
#endif

  /* tell sysfs/udev */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
  dev_class=class_create(THIS_MODULE,C_MOD_NAME);
#else
  dev_class=class_simple_create(THIS_MODULE,C_MOD_NAME);
#endif
  if (IS_ERR(dev_class)) {
    ret=PTR_ERR(dev_class);
    printk(KERN_ERR "c64x+ : can't allocate class\n");
    goto err6;
  }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
  class_device_create(dev_class,NULL,dev_major,NULL,C_MOD_NAME"%d",0);
#else
  class_simple_device_add(dev_class,dev_major,NULL,C_MOD_NAME"%d",0);
#endif

  printk(KERN_INFO "c64x+ : module load finished\n");
  return 0;
  /* error out */
err6:
#ifdef C64X_IRQ
  free_irq(dev_irq,0);
err5:
#endif
  cdev_del(dev_cdev);
err4:
  unregister_chrdev_region(dev_major,1);
err3:
  iounmap(l2ram);
err2:
  iounmap((void*)l1dram);
err1:
  iounmap((void*)mmr);
err0:
  return ret;
}
module_init(dev_init);

/* EXIT */
static void __exit dev_exit(void) {
  /* Put the DSP into Reset */
  MDCTL39=0x00000000;
  /* release the DSP memory */
  iounmap((void*)mmr);
  iounmap((void*)l1dram);
  iounmap(l2ram);
  /* release all the other resources */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
  class_device_destroy(dev_class,dev_major);
  class_destroy(dev_class);
#else
  class_simple_device_remove(dev_major);
  class_simple_destroy(dev_class);
#endif
#ifdef C64X_IRQ
  free_irq(dev_irq,0);
#endif
  cdev_del(dev_cdev);
  unregister_chrdev_region(dev_major,C_MOD_NUM_DEV);
}
module_exit(dev_exit);

