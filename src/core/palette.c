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

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/palette.h>

static const __u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const __u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };

CorePalette *
dfb_palette_allocate( unsigned int size )
{
     CorePalette *palette;
     DFBColor    *entries;

     DFB_ASSERT( size > 0 );

     palette = shcalloc( 1, sizeof(CorePalette) );
     if (!palette)
          return NULL;

     entries = shcalloc( size, sizeof(DFBColor) );
     if (!entries) {
          shfree( palette );
          return NULL;
     }
     
     palette->num_entries = size;
     palette->entries     = entries;

     return palette;
}

void
dfb_palette_deallocate( CorePalette *palette )
{
     DFB_ASSERT( palette != NULL );

     shfree( palette->entries );
     shfree( palette );
}

void
dfb_palette_generate_rgb332_map( CorePalette *palette )
{
     int i;

     DFB_ASSERT( palette != NULL );
     
     for (i=0; i<palette->num_entries; i++) {
          palette->entries[i].a = 0xff;
          palette->entries[i].r = lookup3to8[ (i & 0xE0) >> 5 ];
          palette->entries[i].g = lookup3to8[ (i & 0x1C) >> 2 ];
          palette->entries[i].b = lookup2to8[ (i & 0x03) ];
     }
}

