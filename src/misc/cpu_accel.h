/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   cpu_accel code was taken from xine that obviously took it from mpeg2dec.

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

/*
 * cpu_accel.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CPU_ACCEL_H__
#define __CPU_ACCEL_H__

#include <asm/types.h>

                        /* CPU Acceleration */

/*
 * The type of an value that fits in an MMX register (note that long
 * long constant values MUST be suffixed by LL and unsigned long long
 * values by ULL, lest they be truncated by the compiler)
 */

/* generic accelerations */
#define MM_ACCEL_MLIB           0x00000001

/* x86 accelerations */
#define MM_ACCEL_X86_MMX        0x80000000
#define MM_ACCEL_X86_3DNOW      0x40000000
#define MM_ACCEL_X86_MMXEXT     0x20000000
#define MM_ACCEL_X86_SSE        0x10000000
#define MM_ACCEL_X86_SSE2       0x08000000
/* powerpc accelerations */
#define MM_ACCEL_PPC_ALTIVEC    0x04000000
/* x86 compat defines */
#define MM_MMX                  MM_ACCEL_X86_MMX
#define MM_3DNOW                MM_ACCEL_X86_3DNOW
#define MM_MMXEXT               MM_ACCEL_X86_MMXEXT
#define MM_SSE                  MM_ACCEL_X86_SSE
#define MM_SSE2                 MM_ACCEL_X86_SSE2

__u32 dfb_mm_accel (void);

#endif

