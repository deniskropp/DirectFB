/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
	      Michel Dänzer <michel@daenzer.net>.

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


#ifndef ___RADEON_MMIO_H__
#define ___RADEON_MMIO_H__

#include <dfb_types.h>

#include "radeon.h"

static inline void
radeon_out32( volatile __u8 *mmioaddr, __u32 reg, __u32 value )
{
#ifdef __powerpc__
    asm volatile( "stwbrx %0,%1,%2;eieio" : : "r"( value ), "b"( reg ),
		  "r"( mmioaddr ) : "memory" );
#else
    *( ( volatile __u32* )( mmioaddr+reg ) ) = value;
#endif
}

static inline __u32
radeon_in32( volatile __u8 *mmioaddr, __u32 reg )
{
#ifdef __powerpc__
    __u32 value;

    asm volatile( "lwbrx %0,%1,%2;eieio" : "=r"( value ) : "b"( reg ),
		  "r"( mmioaddr ) );

    return value;
#else
    return *( ( volatile __u32* )( mmioaddr+reg ) );
#endif
}

static inline void radeon_waitidle( RADEONDriverData *adrv,
                                    RADEONDeviceData *adev )
{
    int timeout = 1000000;

    while (timeout--) {
	if ( ( radeon_in32( adrv->mmio_base, RBBM_STATUS ) & RBBM_FIFOCNT_MASK) == 64 )
	    break;

	adev->idle_waitcycles++;
    }

    timeout = 1000000;

    while ( timeout-- ) {
	if ( ( radeon_in32( adrv->mmio_base, RBBM_STATUS ) & RBBM_ACTIVE ) == ENGINE_IDLE )
	    break;

	adev->idle_waitcycles++;
    }

    adev->fifo_space = 60;
}

static inline void radeon_waitfifo( RADEONDriverData *adrv,
                                    RADEONDeviceData *adev,
                                    unsigned int requested_fifo_space )
{
    int timeout = 1000000;

    adev->waitfifo_sum += requested_fifo_space;
    adev->waitfifo_calls++;

    if ( adev->fifo_space < requested_fifo_space ) {
	while ( timeout-- ) {
	    adev->fifo_waitcycles++;

	    adev->fifo_space = radeon_in32( adrv->mmio_base, RBBM_STATUS) & RBBM_FIFOCNT_MASK;
	    if (adev->fifo_space >= requested_fifo_space)
		break;
	}
    } else {
	adev->fifo_cache_hits++;
    }
    adev->fifo_space -= requested_fifo_space;
}

#endif
