/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   Fast memcpy code was taken from xine (see below).

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

#include <config.h>

#include <sys/time.h>
#include <time.h>

#include <stdlib.h>
#include <string.h>

#include <dfb_types.h>

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#if defined (ARCH_PPC) || (SIZEOF_LONG == 8)
# define RUN_BENCHMARK  1
#else
# define RUN_BENCHMARK  0
#endif

#if RUN_BENCHMARK
D_DEBUG_DOMAIN( Direct_Memcpy, "Direct/Memcpy", "Direct's Memcpy Routines" );
#endif

#ifdef USE_PPCASM
#include "ppcasm_memcpy.h"
#endif


#if SIZEOF_LONG == 8

static void * generic64_memcpy( void * to, const void * from, size_t len )
{
     register u8 *d = (__u8*)to;
     register u8 *s = (__u8*)from;
     size_t       n;

     if (len >= 128) {
          unsigned long delta;

          /* Align destination to 8-byte boundary */
          delta = (unsigned long)d & 7;
          if (delta) {
               len -= 8 - delta;                 

               if ((unsigned long)d & 1) {
                    *d++ = *s++;
               }
               if ((unsigned long)d & 2) {
                    *((u16*)d) = *((u16*)s);
                    d += 2; s += 2;
               }
               if ((unsigned long)d & 4) {
                    *((u32*)d) = *((u32*)s);
                    d += 4; s += 4;
               }
          }
          
          n    = len >> 6;
          len &= 63;
          
          for (; n; n--) {
               ((u64*)d)[0] = ((u64*)s)[0];
               ((u64*)d)[1] = ((u64*)s)[1];
               ((u64*)d)[2] = ((u64*)s)[2];
               ((u64*)d)[3] = ((u64*)s)[3];
               ((u64*)d)[4] = ((u64*)s)[4];
               ((u64*)d)[5] = ((u64*)s)[5];
               ((u64*)d)[6] = ((u64*)s)[6];
               ((u64*)d)[7] = ((u64*)s)[7];
               d += 64; s += 64;
          }
     }
     /*
      * Now do the tail of the block
      */
     if (len) {
          n = len >> 3;
          
          for (; n; n--) {
               *((u64*)d) = *((u64*)s);
               d += 8; s += 8;
          }
          if (len & 4) {
               *((u32*)d) = *((u32*)s);
               d += 4; s += 4;
          }
          if (len & 2)  {
               *((u16*)d) = *((u16*)s);
               d += 2; s += 2;
          }
          if (len & 1)
               *d = *s;
     }
     
     return to;
}

#endif /* SIZEOF_LONG == 8 */


typedef void* (*memcpy_func)(void *to, const void *from, size_t len);

static struct {
     char                 *name;
     char                 *desc;
     memcpy_func           function;
     unsigned long long    time;
     u32                   cpu_require;
} memcpy_method[] =
{
     { NULL, NULL, NULL, 0, 0},
     { "libc",     "libc memcpy()",             (memcpy_func) memcpy, 0, 0},
#if SIZEOF_LONG == 8
     { "generic64","Generic 64bit memcpy()",    generic64_memcpy, 0, 0},
#endif /* SIZEOF_LONG == 8 */
#ifdef USE_PPCASM
     { "ppc",      "ppcasm_memcpy()",            direct_ppcasm_memcpy, 0, 0},
#ifdef __LINUX__
     { "ppccache", "ppcasm_cacheable_memcpy()",  direct_ppcasm_cacheable_memcpy, 0, 0},
#endif /* __LINUX__ */
#endif /* USE_PPCASM */
     { NULL, NULL, NULL, 0, 0}
};


static inline unsigned long long int rdtsc()
{
     struct timeval tv;

     gettimeofday (&tv, NULL);
     return (tv.tv_sec * 1000000 + tv.tv_usec);
}


memcpy_func direct_memcpy = (memcpy_func) memcpy;

#define BUFSIZE 1024

void
direct_find_best_memcpy()
{
     /* Save library size and startup time
        on platforms without a special memcpy() implementation. */
#if RUN_BENCHMARK
     unsigned long long t;
     char *buf1, *buf2;
     int i, j, best = 0;
     u32 config_flags = 0;

     if (direct_config->memcpy) {
          for (i=1; memcpy_method[i].name; i++) {
               if (!strcmp( direct_config->memcpy, memcpy_method[i].name )) {
                    if (memcpy_method[i].cpu_require & ~config_flags)
                         break;

                    direct_memcpy = memcpy_method[i].function;

                    D_INFO( "Direct/Memcpy: Forced to use %s\n", memcpy_method[i].desc );

                    return;
               }
          }
     }

     if (!(buf1 = D_MALLOC( BUFSIZE * 500 )))
          return;

     if (!(buf2 = D_MALLOC( BUFSIZE * 500 ))) {
          D_FREE( buf1 );
          return;
     }

     D_DEBUG_AT( Direct_Memcpy, "Benchmarking memcpy methods (smaller is better):\n");

     /* make sure buffers are present on physical memory */
     memcpy( buf1, buf2, BUFSIZE * 500 );
     memcpy( buf2, buf1, BUFSIZE * 500 );

     for (i=1; memcpy_method[i].name; i++) {
          if (memcpy_method[i].cpu_require & ~config_flags)
               continue;

          t = rdtsc();

          for (j=0; j<500; j++)
               memcpy_method[i].function( buf1 + j*BUFSIZE, buf2 + j*BUFSIZE, BUFSIZE );

          t = rdtsc() - t;
          memcpy_method[i].time = t;

          D_DEBUG_AT( Direct_Memcpy, "\t%-10s  %20lld\n", memcpy_method[i].name, t );

          if (best == 0 || t < memcpy_method[best].time)
               best = i;
     }

     if (best) {
          direct_memcpy = memcpy_method[best].function;

          D_INFO( "Direct/Memcpy: Using %s\n", memcpy_method[best].desc );
     }

     D_FREE( buf1 );
     D_FREE( buf2 );
#endif
}

void
direct_print_memcpy_routines()
{
     int i;
     u32 config_flags = 0;

     direct_log_printf( NULL, "\nPossible values for memcpy option are:\n\n" );

     for (i=1; memcpy_method[i].name; i++) {
          bool unsupported = (memcpy_method[i].cpu_require & ~config_flags);

          direct_log_printf( NULL, "  %-10s  %-27s  %s\n", memcpy_method[i].name,
                             memcpy_method[i].desc, unsupported ? "" : "supported" );
     }

     direct_log_printf( NULL, "\n" );
}

