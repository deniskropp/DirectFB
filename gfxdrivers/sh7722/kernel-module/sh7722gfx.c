/*
 *	SH7722 Graphics Device
 *
 *	Copyright (C) 2006-2007  IGEL Co.,Ltd
 *
 *      Written by Denis Oliver Kropp <dok@directfb.org>
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License v2
 *	as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <sh7722gfx.h>

//#define SH7722GFX_DEBUG
//#define SH7722GFX_IRQ_POLLER

/**********************************************************************************************************************/

#define	ENGINE_REG_TOP   0xfd000000
#define SH7722_BEU_BASE  0xFE930000

#define BEM_REG(x)       (*(volatile u32*)((x)+ENGINE_REG_TOP))
#define BEU_REG(x)       (*(volatile u32*)((x)+SH7722_BEU_BASE))

#define	BEM_HC_STATUS			BEM_REG(0x00000)
#define	BEM_HC_RESET			BEM_REG(0x00004)
#define	BEM_HC_CLOCK			BEM_REG(0x00008)
#define	BEM_HC_INT_STATUS		BEM_REG(0x00020)
#define	BEM_HC_INT_MASK			BEM_REG(0x00024)
#define	BEM_HC_INT_CLEAR		BEM_REG(0x00028)
#define	BEM_HC_CACHE_FLUSH		BEM_REG(0x0002C)
#define	BEM_HC_DMA_ADR			BEM_REG(0x00040)
#define	BEM_HC_DMA_START		BEM_REG(0x00044)
#define	BEM_HC_DMA_STOP			BEM_REG(0x00048)

#define BEVTR                   BEU_REG(0x018C)

/**********************************************************************************************************************/

#ifdef SH7722GFX_DEBUG
#define QPRINT(x...)     do {                                                        \
     char buf[128];                                                                  \
     struct timeval tv;                                                              \
     do_gettimeofday( &tv );                                                         \
     snprintf( buf, sizeof(buf), x );                                                \
     printk( KERN_DEBUG "%ld.%03ld.%03ld - %-17s: %s\n",                             \
             tv.tv_sec - base_time.tv_sec,                                           \
             tv.tv_usec / 1000, tv.tv_usec % 1000, __FUNCTION__, buf );              \
} while (0)
#else
#define QPRINT(x...)     do {} while (0)
#endif

#define QDUMP(msg)       QPRINT( "%-12s (%s, hw %5d-%5d, next %5d-%5d, %svalid, "    \
                                 "HC %07x, INT %06x)", msg,                          \
                                 shared->hw_running ? "running" : "   idle",         \
                                 shared->hw_start,                                   \
                                 shared->hw_end,                                     \
                                 shared->next_start,                                 \
                                 shared->next_end,                                   \
                                 shared->next_valid ? "  " : "in",                   \
                                 BEM_HC_STATUS, BEM_HC_INT_STATUS );

/**********************************************************************************************************************/

static DECLARE_WAIT_QUEUE_HEAD( wait_idle );
static DECLARE_WAIT_QUEUE_HEAD( wait_next );

static SH7722GfxSharedArea *shared;

static struct timeval       base_time;

#ifndef SHARED_AREA_PHYS
static struct page         *shared_page;
static unsigned int         shared_order;
#endif

#ifdef SH7722GFX_IRQ_POLLER
static int                  stop_poller;
#endif

/**********************************************************************************************************************/

static int
sh7722_reset( SH7722GfxSharedArea *shared )
{
     int i;

     do_gettimeofday( &base_time );

     QPRINT( "Resetting hardware..." );

     BEM_HC_CLOCK = 0;
     for (i=0; i<30000; i++);
     BEM_HC_CLOCK = 0x1111;

     BEM_HC_RESET = 0x1111;
     for (i=0; i<30000; i++);
     BEM_HC_RESET = 0;


     QPRINT( "Initializing shared area..." );

     memset( (void*) shared, 0, sizeof(SH7722GfxSharedArea) );

     shared->buffer_phys = virt_to_phys(&shared->buffer[0]);
     shared->magic       = SH7722GFX_SHARED_MAGIC;


     QPRINT( "Clearing interrupts..." );

     BEM_HC_INT_CLEAR = 0x111111;
     BEM_HC_INT_MASK  = 0x110011;

     QDUMP( "Ready" );

     return 0;
}

static int
sh7722_wait_idle( SH7722GfxSharedArea *shared )
{
     int ret;

     QDUMP( "Waiting....." );

     /* Does not need to be atomic. There's a lock in user space,
      * but anyhow, this is just for statistics. */
     shared->num_wait_idle++;

     ret = wait_event_interruptible_timeout( wait_idle, !shared->hw_running, 42*HZ );
     if (!ret) {
          printk( KERN_ERR "%s: TIMEOUT! (%srunning, hw %d-%d, next %d-%d - %svalid, "
                           "STATUS 0x%08x, INT_STATUS 0x%08x)\n",
                  __FUNCTION__,
                  shared->hw_running ? "" : "not ",
                  shared->hw_start,
                  shared->hw_end,
                  shared->next_start,
                  shared->next_end,
                  shared->next_valid ? "" : "not ",
                  BEM_HC_STATUS, BEM_HC_INT_STATUS );
     }

     QDUMP( "........done" );

     return (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
}

static int
sh7722_wait_next( SH7722GfxSharedArea *shared )
{
     int ret;

     QDUMP( "Waiting....." );

     /* Does not need to be atomic. There's a lock in user space,
      * but anyhow, this is just for statistics. */
     shared->num_wait_next++;

     ret = wait_event_interruptible_timeout( wait_next, !shared->hw_running ||
                                             shared->next_start == shared->next_end, 42*HZ );
     if (!ret) {
          printk( KERN_ERR "%s: TIMEOUT! (%srunning, hw %d-%d, next %d-%d - %svalid, "
                           "STATUS 0x%08x, INT_STATUS 0x%08x)\n",
                  __FUNCTION__,
                  shared->hw_running ? "" : "not ",
                  shared->hw_start,
                  shared->hw_end,
                  shared->next_start,
                  shared->next_end,
                  shared->next_valid ? "" : "not ",
                  BEM_HC_STATUS, BEM_HC_INT_STATUS );
     }

     QDUMP( "........done" );

     return (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
}

/**********************************************************************************************************************/

static irqreturn_t
sh7722_beu_irq( int irq, void *ctx )
{
     BEVTR = 0;

     /* Nothing here so far. But Vsync could be added. */

     return IRQ_HANDLED;
}

static irqreturn_t
sh7722_tdg_irq( int irq, void *ctx )
{
     SH7722GfxSharedArea *shared = ctx;
     u32                  status = BEM_HC_INT_STATUS;

     if (! (status & 0x111111)) {
#ifndef SH7722GFX_IRQ_POLLER
          printk( KERN_WARNING "%s: bogus interrupt, INT_STATUS 0x%08x!\n", __FUNCTION__, status );
#endif
          return IRQ_NONE;
     }

     if (status & ~0x100)
          QDUMP( "-Interrupt" );

     if (status & ~0x101100)
          printk( KERN_ERR "%s: error! INT_STATUS 0x%08x!\n", __FUNCTION__, status );

     shared->num_interrupts++;

     /* Clear the interrupt. */
     BEM_HC_INT_CLEAR = status;

     if (status & 0x100010) {
          if (!shared->hw_running)
               printk( KERN_WARNING "%s: hw not running? INT_STATUS 0x%08x!\n", __FUNCTION__, status );

          if (status & 0x10) {
               printk( KERN_ERR "%s: RUNAWAY! (%srunning, hw %d-%d, next %d-%d - %svalid, "
                                "STATUS 0x%08x, INT_STATUS 0x%08x)\n",
                       __FUNCTION__,
                       shared->hw_running ? "" : "not ",
                       shared->hw_start,
                       shared->hw_end,
                       shared->next_start,
                       shared->next_end,
                       shared->next_valid ? "" : "not ",
                       BEM_HC_STATUS, status );

               BEM_HC_RESET = 0x1111;
          }

          /* Next valid means user space is not in the process of extending the buffer. */
          if (shared->next_valid && shared->next_start != shared->next_end) {
               shared->hw_start = shared->next_start;
               shared->hw_end   = shared->next_end;

               shared->next_start = shared->next_end = (shared->hw_end + 1 + 3) & ~3;
               shared->next_valid = 0;

               shared->num_words += shared->hw_end - shared->hw_start;

               shared->num_starts++;

               QDUMP( " '-> Start!" );

               BEM_HC_DMA_ADR   = shared->buffer_phys + shared->hw_start*4;
               BEM_HC_DMA_START = 1;

               wake_up_all( &wait_next );
          }
          else {
               shared->num_idle++;

               QDUMP( " '-> Idle." );

               shared->hw_running = 0;

               wake_up_all( &wait_next );
               wake_up_all( &wait_idle );
          }

          shared->num_done++;
     }

     return IRQ_HANDLED;
}

#ifdef SH7722GFX_IRQ_POLLER
static int
sh7722_tdg_irq_poller( void *arg )
{
     daemonize( "%s", __FUNCTION__ );

     sigfillset( &current->blocked );

     while (!stop_poller) {
          set_current_state( TASK_UNINTERRUPTIBLE );
          schedule_timeout( 1 );

          sh7722_tdg_irq( TDG_IRQ, (void*) arg );
     }

     stop_poller = 0;

     return 0;
}
#endif

/**********************************************************************************************************************/

static int
sh7722gfx_ioctl( struct inode  *inode,
                 struct file   *filp,
                 unsigned int   cmd,
                 unsigned long  arg )
{
     SH7722Register reg;

     switch (cmd) {
          case SH7722GFX_IOCTL_RESET:
               return sh7722_reset( shared );

          case SH7722GFX_IOCTL_WAIT_IDLE:
               return sh7722_wait_idle( shared );

          case SH7722GFX_IOCTL_WAIT_NEXT:
               return sh7722_wait_next( shared );

          case SH7722GFX_IOCTL_SETREG32:
               if (copy_from_user( &reg, arg, sizeof(SH7722Register) ))
                    return -EFAULT;

               /* BEU, LCDC, VOU, JPEG */
               if (reg.address < 0xFE930000 || reg.address > 0xFEA102D0)
                    return -EACCES;

               *(volatile __u32 *) reg.address = reg.value;

               return 0;

          case SH7722GFX_IOCTL_GETREG32:
               if (copy_from_user( &reg, arg, sizeof(SH7722Register) ))
                    return -EFAULT;

               /* BEU, LCDC, VOU, JPEG */
               if (reg.address < 0xFE930000 || reg.address > 0xFEA102D0)
                    return -EACCES;

               reg.value = *(volatile __u32 *) reg.address;

               if (copy_to_user( arg, &reg, sizeof(SH7722Register) ))
                    return -EFAULT;

               return 0;
     }

     return -ENOSYS;
}

static int
sh7722gfx_mmap( struct file           *file,
                struct vm_area_struct *vma )
{
     unsigned int size;

     /* Just allow mapping at offset 0. */
     if (vma->vm_pgoff)
          return -EINVAL;

     /* Check size of requested mapping. */
     size = vma->vm_end - vma->vm_start;
     if (size != PAGE_ALIGN(sizeof(SH7722GfxSharedArea)))
          return -EINVAL;

     /* Set reserved and I/O flag for the area. */
     vma->vm_flags |= VM_RESERVED | VM_IO;

     /* Select uncached access. */
     vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
     return remap_pfn_range( vma, vma->vm_start,
                             virt_to_phys((void*)shared) >> PAGE_SHIFT,
                             size, vma->vm_page_prot );
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
     return remap_page_range( vma, vma->vm_start,
                              virt_to_phys((void*)shared),
                              size, vma->vm_page_prot );
#else
     return io_remap_page_range( vma->vm_start,
                                 virt_to_phys((void*)shared),
                                 size, vma->vm_page_prot );
#endif
}

/**********************************************************************************************************************/

static struct file_operations sh7722gfx_fops = {
     ioctl:    sh7722gfx_ioctl,
     mmap:     sh7722gfx_mmap
};

static struct miscdevice sh7722gfx_miscdev = {
     minor:    196,           // 7*7*2*2
     name:     "sh7722gfx",
     fops:     &sh7722gfx_fops
};

/**********************************************************************************************************************/

static int __init
sh7722gfx_module_init( void )
{
#ifndef SHARED_AREA_PHYS
     int i;
#endif
     int ret;

     /* Register the SH7722 graphics device. */
     ret = misc_register( &sh7722gfx_miscdev );
     if (ret < 0) {
          printk( KERN_ERR "%s: misc_register() for minor %d failed! (error %d)\n",
                  __FUNCTION__, sh7722gfx_miscdev.minor, ret );
          return ret;
     }

     /* Allocate and initialize the shared area. */
#ifdef SHARED_AREA_PHYS
#if SHARED_AREA_SIZE < PAGE_ALIGN(sizeof(SH7722GfxSharedArea))
#error SHARED_AREA_SIZE < PAGE_ALIGN(sizeof(SH7722GfxSharedArea))!
#endif
     shared = ioremap( SHARED_AREA_PHYS, PAGE_ALIGN(sizeof(SH7722GfxSharedArea)) );
#else
     shared_order = get_order(sizeof(SH7722GfxSharedArea));
     shared_page  = alloc_pages( GFP_DMA | GFP_KERNEL, shared_order );
     shared       = ioremap( virt_to_phys( page_address(shared_page) ),
                             PAGE_ALIGN(sizeof(SH7722GfxSharedArea)) );

     for (i=0; i<1<<shared_order; i++)
          SetPageReserved( shared_page + i );
#endif

     printk( KERN_INFO "sh7722gfx: shared area (order %d) at %p [%lx] using %d bytes\n",
             shared_order, shared, virt_to_phys(shared), sizeof(SH7722GfxSharedArea) );

     /* Register the BEU interrupt handler. */
     ret = request_irq( BEU_IRQ, sh7722_beu_irq, IRQF_DISABLED, "BEU", (void*) shared );
     if (ret) {
          printk( KERN_ERR "%s: request_irq() for interrupt %d failed! (error %d)\n",
                  __FUNCTION__, BEU_IRQ, ret );
          goto error_beu;
     }

#ifdef SH7722GFX_IRQ_POLLER
     kernel_thread( sh7722_tdg_irq_poller, (void*) shared, CLONE_KERNEL );
#else
     /* Register the TDG interrupt handler. */
     ret = request_irq( TDG_IRQ, sh7722_tdg_irq, IRQF_DISABLED, "TDG", (void*) shared );
     if (ret) {
          printk( KERN_ERR "%s: request_irq() for interrupt %d failed! (error %d)\n",
                  __FUNCTION__, TDG_IRQ, ret );
          goto error_tdg;
     }
#endif

     sh7722_reset( shared );

     return 0;


#ifndef SH7722GFX_IRQ_POLLER
error_tdg:
     free_irq( BEU_IRQ, (void*) shared );
#endif

error_beu:
#ifndef SHARED_AREA_PHYS
     for (i=0; i<1<<shared_order; i++)
          ClearPageReserved( shared_page + i );

     __free_pages( shared_page, shared_order );
#endif

     misc_deregister( &sh7722gfx_miscdev );

     return ret;
}

module_init( sh7722gfx_module_init );

/**********************************************************************************************************************/

static void __exit
sh7722gfx_module_exit( void )
{
#ifndef SHARED_AREA_PHYS
     int i;
#endif


#ifdef SH7722GFX_IRQ_POLLER
     stop_poller = 1;

     while (stop_poller) {
          set_current_state( TASK_UNINTERRUPTIBLE );
          schedule_timeout( 1 );
     }
#else
     free_irq( TDG_IRQ, (void*) shared );
#endif

     free_irq( BEU_IRQ, (void*) shared );

     misc_deregister( &sh7722gfx_miscdev );


#ifndef SHARED_AREA_PHYS
     for (i=0; i<1<<shared_order; i++)
          ClearPageReserved( shared_page + i );

     __free_pages( shared_page, shared_order );
#endif
}

module_exit( sh7722gfx_module_exit );

/**********************************************************************************************************************/

MODULE_AUTHOR( "Denis Oliver Kropp <dok@directfb.org>" );
MODULE_LICENSE( "GPL v2" );

