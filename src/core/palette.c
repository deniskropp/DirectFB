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

#include <core/surfaces.h>
#include <core/palette.h>

#include <misc/util.h>

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

     /* reset cache */
     palette->search_cache.index = -1;

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

unsigned int
dfb_palette_search( CorePalette *palette,
                    __u8         r,
                    __u8         g,
                    __u8         b,
                    __u8         a )
{
     int       i;
     int       min_diff  = 0;
     int       min_index = 0;
     DFBColor *entries   = palette->entries;

     /* check cache first */
     if (palette->search_cache.index != -1 &&
         palette->search_cache.color.a == a &&
         palette->search_cache.color.r == r &&
         palette->search_cache.color.g == g &&
         palette->search_cache.color.b == b)
          return palette->search_cache.index;

     /* find closest match */
     for (i=0; i<palette->num_entries; i++) {
          int diff = ((ABS((int) entries[i].r - (int) r) +
                       ABS((int) entries[i].g - (int) g) +
                       ABS((int) entries[i].b - (int) b)) << 3) +
                     ABS((int) entries[i].a - (int) a);

          if (diff < min_diff || i == 0) {
               min_diff  = diff;
               min_index = i;
          }
          
          if (!diff)
               break;
     }

     /* write into cache */
     palette->search_cache.color.a = a;
     palette->search_cache.color.r = r;
     palette->search_cache.color.g = g;
     palette->search_cache.color.b = b;
     
     palette->search_cache.index = min_index;

     /* return best match */
     return min_index;
}

void
dfb_palette_update( CoreSurface *surface, CorePalette *palette )
{
     /* reset cache */
     palette->search_cache.index = -1;
     
     /* post message about palette update */
     dfb_surface_notify_listeners( surface, CSNF_PALETTE );
}

