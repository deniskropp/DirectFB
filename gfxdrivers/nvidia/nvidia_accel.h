/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __NVIDIA_ACCEL_H__
#define __NVIDIA_ACCEL_H__

#include <unistd.h>

#include "nvidia.h"
#include "nvidia_regs.h"


static __inline__ void
nv_out8( volatile void *mmioaddr, u32 reg, u8 value )
{
     *((volatile u8*)(mmioaddr+reg)) = value;
}

static __inline__ void
nv_out16( volatile void *mmioaddr, u32 reg, u16 value )
{
     *((volatile u16*)(mmioaddr+reg)) = value;
}

static __inline__ void
nv_out32( volatile void *mmioaddr, u32 reg, u32 value )
{
    *((volatile u32*)(mmioaddr+reg)) = value;
}

static __inline__ u8
nv_in8( volatile void *mmioaddr, u32 reg )
{
     return *((volatile u8*)(mmioaddr+reg));
}

static __inline__ u16
nv_in16( volatile void *mmioaddr, u32 reg )
{
     return *((volatile u16*)(mmioaddr+reg));
}

static __inline__ u32
nv_in32( volatile void *mmioaddr, u32 reg )
{
    return *((volatile u32*)(mmioaddr+reg));
}

static __inline__ void
nv_outcrtc( volatile void *mmioaddr, u8 reg, u8 value )
{
     nv_out8( mmioaddr, PCIO_CRTC_INDEX, reg );
     nv_out8( mmioaddr, PCIO_CRTC_DATA, value );
}

static __inline__ u8
nv_incrtc( volatile void *mmioaddr, u8 reg )
{
     nv_out8( mmioaddr, PCIO_CRTC_INDEX, reg );
     return nv_in8( mmioaddr, PCIO_CRTC_DATA );
}

#define WAIT_MAX 10000000

static inline void
nv_waitidle( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     u32 status;
     int   waitcycles = 0;
     
     do {
          status = nv_in32( nvdrv->mmio_base, PGRAPH_STATUS );
          if (++waitcycles > WAIT_MAX) {
               D_BREAK( "Engine timed out" );
               /* avoid card crash */
               _exit(-1);
          }
     } while (status & PGRAPH_STATUS_STATE_BUSY);
      
     nvdev->idle_waitcycles += waitcycles;
}

/*
 * FIFO control
 */

static inline void
nv_waitfifo( NVidiaDriverData *nvdrv, 
             NVidiaDeviceData *nvdev, 
             unsigned int      space )
{
     volatile void *mmio       = nvdrv->mmio_base;
     int            waitcycles = 0;
     
     nvdev->waitfree_sum += (space);
     nvdev->waitfree_calls++;

     if (nvdev->fifo_free < space) {
          do {
#ifdef WORDS_BIGENDIAN
               nvdev->fifo_free = nv_in16( mmio, FIFO_FREE ) >> 2;
#else
               nvdev->fifo_free = nv_in32( mmio, FIFO_FREE ) >> 2;
#endif
               if (++waitcycles > WAIT_MAX) {
                    D_BREAK( "FIFO timed out" );
                    /* avoid card crash */
                    _exit(-1);
               }
          } while (nvdev->fifo_free < space);
          
          nvdev->free_waitcycles += waitcycles;
     } else
          nvdev->cache_hits++;
          
     nvdev->fifo_free -= space;
}

/*
 * DMA control
 */

static inline void
nv_emitdma( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     if (nvdev->dma_put != nvdev->dma_cur) {
          volatile u8 scratch;
          
          /* flush MTRR buffers */
          scratch = nv_in8( nvdrv->fb_base, 0 );
          nv_out32( nvdrv->mmio_base, DMA_PUT, nvdev->dma_cur << 2 );
          
          nvdev->dma_put = nvdev->dma_cur;
     }
}

static inline void
nv_waitdma( NVidiaDriverData *nvdrv, 
            NVidiaDeviceData *nvdev, 
            unsigned int      space )
{
     volatile void *mmio       = nvdrv->mmio_base;
     volatile void *ring       = nvdrv->dma_base;
     int            waitcycles = 0;

     nvdev->waitfree_sum += (space);
     nvdev->waitfree_calls++;

     if (nvdev->dma_free < space) {
          do {
               nvdev->dma_get = nv_in32( mmio, DMA_GET ) >> 2;
               
               if (nvdev->dma_put >= nvdev->dma_get) {
                    nvdev->dma_free = nvdev->dma_max - nvdev->dma_cur;
                         
                    if (nvdev->dma_free < space) {
                         /* rewind ring */
                         nv_out32( ring, nvdev->dma_cur << 2, 0x20000000 );

                         if (!nvdev->dma_get) {
                              if (!nvdev->dma_put) {
                                   nvdev->dma_cur = 1;
                                   nv_emitdma( nvdrv, nvdev );
                              }
                              
                              do {
                                   nvdev->dma_get = nv_in32( mmio, DMA_GET ) >> 2;
                                   if (++waitcycles > WAIT_MAX) {
                                        D_BREAK( "DMA timed out" );
                                        /* avoid card crash */
                                        _exit(-1);
                                   }
                              } while (!nvdev->dma_get);
                         }
                        
                         nvdev->dma_cur = 0;
                         nv_emitdma( nvdrv, nvdev );
                         
                         nvdev->dma_free = nvdev->dma_get - 1;
                    }
               }
               else {
                    nvdev->dma_free = nvdev->dma_get - nvdev->dma_cur - 1;
               }
                    
               if (++waitcycles > WAIT_MAX) {
                    D_BREAK( "DMA timed out" );
                    /* avoid card crash */
                    _exit(-1);
               }
          } while (nvdev->dma_free < space);

          nvdev->free_waitcycles += waitcycles;
     } else
          nvdev->cache_hits++;

     nvdev->dma_free -= space;
}

/* Begin writing into ring/fifo */
#define nv_begin( subc, start, size ) {                          \
     if (nvdev->use_dma) {                                       \
          nv_waitdma( nvdrv, nvdev, (size)+1 );                  \
          nv_out32( nvdrv->dma_base, nvdev->dma_cur << 2,        \
                   ((size) << 18) | ((subc)*0x2000 + (start)) ); \
          nvdev->cmd_ptr = nvdrv->dma_base;                      \
          nvdev->cmd_ptr += nvdev->dma_cur + 1;                  \
          nvdev->dma_cur += (size) + 1;                          \
          D_ASSERT( nvdev->dma_cur <= nvdev->dma_max );          \
     } else {                                                    \
          nv_waitfifo( nvdrv, nvdev, size );                     \
          nvdev->cmd_ptr = (nvdrv->mmio_base + FIFO_ADDRESS +    \
                            (subc)*0x2000 + (start));            \
     }                                                           \
}

/* Output to ring/register */
#define nv_outr( value )  *nvdev->cmd_ptr++ = (value)


#endif /* __NVIDIA_ACCEL_H__ */
