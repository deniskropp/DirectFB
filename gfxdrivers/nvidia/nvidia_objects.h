/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

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

#ifndef __NVIDIA_OBJECTS_H__
#define __NVIDIA_OBJECTS_H__

#include "nvidia.h"
#include "nvidia_accel.h"

/* Engine */
#define ENGINE_SW        0
#define ENGINE_GRAPHICS  1
#define ENGINE_DVD       2


static __inline__ __u32
nv_hashkey( __u32 obj )
{
     return ((obj >>  0) & 0x000001FF) ^
            ((obj >>  9) & 0x000001FF) ^
            ((obj >> 18) & 0x000001FF) ^
            ((obj >> 27) & 0x000001FF) ^ (0 << 5); /* channel 0 */
}


/* DMA flags */
#define DMA_FLAG_PAGE_TABLE        (1 << 12) /* valid */
#define DMA_FLAG_PAGE_ENTRY_NONLIN (0 << 13)
#define DMA_FLAG_PAGE_ENTRY_LINEAR (1 << 13) 
#define DMA_FLAG_ACCESS_RDWR       (0 << 14)
#define DMA_FLAG_ACCESS_RDONLY     (1 << 14) 
#define DMA_FLAG_ACCESS_WRONLY     (2 << 14)
#define DMA_FLAG_TARGET_NVM        (0 << 16) 
#define DMA_FLAG_TARGET_NVM_TILED  (1 << 16)
#define DMA_FLAG_TARGET_PCI        (2 << 16)
#define DMA_FLAG_TARGET_AGP        (3 << 16)

/* DMA frame access */
#define DMA_FRAME_UNKNOWN_FLAG     (1 << 0)
#define DMA_FRAME_ACCESS_RDONLY    (0 << 1)
#define DMA_FRAME_ACCESS_RDWR      (1 << 1)

static inline void
nv_store_dma( NVidiaDriverData *nvdrv, __u32 obj,
              __u32 addr, __u32 class, __u32 flags,
              __u32 size, __u32 frame, __u32 access )
{
     volatile void *mmio = nvdrv->mmio_base;
     __u32          key  = nv_hashkey( obj );
     __u32          ctx  = addr | (ENGINE_SW << 16) | (1 << 31);
     
     /* NV_PRAMIN_RAMRO_0 */
     nv_out32( mmio, PRAMIN + (addr << 4) +  0, class | flags );
     nv_out32( mmio, PRAMIN + (addr << 4) +  4, size - 1 );
     nv_out32( mmio, PRAMIN + (addr << 4) +  8, (frame & 0xFFFFF000) | access );
     nv_out32( mmio, PRAMIN + (addr << 4) + 12, (frame & 0xFFFFF000) | access );

     /* store object id and context */
     nv_out32( mmio, PRAMHT + (key << 3) + 0, obj );
     nv_out32( mmio, PRAMHT + (key << 3) + 4, ctx );
}


/* Context flags */
#define CTX_FLAG_CHROMA_KEY                 (1 << 12)
#define CTX_FLAG_USER_CLIP                  (1 << 13)
#define CTX_FLAG_SWIZZLE                    (1 << 14)
#define CTX_FLAG_PATCH_COPY                 (0 << 15)
#define CTX_FLAG_PATCH_ROP                  (1 << 15)
#define CTX_FLAG_PATCH_BLEND                (2 << 15)
#define CTX_FLAG_PATCH_SRCCOPY              (3 << 15)
#define CTX_FLAG_PATCH_COLOR_MULTIPLY       (4 << 15)
#define CTX_FLAG_PATCH_BLEND_PREMULTIPLIED  (5 << 15)
#define CTX_FLAG_SYNCHRONIZE                (1 << 18)
#define CTX_FLAG_ENDIAN_LITTLE              (0 << 19)
#define CTX_FLAG_ENDIAN_BIG                 (1 << 19)
#define CTX_FLAG_CONVERSION_COMPAT          (0 << 20)
#define CTX_FLAG_CONVERSION_DITHER          (1 << 20)
#define CTX_FLAG_CONVERSION_TRUNC           (2 << 20)
#define CTX_FLAG_CONVERSION_SUB_TRUNC       (3 << 20)
#define CTX_FLAG_SINGLE_STEP                (1 << 23)
#define CTX_FLAG_PATCH                      (1 << 24) /* valid */
#define CTX_FLAG_CTX_SURFACE0               (1 << 25) /* valid */
#define CTX_FLAG_CTX_SURFACE1               (1 << 26) /* valid */
#define CTX_FLAG_CTX_PATTERN                (1 << 27) /* valid */
#define CTX_FLAG_CTX_ROP                    (1 << 28) /* valid */
#define CTX_FLAG_CTX_BETA1                  (1 << 29) /* valid */
#define CTX_FLAG_CTX_BETA4                  (1 << 30) /* valid */


static inline void
nv_store_object( NVidiaDriverData *nvdrv, 
                 __u32 obj,   __u32 addr, 
                 __u32 class, __u32 flags,
                 __u32 dma0,  __u32 dma1 )
{
     volatile void *mmio = nvdrv->mmio_base;
     __u32          key  = nv_hashkey( obj );
     __u32          ctx  = addr | (ENGINE_GRAPHICS << 16) | (1 << 31);
     
     /* set the endian flag here, for simplicity */
#ifdef WORDS_BIGENDIAN
     flags |= CTX_FLAG_ENDIAN_BIG;
#endif
     /* NV_PRAMIN_CTX_0 */
     nv_out32( mmio, PRAMIN + (addr << 4) +  0, class | flags );
     /* NV_PRAMIN_CTX_1 */
     nv_out32( mmio, PRAMIN + (addr << 4) +  4, 0x00000000 ); /* color */
     /* NV_PRAMIN_CTX_2 */
     nv_out32( mmio, PRAMIN + (addr << 4) +  8, dma0 | (dma1 << 16) );
     /* NV_PRAMIN_CTX_3 */
     nv_out32( mmio, PRAMIN + (addr << 4) + 12, 0x00000000 ); /* traps */

     /* store object id and context */
     nv_out32( mmio, PRAMHT + (key << 3) + 0, obj );
     nv_out32( mmio, PRAMHT + (key << 3) + 4, ctx );
}


static inline void
nv_assign_object( NVidiaDriverData *nvdrv,
                  NVidiaDeviceData *nvdev,
                  int               subc,
                  __u32             object,
                  bool              reset )
{
     if (reset || nvdev->subchannel_object[subc] != object) {
          nv_begin( subc, SET_OBJECT, 1 );
          nv_outr( object );

          nvdev->subchannel_object[subc] = object;
     }
}

#endif /* __NVIDIA_OBJECTS_H__ */
