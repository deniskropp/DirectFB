/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>

#include "neomagic.h"

/* for fifo/performance monitoring */
//unsigned int neo_fifo_space = 0;

unsigned int neo_waitfifo_sum    = 0;
unsigned int neo_waitfifo_calls  = 0;
unsigned int neo_fifo_waitcycles = 0;
unsigned int neo_idle_waitcycles = 0;
unsigned int neo_fifo_cache_hits = 0;


volatile __u8 *mmio_base = NULL;
GfxCard       *neo = NULL;


/* exported symbols */

int driver_probe( int fd, GfxCard *card )
{
  switch (card->fix.accel)
    {
    case 42 ... 45:
      return 1;
    }

  return 0;
}

int driver_init( int fd, GfxCard *card )
{
  mmio_base = (__u8*)mmap(NULL, card->fix.mmio_len, PROT_READ | PROT_WRITE,
			  MAP_SHARED, fd, card->fix.smem_len);

  if (mmio_base == MAP_FAILED)
    {
      PERRORMSG("DirectFB/NeoMagic: Unable to map mmio region!\n");
      return DFB_IO;
    }

  sprintf( card->info.driver_name, "NeoMagic" );
  sprintf( card->info.driver_vendor, "convergence integrated media GmbH" );

  card->info.driver_version.major = 0;
  card->info.driver_version.minor = 0;

  /* use polling for syncing, artefacts occur otherwise */
  config->pollvsync_after = 1;

  neo = card;

  switch (card->fix.accel)
    {
    /* no support for other NeoMagic cards yet */
    case 42:        // NM2200
    case 43:        // NM2230
    case 44:        // NM2360
    case 45:        // NM2380
      return neo2200_init (card);
    }
     
  return DFB_BUG;
}

void driver_init_layers()
{
}

void driver_deinit()
{
     DEBUGMSG( "DirectFB/NEO: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/NEO:  %9d neo_waitfifo calls\n",
               neo_waitfifo_calls );
     DEBUGMSG( "DirectFB/NEO:  %9d register writes (neo_waitfifo sum)\n",
               neo_waitfifo_sum );
     DEBUGMSG( "DirectFB/NEO:  %9d FIFO wait cycles (depends on CPU)\n",
               neo_fifo_waitcycles );
     DEBUGMSG( "DirectFB/NEO:  %9d IDLE wait cycles (depends on CPU)\n",
               neo_idle_waitcycles );
     DEBUGMSG( "DirectFB/NEO:  %9d FIFO space cache hits(depends on CPU)\n",
               neo_fifo_cache_hits );
     DEBUGMSG( "DirectFB/NEO: Conclusion:\n" );
     DEBUGMSG( "DirectFB/NEO:  Average register writes/neo_waitfifo"
               "call:%.2f\n",
               neo_waitfifo_sum/(float)(neo_waitfifo_calls) );
     DEBUGMSG( "DirectFB/NEO:  Average wait cycles/neo_waitfifo call:"
               " %.2f\n",
               neo_fifo_waitcycles/(float)(neo_waitfifo_calls) );
     DEBUGMSG( "DirectFB/NEO:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * neo_fifo_cache_hits/
               (float)(neo_waitfifo_calls)) );

     munmap( (void*)mmio_base, neo->fix.mmio_len);
}

