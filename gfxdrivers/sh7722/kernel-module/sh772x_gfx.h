#ifndef __SH772X_GFX_H__
#define __SH772X_GFX_H__

#include <asm/types.h>


#define SH772xGFX_BUFFER_WORDS  0x1f000      /* Number of 32bit words in display list (ring buffer). */

#define SH7722GFX_SHARED_MAGIC  0x77220001   /* Increase if binary compatibility is broken. */
#define SH7723GFX_SHARED_MAGIC  0x77230001   /* Increase if binary compatibility is broken. */

#define SH7722GFX_JPEG_RELOAD_SIZE       (64 * 1024)
#define SH7722GFX_JPEG_LINEBUFFER_PITCH  (2560)
#define SH7722GFX_JPEG_LINEBUFFER_HEIGHT (16)
#define SH7722GFX_JPEG_LINEBUFFER_SIZE   (SH7722GFX_JPEG_LINEBUFFER_PITCH * SH7722GFX_JPEG_LINEBUFFER_HEIGHT * 2)
#define SH7722GFX_JPEG_LINEBUFFER_SIZE_Y (SH7722GFX_JPEG_LINEBUFFER_PITCH * SH7722GFX_JPEG_LINEBUFFER_HEIGHT)
#define SH7722GFX_JPEG_SIZE              (SH7722GFX_JPEG_LINEBUFFER_SIZE * 2 + SH7722GFX_JPEG_RELOAD_SIZE * 2)


typedef volatile struct {
     u32            buffer[SH772xGFX_BUFFER_WORDS];


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
     unsigned long  jpeg_phys;

     u32            magic;
} SH772xGfxSharedArea;


typedef struct {
     u32            address;  /* in */
     u32            value;    /* in/out */
} SH772xRegister;


typedef enum {
     SH7722_JPEG_START,
     SH7722_JPEG_RUN,
     SH7722_JPEG_END
} SH7722JPEGState;

typedef enum {
     SH7722_JPEG_FLAG_RELOAD  = 0x00000001,  /* enable reload mode */
     SH7722_JPEG_FLAG_CONVERT = 0x00000002,  /* enable conversion through VEU */
     SH7722_JPEG_FLAG_ENCODE  = 0x00000004   /* set encoding mode */
} SH7722JPEGFlags;

typedef struct {
     SH7722JPEGState state;   /* starting, running or ended (done/error) */
     SH7722JPEGFlags flags;   /* control decoding options */

     u32             buffers; /* input = loaded buffers, output = buffers to reload */
     u32             error;   /* valid in END state, non-zero means error */

     unsigned long   phys;    /* needed in case of scaling, prevents rounding errors */
     int             height;
     int             inputheight;
} SH7722JPEG;


/* Just initialization and synchronization.
 * Hardware is started from user space via MMIO to DMA registers. */
#define SH772xGFX_IOCTL_RESET      _IO( 'G', 0 )
#define SH772xGFX_IOCTL_WAIT_IDLE  _IO( 'G', 1 )
#define SH772xGFX_IOCTL_WAIT_NEXT  _IO( 'G', 2 )

/* JPEG processing, requires programming from user space. */
#define SH7722GFX_IOCTL_WAIT_JPEG   _IO  ( 'J', 0 )
#define SH7722GFX_IOCTL_RUN_JPEG    _IOWR( 'J', 1, SH7722JPEG )
#define SH7722GFX_IOCTL_LOCK_JPEG   _IO  ( 'J', 2 )
#define SH7722GFX_IOCTL_UNLOCK_JPEG _IO  ( 'J', 3 )


/* Register access limited to BEU, LCDC, VOU and JPU. */
#define SH772xGFX_IOCTL_SETREG32   _IOW( 'g', 0, SH772xRegister )
#define SH772xGFX_IOCTL_GETREG32   _IOR( 'g', 1, SH772xRegister )


#endif

