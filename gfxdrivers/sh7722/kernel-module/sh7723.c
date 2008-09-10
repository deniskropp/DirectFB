/*
 *	SH7723 Graphics Device
 *
 *	Copyright (C) 2006-2008  IGEL Co.,Ltd
 *
 *      Written by Janine Kropp <nin@directfb.org>,
 *                 Denis Oliver Kropp <dok@directfb.org>
 *                 
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License v2
 *	as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach/irq.h>

#include <sh772x_gfx.h>


//#define SH7723GFX_DEBUG_2DG
//#define SH7723GFX_IRQ_POLLER


/**********************************************************************************************************************/

#ifndef SH7723_BEU_IRQ
#define SH7723_BEU_IRQ 53
#endif

#ifndef SH7723_TDG_IRQ
#define SH7723_TDG_IRQ 44
#endif

/**********************************************************************************************************************/

#define	ENGINE_REG_TOP   0xA4680000
#define SH7723_BEU_BASE  0xFE930000

#define M2DG_REG(x)      (*(volatile u32*)((x)+ENGINE_REG_TOP))
#define BEU_REG(x)       (*(volatile u32*)((x)+SH7723_BEU_BASE))

#define	M2DG_SCLR             M2DG_REG(0x000)
#define	M2DG_DLSAR            M2DG_REG(0x048)


#define M2DG_STATUS           M2DG_REG(0x004)
#define M2DG_STATUS_CLEAR     M2DG_REG(0x008)
#define M2DG_INT_ENABLE       M2DG_REG(0x00c)

#define M2DG_SCLR_START       0x00000001 
#define M2DG_SCLR_RESET       0x80000000

#define M2DG_INT_TRAP         0x0001 
#define M2DG_INT_INTERRUPT    0x0002
#define M2DG_INT_ERROR        0x0004
#define M2DG_INT_ANY          0x0007 

#define BEVTR                 BEU_REG(0x0018C)

/**********************************************************************************************************************/

#ifdef SH7723GFX_DEBUG_2DG
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
                                 "STATUS 0x%07x)", msg,                          \
                                 shared->hw_running ? "running" : "   idle",         \
                                 shared->hw_start,                                   \
                                 shared->hw_end,                                     \
                                 shared->next_start,                                 \
                                 shared->next_end,                                   \
                                 shared->next_valid ? "  " : "in",                   \
                                 M2DG_STATUS & M2DG_INT_ANY );

/**********************************************************************************************************************/

static DECLARE_WAIT_QUEUE_HEAD( wait_idle );
static DECLARE_WAIT_QUEUE_HEAD( wait_next );

static SH772xGfxSharedArea *shared;

static struct timeval       base_time;

#ifndef SHARED_AREA_PHYS
static struct page         *shared_page;
static unsigned int         shared_order;
#endif

#ifdef SH7723GFX_IRQ_POLLER
static int                  stop_poller;
#endif

/**********************************************************************************************************************/

static int
sh7723_reset( SH772xGfxSharedArea *shared )
{
     do_gettimeofday( &base_time );

     QPRINT( "Resetting hardware..." );

     M2DG_SCLR = M2DG_SCLR_RESET;
     udelay( 5 );
     M2DG_SCLR = 0;

     QPRINT( "Initializing shared area..." );

     memset( (void*) shared, 0, sizeof(SH772xGfxSharedArea) );

     shared->buffer_phys = virt_to_phys(&shared->buffer[0]);
     shared->magic       = SH7723GFX_SHARED_MAGIC;


     QPRINT( "Clearing interrupts..." );

     M2DG_STATUS_CLEAR = M2DG_INT_ANY;

     M2DG_INT_ENABLE   = M2DG_INT_ANY;

     QDUMP( "Ready" );

     return 0;
}

static int
sh7723_wait_idle( SH772xGfxSharedArea *shared )
{
     int ret;

     QDUMP( "Waiting....." );

     /* Does not need to be atomic. There's a lock in user space,
      * but anyhow, this is just for statistics. */
     shared->num_wait_idle++;

     ret = wait_event_interruptible_timeout( wait_idle, !shared->hw_running, 42*HZ );
     if (!ret) {
          printk( KERN_ERR "%s: TIMEOUT! (%srunning, hw %d-%d, next %d-%d - %svalid, "
                           "STATUS 0x%08x)\n",
                  __FUNCTION__,
                  shared->hw_running ? "" : "not ",
                  shared->hw_start,
                  shared->hw_end,
                  shared->next_start,
                  shared->next_end,
                  shared->next_valid ? "" : "not ",
                  M2DG_STATUS & M2DG_INT_ANY );
     }

     QDUMP( "........done" );

     return (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
}

static int
sh7723_wait_next( SH772xGfxSharedArea *shared )
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
                           "STATUS 0x%08x)\n",
                  __FUNCTION__,
                  shared->hw_running ? "" : "not ",
                  shared->hw_start,
                  shared->hw_end,
                  shared->next_start,
                  shared->next_end,
                  shared->next_valid ? "" : "not ",
                  M2DG_STATUS & M2DG_INT_ANY );
     }

     QDUMP( "........done" );

     return (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
}

/**********************************************************************************************************************/

static irqreturn_t
sh7723_beu_irq( int irq, void *ctx )
{
     BEVTR = 0;

     /* Nothing here so far. But Vsync could be added. */

     return IRQ_HANDLED;
}

static irqreturn_t
sh7723_tdg_irq( int irq, void *ctx )
{
     SH772xGfxSharedArea *shared = ctx;
     u32                  status = M2DG_STATUS & M2DG_INT_ANY;

     if (! (status & M2DG_INT_ANY)) {
#ifndef SH7723GFX_IRQ_POLLER
          printk( KERN_WARNING "%s: bogus interrupt, STATUS 0x%08x!\n", __FUNCTION__, status );
#endif
          return IRQ_NONE;
     }

//     if (status & ~0x100)
          QDUMP( "-Interrupt" );

     if (status & M2DG_INT_ERROR)
          printk( KERN_ERR "%s: error! STATUS 0x%08x!\n", __FUNCTION__, status );

     shared->num_interrupts++;

     /* Clear the interrupt. */
     M2DG_STATUS_CLEAR = status;

     if (status & (M2DG_INT_TRAP | M2DG_INT_ERROR)) {
          if (!shared->hw_running)
               printk( KERN_WARNING "%s: huh, hw running? STATUS 0x%08x!\n", __FUNCTION__, status );

          if (status & M2DG_INT_ERROR) {
               printk( KERN_ERR "%s: ERROR! (%srunning, hw %d-%d, next %d-%d - %svalid, "
                                "STATUS 0x%08x)\n",
                       __FUNCTION__,
                       shared->hw_running ? "" : "not ",
                       shared->hw_start,
                       shared->hw_end,
                       shared->next_start,
                       shared->next_end,
                       shared->next_valid ? "" : "not ",
                       status );

               M2DG_SCLR = M2DG_SCLR_RESET;
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

               M2DG_DLSAR = shared->buffer_phys + shared->hw_start*4;
               M2DG_SCLR  = M2DG_SCLR_START;

               wake_up_all( &wait_next );
          }
          else {
               shared->num_idle++;

               QDUMP( " '-> Idle." );

//check if needed
//               BEM_PE_CACHE = 1;

               shared->hw_running = 0;

               wake_up_all( &wait_next );
               wake_up_all( &wait_idle );
          }

          shared->num_done++;
     }

     return IRQ_HANDLED;
}

#ifdef SH7723GFX_IRQ_POLLER
static int
sh7723_tdg_irq_poller( void *arg )
{
     daemonize( "%s", __FUNCTION__ );

     sigfillset( &current->blocked );

     while (!stop_poller) {
          set_current_state( TASK_UNINTERRUPTIBLE );
          schedule_timeout( 1 );

          sh7723_tdg_irq( SH7723_TDG_IRQ, (void*) arg );
     }

     stop_poller = 0;

     return 0;
}
#endif

/**********************************************************************************************************************/

static int
sh7723gfx_ioctl( struct inode  *inode,
                 struct file   *filp,
                 unsigned int   cmd,
                 unsigned long  arg )
{
     SH772xRegister reg;

     switch (cmd) {
          case SH772xGFX_IOCTL_RESET:
               return sh7723_reset( shared );

          case SH772xGFX_IOCTL_WAIT_IDLE:
               return sh7723_wait_idle( shared );

          case SH772xGFX_IOCTL_WAIT_NEXT:
               return sh7723_wait_next( shared );

          case SH772xGFX_IOCTL_SETREG32:
               if (copy_from_user( &reg, (void*)arg, sizeof(SH772xRegister) ))
                    return -EFAULT;

               /* BEU, LCDC, VOU, 2DG */
               if ((reg.address < 0xFE930000 || reg.address > 0xFEA102D0) &&
                   (reg.address < 0xA4680000 || reg.address > 0xA468FFFF))
                    return -EACCES;

               *(volatile __u32 *) reg.address = reg.value;

               return 0;

          case SH772xGFX_IOCTL_GETREG32:
               if (copy_from_user( &reg, (void*)arg, sizeof(SH772xRegister) ))
                    return -EFAULT;

               /* BEU, LCDC, VOU */
               if ((reg.address < 0xFE930000 || reg.address > 0xFEA102D0) &&
                   (reg.address < 0xA4680000 || reg.address > 0xA468FFFF))
                    return -EACCES;

               reg.value = *(volatile __u32 *) reg.address;

               if (copy_to_user( (void*)arg, &reg, sizeof(SH772xRegister) ))
                    return -EFAULT;

               return 0;
     }

     return -ENOSYS;
}

static int
sh7723gfx_mmap( struct file           *file,
                struct vm_area_struct *vma )
{
     unsigned int size;

     /* Just allow mapping at offset 0. */
     if (vma->vm_pgoff)
          return -EINVAL;

     /* Check size of requested mapping. */
     size = vma->vm_end - vma->vm_start;
     if (size != PAGE_ALIGN(sizeof(SH772xGfxSharedArea)))
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

static struct file_operations sh7723gfx_fops = {
     ioctl:    sh7723gfx_ioctl,
     mmap:     sh7723gfx_mmap
};

static struct miscdevice sh7723gfx_miscdev = {
     minor:    196,           // 7*7*2*2
     name:     "sh772x_gfx",
     fops:     &sh7723gfx_fops
};

/**********************************************************************************************************************/

int
sh7723_init( void )
{
#ifndef SHARED_AREA_PHYS
     int i;
#endif
     int ret;

     /* Register the SH7723 graphics device. */
     ret = misc_register( &sh7723gfx_miscdev );
     if (ret < 0) {
          printk( KERN_ERR "%s: misc_register() for minor %d failed! (error %d)\n",
                  __FUNCTION__, sh7723gfx_miscdev.minor, ret );
          return ret;
     }

     /* Allocate and initialize the shared area. */
#ifdef SHARED_AREA_PHYS
#if SHARED_AREA_SIZE < PAGE_ALIGN(sizeof(SH772xGfxSharedArea))
#error SHARED_AREA_SIZE < PAGE_ALIGN(sizeof(SH772xGfxSharedArea))!
#endif
     shared = ioremap( SHARED_AREA_PHYS, PAGE_ALIGN(sizeof(SH772xGfxSharedArea)) );
#else
     shared_order = get_order(sizeof(SH772xGfxSharedArea));
     shared_page  = alloc_pages( GFP_DMA | GFP_KERNEL, shared_order );
     shared       = ioremap( virt_to_phys( page_address(shared_page) ),
                             PAGE_ALIGN(sizeof(SH772xGfxSharedArea)) );

     for (i=0; i<1<<shared_order; i++)
          SetPageReserved( shared_page + i );
#endif

     printk( KERN_INFO "sh7723gfx: shared area (order %d) at %p [%lx] using %d bytes\n",
             shared_order, shared, virt_to_phys(shared), sizeof(SH772xGfxSharedArea) );

     /* Register the BEU interrupt handler. */
     ret = request_irq( SH7723_BEU_IRQ, sh7723_beu_irq, IRQF_DISABLED, "BEU", (void*) shared );
     if (ret) {
          printk( KERN_ERR "%s: request_irq() for BEU interrupt %d failed! (error %d)\n",
                  __FUNCTION__, SH7723_BEU_IRQ, ret );
          goto error_beu;
     }

#ifdef SH7723GFX_IRQ_POLLER
     kernel_thread( sh7723_tdg_irq_poller, (void*) shared, CLONE_KERNEL );
#else
     /* Register the TDG interrupt handler. */
     ret = request_irq( SH7723_TDG_IRQ, sh7723_tdg_irq, IRQF_DISABLED, "TDG", (void*) shared );
     if (ret) {
          printk( KERN_ERR "%s: request_irq() for TDG interrupt %d failed! (error %d)\n",
                  __FUNCTION__, SH7723_TDG_IRQ, ret );
          goto error_tdg;
     }
#endif

     sh7723_reset( shared );

     return 0;


error_tdg:
     free_irq( SH7723_BEU_IRQ, (void*) shared );

error_beu:
#ifndef SHARED_AREA_PHYS
     for (i=0; i<1<<shared_order; i++)
          ClearPageReserved( shared_page + i );

     __free_pages( shared_page, shared_order );
#endif

     misc_deregister( &sh7723gfx_miscdev );

     return ret;
}

/**********************************************************************************************************************/

void
sh7723_exit( void )
{
#ifndef SHARED_AREA_PHYS
     int i;
#endif


#ifdef SH7723GFX_IRQ_POLLER
     stop_poller = 1;

     while (stop_poller) {
          set_current_state( TASK_UNINTERRUPTIBLE );
          schedule_timeout( 1 );
     }
#else
     free_irq( SH7723_TDG_IRQ, (void*) shared );
#endif

     free_irq( SH7723_BEU_IRQ, (void*) shared );

     misc_deregister( &sh7723gfx_miscdev );


#ifndef SHARED_AREA_PHYS
     for (i=0; i<1<<shared_order; i++)
          ClearPageReserved( shared_page + i );

     __free_pages( shared_page, shared_order );
#endif
}

