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

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

#include "core/coredefs.h"

#include "cpu_accel.h"

#ifdef ARCH_X86
static __u32 arch_accel (void)
{
  __u32 eax, ebx, ecx, edx;
  int AMD;
  __u32 caps;

#ifndef PIC
#define cpuid(op,eax,ebx,ecx,edx)       \
    asm ("cpuid"                        \
         : "=a" (eax),                  \
           "=b" (ebx),                  \
           "=c" (ecx),                  \
           "=d" (edx)                   \
         : "a" (op)                     \
         : "cc")
#else   /* PIC version : save ebx */
#define cpuid(op,eax,ebx,ecx,edx)       \
    asm ("pushl %%ebx\n\t"              \
         "cpuid\n\t"                    \
         "movl %%ebx,%1\n\t"            \
         "popl %%ebx"                   \
         : "=a" (eax),                  \
           "=r" (ebx),                  \
           "=c" (ecx),                  \
           "=d" (edx)                   \
         : "a" (op)                     \
         : "cc")
#endif

  asm ("pushfl\n\t"
       "pushfl\n\t"
       "popl %0\n\t"
       "movl %0,%1\n\t"
       "xorl $0x200000,%0\n\t"
       "pushl %0\n\t"
       "popfl\n\t"
       "pushfl\n\t"
       "popl %0\n\t"
       "popfl"
       : "=r" (eax),
       "=r" (ebx)
       :
       : "cc");

  if (eax == ebx)             /* no cpuid */
    return 0;

  cpuid (0x00000000, eax, ebx, ecx, edx);
  if (!eax)                   /* vendor string only */
    return 0;

  AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

  cpuid (0x00000001, eax, ebx, ecx, edx);
  if (! (edx & 0x00800000))   /* no MMX */
    return 0;

  caps = MM_ACCEL_X86_MMX;
  if (edx & 0x02000000)       /* SSE - identical to AMD MMX extensions */
    caps |= MM_ACCEL_X86_SSE | MM_ACCEL_X86_MMXEXT;

  if (edx & 0x04000000)       /* SSE2 */
    caps |= MM_ACCEL_X86_SSE2;

  cpuid (0x80000000, eax, ebx, ecx, edx);
  if (eax < 0x80000001)       /* no extended capabilities */
    return caps;

  cpuid (0x80000001, eax, ebx, ecx, edx);

  if (edx & 0x80000000)
    caps |= MM_ACCEL_X86_3DNOW;

  if (AMD && (edx & 0x00400000))      /* AMD MMX extensions */
    caps |= MM_ACCEL_X86_MMXEXT;

  return caps;
}

static jmp_buf sigill_return;

static void sigill_handler (int n) {
  DEBUGMSG ("DirectFB/misc/cpu_accel: OS doesn't support SSE instructions.\n");
  longjmp(sigill_return, 1);
}
#endif /* ARCH_X86 */

#if defined (ARCH_PPC) && defined (ENABLE_ALTIVEC)
static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void sigill_handler (int sig)
{
    if (!canjump) {
        signal (sig, SIG_DFL);
        raise (sig);
    }

    canjump = 0;
    siglongjmp (jmpbuf, 1);
}

static __u32 arch_accel (void)
{
    signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
        signal (SIGILL, SIG_DFL);
        return 0;
    }

    canjump = 1;

    asm volatile ("mtspr 256, %0\n\t"
                  "vand %%v0, %%v0, %%v0"
                  :
                  : "r" (-1));

    signal (SIGILL, SIG_DFL);
    return MM_ACCEL_PPC_ALTIVEC;
}
#endif /* ARCH_PPC */

__u32 dfb_mm_accel (void)
{
#if defined (ARCH_X86) || (defined (ARCH_PPC) && defined (ENABLE_ALTIVEC))
  static __u32 accel = ~0;

  if (accel != ~0)
       return accel;

  accel = arch_accel ();

#ifdef ARCH_X86
  /* test OS support for SSE */
  if( accel & MM_ACCEL_X86_SSE ) {
    if (setjmp(sigill_return)) {
      accel &= ~(MM_ACCEL_X86_SSE|MM_ACCEL_X86_SSE2);
    } else {
      signal (SIGILL, sigill_handler);
      __asm __volatile ("xorps %xmm0, %xmm0");
      signal (SIGILL, SIG_DFL);
    }
  }
#endif /* ARCH_X86 */

  return accel;

#else /* !ARCH_X86 && !ARCH_PPC/ENABLE_ALTIVEC */
#ifdef HAVE_MLIB
  return MM_ACCEL_MLIB;
#else
  return 0;
#endif
#endif /* !ARCH_X86 && !ARCH_PPC/ENABLE_ALTIVEC */
}

