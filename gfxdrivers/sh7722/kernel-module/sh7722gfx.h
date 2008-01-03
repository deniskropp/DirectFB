#ifndef __SH7722GFX_H__
#define __SH7722GFX_H__

#include <asm/types.h>


#define SH7722GFX_BUFFER_WORDS  0x1f000      /* Number of 32bit words in display list (ring buffer). */
#define SH7722GFX_SHARED_MAGIC  0x77220001   /* Increase if binary compatibility is broken. */


typedef volatile struct {
     u32            buffer[SH7722GFX_BUFFER_WORDS];


     int            hw_start;
     int            hw_end;

     int            hw_running;


     int            next_start;
     int            next_end;

     int            next_valid;


     unsigned long  buffer_phys;

     unsigned int   num_words;
     unsigned int   num_starts;
     unsigned int   num_done;
     unsigned int   num_interrupts;
     unsigned int   num_wait_idle;
     unsigned int   num_wait_next;
     unsigned int   num_idle;

     u32            jpeg_ints;

     u32            magic;
} SH7722GfxSharedArea;


typedef struct {
     u32            address;  /* in */
     u32            value;    /* in/out */
} SH7722Register;


typedef enum {
     SH7722_JPEG_START,
     SH7722_JPEG_RUN,
     SH7722_JPEG_END
} SH7722JPEGState;

typedef struct {
     SH7722JPEGState state;

     u32             buffers; /* input = loaded buffers, output = buffers to reload */
     u32             error;   /* valid in END state, non-zero means error */
} SH7722JPEG;


/* Just initialization and synchronization.
 * Hardware is started from user space via MMIO to DMA registers. */
#define SH7722GFX_IOCTL_RESET      _IO( 'G', 0 )
#define SH7722GFX_IOCTL_WAIT_IDLE  _IO( 'G', 1 )
#define SH7722GFX_IOCTL_WAIT_NEXT  _IO( 'G', 2 )

/* JPEG processing, requires programming from user space. */
#define SH7722GFX_IOCTL_WAIT_JPEG  _IO  ( 'J', 0 )
#define SH7722GFX_IOCTL_RUN_JPEG   _IOWR( 'J', 1, SH7722JPEG )


/* Register access limited to BEU, LCDC, VOU and JPU. */
#define SH7722GFX_IOCTL_SETREG32   _IOW( 'g', 0, SH7722Register )
#define SH7722GFX_IOCTL_GETREG32   _IOR( 'g', 1, SH7722Register )


#endif

