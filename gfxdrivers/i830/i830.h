/*
   Intel i830 DirectFB graphics driver

   (c) Copyright 2005       Servision Ltd.
                            http://www.servision.net/

   All rights reserved.

   Based on i810 driver written by Antonino Daplas <adaplas@pol.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __I830_H__
#define __I830_H__

#include <asm/types.h>
#include <sys/io.h>
#include <linux/agpgart.h>

#include <core/gfxcard.h>
#include <core/layers.h>


#define RINGBUFFER_SIZE             (128 * 1024)



/* Ring buffer registers, p277, overview p19
 */
#define LP_RING     0x2030
#define HP_RING     0x2040

#define RING_TAIL      0x00
#define TAIL_ADDR           0x000FFFF8
#define I830_TAIL_MASK	    0x001FFFF8

#define RING_HEAD      0x04
#define HEAD_WRAP_COUNT     0xFFE00000
#define HEAD_WRAP_ONE       0x00200000
#define HEAD_ADDR           0x001FFFFC
#define I830_HEAD_MASK      0x001FFFFC

#define RING_START     0x08
#define START_ADDR          0x00FFFFF8
#define I830_RING_START_MASK	0xFFFFF000

#define RING_LEN       0x0C
#define RING_NR_PAGES       0x000FF000
#define I830_RING_NR_PAGES	0x001FF000
#define RING_REPORT_MASK    0x00000006
#define RING_REPORT_64K     0x00000002
#define RING_REPORT_128K    0x00000004
#define RING_NO_REPORT      0x00000000
#define RING_VALID_MASK     0x00000001
#define RING_VALID          0x00000001
#define RING_INVALID        0x00000000


/* Overlay Flip */
#define MI_OVERLAY_FLIP			(0x11<<23)
#define MI_OVERLAY_FLIP_CONTINUE	(0<<21)
#define MI_OVERLAY_FLIP_ON		(1<<21)
#define MI_OVERLAY_FLIP_OFF		(2<<21)

/* Wait for Events */
#define MI_WAIT_FOR_EVENT		(0x03<<23)
#define MI_WAIT_FOR_OVERLAY_FLIP	(1<<16)

/* Flush */
#define MI_FLUSH			(0x04<<23)
#define MI_WRITE_DIRTY_STATE		(1<<4)
#define MI_END_SCENE			(1<<3)
#define MI_INHIBIT_RENDER_CACHE_FLUSH	(1<<2)
#define MI_INVALIDATE_MAP_CACHE		(1<<0)

/* Noop */
#define MI_NOOP				0x00
#define MI_NOOP_WRITE_ID		(1<<22)
#define MI_NOOP_ID_MASK			(1<<22 - 1)


/* Instruction Parser Mode Register
 *    - p281
 *    - 2 new bits.
 */
#define INST_PM                  0x20c0	
#define AGP_SYNC_PACKET_FLUSH_ENABLE 0x20 /* reserved */
#define SYNC_PACKET_FLUSH_ENABLE     0x10
#define TWO_D_INST_DISABLE           0x08
#define THREE_D_INST_DISABLE         0x04
#define STATE_VAR_UPDATE_DISABLE     0x02
#define PAL_STIP_DISABLE             0x01

#define INST_DONE                0x2090
#define INST_PS                  0x20c4

#define MEMMODE                  0x20dc



#define I830RES_GART                1
#define I830RES_LRING_ACQ           2
#define I830RES_LRING_BIND          4
#define I830RES_OVL_ACQ             8
#define I830RES_OVL_BIND           16
#define I830RES_GART_ACQ           32
#define I830RES_MMAP               64
#define I830RES_STATE_SAVE        128

#ifndef AGP_NORMAL_MEMORY
#define AGP_NORMAL_MEMORY 0
#endif

#ifndef AGP_PHYSICAL_MEMORY
#define AGP_PHYSICAL_MEMORY 2
#endif




/*
 * OCMD - Overlay Command Register
 */
#define MIRROR_MODE             (0x3<<17)
#define MIRROR_HORIZONTAL       (0x1<<17)
#define MIRROR_VERTICAL         (0x2<<17)
#define MIRROR_BOTH             (0x3<<17)
#define OV_BYTE_ORDER           (0x3<<14)
#define UV_SWAP                 (0x1<<14)
#define Y_SWAP                  (0x2<<14)
#define Y_AND_UV_SWAP           (0x3<<14)
#define SOURCE_FORMAT           (0xf<<10)
#define RGB_888                 (0x1<<10)
#define RGB_555                 (0x2<<10)
#define RGB_565                 (0x3<<10)
#define YUV_422                 (0x8<<10)
#define YUV_411                 (0x9<<10)
#define YUV_420                 (0xc<<10)
#define YUV_422_PLANAR          (0xd<<10)
#define YUV_410                 (0xe<<10)
#define TVSYNC_FLIP_PARITY      (0x1<<9)
#define TVSYNC_FLIP_ENABLE      (0x1<<7)
#define BUF_TYPE                (0x1<<5)
#define BUF_TYPE_FRAME          (0x0<<5)
#define BUF_TYPE_FIELD          (0x1<<5)
#define TEST_MODE               (0x1<<4)
#define BUFFER_SELECT           (0x3<<2)
#define BUFFER0                 (0x0<<2)
#define BUFFER1                 (0x1<<2)
#define FIELD_SELECT            (0x1<<1)
#define FIELD0                  (0x0<<1)
#define FIELD1                  (0x1<<1)
#define OVERLAY_ENABLE          0x1

/* OCONFIG register */
#define CC_OUT_8BIT             (0x1<<3)
#define OVERLAY_PIPE_MASK       (0x1<<18)
#define OVERLAY_PIPE_A          (0x0<<18)
#define OVERLAY_PIPE_B          (0x1<<18)

/* DCLRKM register */
#define DEST_KEY_ENABLE         (0x1<<31)

/* Polyphase filter coefficients */
#define N_HORIZ_Y_TAPS          5
#define N_VERT_Y_TAPS           3
#define N_HORIZ_UV_TAPS         3
#define N_VERT_UV_TAPS          3
#define N_PHASES                17
#define MAX_TAPS                5

/* Filter cutoff frequency limits. */
#define MIN_CUTOFF_FREQ         1.0
#define MAX_CUTOFF_FREQ         3.0

typedef volatile struct {
     __u32 OBUF_0Y;
     __u32 OBUF_1Y;
     __u32 OBUF_0U;
     __u32 OBUF_0V;
     __u32 OBUF_1U;
     __u32 OBUF_1V;
     __u32 OSTRIDE;
     __u32 YRGB_VPH;
     __u32 UV_VPH;
     __u32 HORZ_PH;
     __u32 INIT_PHS;
     __u32 DWINPOS;
     __u32 DWINSZ;
     __u32 SWIDTH;
     __u32 SWIDTHSW;
     __u32 SHEIGHT;
     __u32 YRGBSCALE;
     __u32 UVSCALE;
     __u32 OCLRC0;
     __u32 OCLRC1;
     __u32 DCLRKV;
     __u32 DCLRKM;
     __u32 SCLRKVH;
     __u32 SCLRKVL;
     __u32 SCLRKEN;
     __u32 OCONFIG;
     __u32 OCMD;
     __u32 RESERVED1;           /* 0x6C */
     __u32 AWINPOS;
     __u32 AWINSZ;
     __u32 RESERVED2;           /* 0x78 */
     __u32 RESERVED3;           /* 0x7C */
     __u32 RESERVED4;           /* 0x80 */
     __u32 RESERVED5;           /* 0x84 */
     __u32 RESERVED6;           /* 0x88 */
     __u32 RESERVED7;           /* 0x8C */
     __u32 RESERVED8;           /* 0x90 */
     __u32 RESERVED9;           /* 0x94 */
     __u32 RESERVEDA;           /* 0x98 */
     __u32 RESERVEDB;           /* 0x9C */
     __u32 FASTHSCALE;               /* 0xA0 */
     __u32 UVSCALEV;            /* 0xA4 */

     __u32 RESERVEDC[(0x200 - 0xA8) / 4];         /* 0xA8 - 0x1FC */
     __u16 Y_VCOEFS[N_VERT_Y_TAPS * N_PHASES];         /* 0x200 */
     __u16 RESERVEDD[0x100 / 2 - N_VERT_Y_TAPS * N_PHASES];
     __u16 Y_HCOEFS[N_HORIZ_Y_TAPS * N_PHASES];             /* 0x300 */
     __u16 RESERVEDE[0x200 / 2 - N_HORIZ_Y_TAPS * N_PHASES];
     __u16 UV_VCOEFS[N_VERT_UV_TAPS * N_PHASES];            /* 0x500 */
     __u16 RESERVEDF[0x100 / 2 - N_VERT_UV_TAPS * N_PHASES];
     __u16 UV_HCOEFS[N_HORIZ_UV_TAPS * N_PHASES];      /* 0x600 */
     __u16 RESERVEDG[0x100 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
} I830OverlayRegs;

typedef struct {
     CoreLayerRegionConfig  config;
} I830OverlayLayerData;


typedef struct {
     unsigned int   tail_mask;

     int            size;
     int            head;
     int            tail;
     int            space;
} I830RingBuffer;

typedef struct {
     volatile void *virt;
     unsigned int   tail_mask;
     unsigned int   outring;
} I830RingBlock;


typedef struct {
     bool                  initialized;

     I830RingBuffer        lp_ring;

     bool                  overlayOn;
     I830OverlayLayerData *iovl;

     agp_info              info;
     agp_allocate          lring_mem;
     agp_allocate          ovl_mem;
     agp_bind              lring_bind;
     agp_bind              ovl_bind;

     __u32                 pattern;
     __u32                 lring1;
     __u32                 lring2;
     __u32                 lring3;
     __u32                 lring4;

     /* benchmarking */
     __u32                 waitfifo_sum;
     __u32                 waitfifo_calls;
     __u32                 idle_calls;
     __u32                 fifo_waitcycles;
     __u32                 idle_waitcycles;
     __u32                 fifo_cache_hits;
     __u32                 fifo_timeoutsum;
     __u32                 idle_timeoutsum;
} I830DeviceData;

typedef struct {
     I830DeviceData     *idev;

     I830OverlayRegs    *oregs;

     __u32               flags;
     int                 agpgart;
     agp_info            info;
     volatile __u8      *aper_base;
     volatile __u8      *lring_base;
     volatile __u8      *ovl_base;
     volatile __u8      *mmio_base;
     volatile __u8      *pattern_base;
} I830DriverData;

extern DisplayLayerFuncs i830OverlayFuncs;

void i830ovlOnOff( I830DriverData       *idrv,
                   I830DeviceData       *idev,
                   bool                  on );



#define i830_readb(mmio_base, where)                     \
        *((volatile __u8 *) (mmio_base + where))           \

#define i830_readw(mmio_base, where)                     \
       *((volatile __u16 *) (mmio_base + where))           \

#define i830_readl(mmio_base, where)                     \
       *((volatile __u32 *) (mmio_base + where))           \

#define i830_writeb(mmio_base, where, val)                              \
        *((volatile __u8 *) (mmio_base + where)) = (volatile __u8) val      \

#define i830_writew(mmio_base, where, val)                              \
        *((volatile __u16 *) (mmio_base + where)) = (volatile __u16) val    \

#define i830_writel(mmio_base, where, val)                              \
        *((volatile __u32 *) (mmio_base + where)) = (volatile __u32) val    \



DFBResult i830_wait_lp_ring( I830DriverData *idrv,
                             I830DeviceData *idev,
                             int             space );

D_DEBUG_DOMAIN( I830_Ring, "I830/Ring", "I830 Ring Buffer" );


static inline DFBResult
i830_begin_lp_ring( I830DriverData *idrv,
                    I830DeviceData *idev,
                    int             needed,
                    I830RingBlock  *ret_block )
{
     DFBResult       ret;
     I830RingBuffer *buf = &idev->lp_ring;

     D_DEBUG_AT( I830_Ring, "begin_lp_ring( %d ) <- head 0x%08x\n", needed, i830_readl( idrv->mmio_base, LP_RING + RING_HEAD ) );

     if (needed & 1)
          D_ERROR( "i830_begin_ring called with odd argument: %d\n", needed);

     needed *= 4;

     if (buf->space < needed) {
          ret = i830_wait_lp_ring( idrv, idev, needed );
          if (ret)
               return ret;
     }

     buf->space -= needed;

     ret_block->virt      = idrv->lring_base;
     ret_block->tail_mask = buf->tail_mask;
     ret_block->outring   = buf->tail;

     return DFB_OK;
}

static inline void
i830_out_ring( I830RingBlock *block,
               __u32          value )
{
     D_DEBUG_AT( I830_Ring, "out_ring( 0x%08x, 0x%08x )\n", block->outring, value );

     *(volatile __u32*)(block->virt + block->outring) = value;

     block->outring = (block->outring + 4) & block->tail_mask;
}

static inline void
i830_advance_lp_ring( I830DriverData      *idrv,
                      I830DeviceData      *idev,
                      const I830RingBlock *block )
{
     D_DEBUG_AT( I830_Ring, "advance_lp_ring( 0x%08x )\n", block->outring );

     idev->lp_ring.tail = block->outring;

     if (block->outring & 0x07)
          D_ERROR( "i830_advance_lp_ring: "
                   "outring (0x%x) isn't on a QWord boundary", block->outring );

     i830_writel( idrv->mmio_base, LP_RING + RING_TAIL, block->outring );
}

#endif /* __I830_H__ */
