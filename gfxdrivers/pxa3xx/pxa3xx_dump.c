/*
   PXA3xx Graphics Controller

   (c) Copyright 2009  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

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

#ifdef PXA3XX_DEBUG_DUMP
#define DIRECT_ENABLE_DEBUG
#endif

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <asm/types.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/system.h>
#include <direct/util.h>


#include <pxa3xx-gcu.h>


D_DEBUG_DOMAIN( PXA3XX_Dump, "PXA3XX/Dump", "PXA3xx Dump" );

/**********************************************************************************************************************/

#define PXA_GCU_REG(x)        (*(volatile u32*)(pxa3xx_gcu_virt+(x)))

#define PXA_GCCR             PXA_GCU_REG(0x000)
#define PXA_GCISCR           PXA_GCU_REG(0x004)
#define PXA_GCIECR           PXA_GCU_REG(0x008)
#define PXA_GCRBBR           PXA_GCU_REG(0x020)
#define PXA_GCRBLR           PXA_GCU_REG(0x024)
#define PXA_GCRBHR           PXA_GCU_REG(0x028)
#define PXA_GCRBTR           PXA_GCU_REG(0x02C)
#define PXA_GCRBEXHR         PXA_GCU_REG(0x030)

#define PXA_GCD0BR           PXA_GCU_REG(0x060)
#define PXA_GCD0STP          PXA_GCU_REG(0x064)
#define PXA_GCD0STR          PXA_GCU_REG(0x068)

#define PXA_GCD1BR           PXA_GCU_REG(0x06C)
#define PXA_GCD1STP          PXA_GCU_REG(0x070)
#define PXA_GCD1STR          PXA_GCU_REG(0x074)

#define PXA_GCD2BR           PXA_GCU_REG(0x078)
#define PXA_GCD2STP          PXA_GCU_REG(0x07C)
#define PXA_GCD2STR          PXA_GCU_REG(0x080)

#define PXA_GCSCn_WDn(s,w)   PXA_GCU_REG(0x160 + s * 8 + w * 4)

#define PXA_GCISCR_ALL       0x000000FF

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     int                               gfx_fd;
     volatile struct pxa3xx_gcu_shared *gfx_shared;
     void                             *pxa3xx_gcu_virt;

     /* Open the drawing engine device. */
     gfx_fd = direct_try_open( "/dev/pxa3xx_gfx", "/dev/misc/pxa3xx_gfx", O_RDWR, true );
     if (gfx_fd < 0)
          return DFB_INIT;

     /* Map its shared data. */
     gfx_shared = mmap( NULL, direct_page_align( sizeof(struct pxa3xx_gcu_shared) ),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, gfx_fd, 0 );
     if (gfx_shared == MAP_FAILED) {
          D_PERROR( "PXA3XX/Dump: Could not map shared area!\n" );
          close( gfx_fd );
          return DFB_INIT;
     }

     /* Map registers. */
     pxa3xx_gcu_virt = mmap( NULL, 4096, // FIXME
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED, gfx_fd, direct_page_align( sizeof(struct pxa3xx_gcu_shared) ) );
     if (pxa3xx_gcu_virt == MAP_FAILED) {
          D_PERROR( "PXA3XX/Dump: Could not map registers!\n" );
          munmap( (void*) gfx_shared, direct_page_align( sizeof(struct pxa3xx_gcu_shared) ) );
          close( gfx_fd );
          return DFB_INIT;
     }

     /* Check the magic value. */
     if (gfx_shared->magic != PXA3XX_GCU_SHARED_MAGIC)
          D_ERROR( "PXA3XX/Dump: Magic value 0x%08x doesn't match 0x%08x!\n",
                   gfx_shared->magic, PXA3XX_GCU_SHARED_MAGIC );


     while (1) {
          int i;
          u32 base = PXA_GCRBBR;

          printf( "\e[H\e[J" );

          printf( "\n" );
          printf( "\n" );

          printf( "%s, hw %5d-%5d, next %5d-%5d, %svalid, "
                  "STATUS 0x%02x, B 0x%08x [%d], E %5d, H %5d, T %5d\n",
                  gfx_shared->hw_running ? "running" : "idle   ",
                  gfx_shared->hw_start,
                  gfx_shared->hw_end,
                  gfx_shared->next_start,
                  gfx_shared->next_end,
                  gfx_shared->next_valid ? "  " : "in",
                  PXA_GCISCR & PXA_GCISCR_ALL,
                  PXA_GCRBBR, PXA_GCRBLR,
                  (PXA_GCRBEXHR - base) / 4,
                  (PXA_GCRBHR - base) / 4,
                  (PXA_GCRBTR - base) / 4 );

          printf( "\n" );

          printf( "      %u starts, %u done, %u interrupts, %u wait_idle, %u wait_next, %u idle\n",
                  gfx_shared->num_starts, gfx_shared->num_done, gfx_shared->num_interrupts,
                  gfx_shared->num_wait_idle, gfx_shared->num_wait_next, gfx_shared->num_idle );

          printf( "      %u words, %u words/start, %u words/idle, %u starts/idle\n",
                  gfx_shared->num_words,
                  gfx_shared->num_words  / (gfx_shared->num_starts ?: 1),
                  gfx_shared->num_words  / (gfx_shared->num_idle ?: 1),
                  gfx_shared->num_starts / (gfx_shared->num_idle ?: 1) );

          printf( "\n" );

          printf( "      D2        0x%08x [%4d]\n", PXA_GCD0BR, PXA_GCD0STR );

          printf( "      SC0-7     " );

          for (i=0; i<8; i++)
               printf( "0x%08x  ", PXA_GCSCn_WDn(i,0) );

          printf( "\n" );
          printf( "\n" );

          break;
          sleep( 1 );
     }


     /* Unmap registers. */
     munmap( pxa3xx_gcu_virt, 4096 );    // FIXME

     /* Unmap shared area. */
     munmap( (void*) gfx_shared, direct_page_align( sizeof(struct pxa3xx_gcu_shared) ) );

     /* Close Drawing Engine device. */
     close( gfx_fd );

     return 0;
}

