/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __SAVAGE3D_H__
#define __SAVAGE3D_H__

#include <core/gfxcard.h>

#include "savage.h"
#include "mmio.h"


typedef struct {
     SavageDeviceData s;

     /* state validation */
     int v_gbd;      /* destination */
     int v_pbd;      /* source */
     int v_color;    /* opaque fill color */

     /* saved values */
     __u32 Cmd_Src_Transparent;
     __u32 src_colorkey;
} Savage3DDeviceData;

typedef struct {
     SavageDriverData s;
} Savage3DDriverData;


void
savage3d_get_info( GraphicsDevice     *device,
                   GraphicsDriverInfo *info );

DFBResult
savage3d_init_driver( GraphicsDevice      *device,
                      GraphicsDeviceFuncs *funcs,
                      void                *driver_data );

DFBResult
savage3d_init_device( GraphicsDevice     *device,
                      GraphicsDeviceInfo *device_info,
                      void               *driver_data,
                      void               *device_data );

void
savage3d_close_device( GraphicsDevice *device,
                       void           *driver_data,
                       void           *device_data );

void
savage3d_close_driver( GraphicsDevice *device,
                       void           *driver_data );


#define FIFOSTATUS      0x48C00
#define TILEDAPERTURE0	0x48C40
#define TILEDAPERTURE1	0x48C44
#define TILEDAPERTURE2	0x48C48
#define TILEDAPERTURE3	0x48C4C
#define TILEDAPERTURE4	0x48C50
#define TILEDAPERTURE5	0x48C54


/* Wait for fifo space */
static inline void
savage3D_waitfifo(Savage3DDriverData *sdrv, Savage3DDeviceData *sdev, int space)
{
     uint32         slots = MAXFIFO - space;
     volatile __u8 *mmio  = sdrv->s.mmio_base;

     sdev->s.waitfifo_sum += space;
     sdev->s.waitfifo_calls++;
     
     if ((savage_in32(mmio, FIFOSTATUS) & 0x0000ffff) > slots) {
          do {
               sdev->s.fifo_waitcycles++;
          } while ((savage_in32(mmio, FIFOSTATUS) & 0x0000ffff) > slots);
     }
     else {
          sdev->s.fifo_cache_hits++;
     }
}

/* Wait for idle accelerator */
static inline void
savage3D_waitidle(Savage3DDriverData *sdrv, Savage3DDeviceData *sdev)
{
     sdev->s.waitidle_calls++;

     while ((savage_in32(sdrv->s.mmio_base, FIFOSTATUS) & 0x0008ffff) != 0x80000) {
          sdev->s.idle_waitcycles++;
     }
}

#endif
