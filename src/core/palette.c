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
#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/colorhash.h>

#include <misc/util.h>

static const __u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const __u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };

static const React dfb_palette_globals[] = {
/* 0 */   _dfb_surface_palette_listener,
          NULL
};

static void palette_destructor( FusionObject *object, bool zombie )
{
     CorePaletteNotification  notification;
     CorePalette             *palette = (CorePalette*) object;

     DEBUGMSG("DirectFB/core/palette: destroying %p (%d)%s\n", palette,
              palette->num_entries, zombie ? " (ZOMBIE)" : "");

     notification.flags   = CPNF_DESTROY;
     notification.palette = palette;

     fusion_object_dispatch( &palette->object, &notification,
                             dfb_palette_globals );

     if (palette->hash_attached) {
          dfb_colorhash_invalidate( palette );
          dfb_colorhash_detach( palette );
     }

     shfree( palette->entries );

     fusion_object_destroy( object );
}

/** public **/

FusionObjectPool *
dfb_palette_pool_create()
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Palette Pool",
                                       sizeof(CorePalette),
                                       sizeof(CorePaletteNotification),
                                       palette_destructor );

     return pool;
}

DFBResult
dfb_palette_create( unsigned int size, CorePalette **ret_palette )
{
     CorePalette *palette;

     DFB_ASSERT( ret_palette );

     palette = (CorePalette*) fusion_object_create( dfb_gfxcard_palette_pool() );
     if (!palette)
          return DFB_FUSION;

     if (size) {
          palette->entries = shcalloc( size, sizeof(DFBColor) );
          if (!palette->entries) {
               fusion_object_destroy( &palette->object );
               return DFB_NOSYSTEMMEMORY;
          }
     }
     
     palette->num_entries = size;

     /* reset cache */
     palette->search_cache.index = -1;

     /* activate object */
     fusion_object_activate( &palette->object );
     
     /* return the new palette */
     *ret_palette = palette;

     return DFB_OK;
}

void
dfb_palette_generate_rgb332_map( CorePalette *palette )
{
     int i;

     DFB_ASSERT( palette != NULL );

     if (!palette->num_entries)
          return;

     for (i=0; i<palette->num_entries; i++) {
          palette->entries[i].a = i ? 0xff : 0x00;
          palette->entries[i].r = lookup3to8[ (i & 0xE0) >> 5 ];
          palette->entries[i].g = lookup3to8[ (i & 0x1C) >> 2 ];
          palette->entries[i].b = lookup2to8[ (i & 0x03) ];
     }

     dfb_palette_update( palette, 0, palette->num_entries - 1 );
}

void
dfb_palette_generate_rgb121_map( CorePalette *palette )
{
     int i;

     DFB_ASSERT( palette != NULL );

     if (!palette->num_entries)
          return;

     for (i=0; i<palette->num_entries; i++) {
          palette->entries[i].a = i ? 0xff : 0x00;
          palette->entries[i].r = (i & 0x8) ? 0xff : 0x00;
          palette->entries[i].g = lookup2to8[ (i & 0x6) >> 1 ];
          palette->entries[i].b = (i & 0x1) ? 0xff : 0x00;
     }

     dfb_palette_update( palette, 0, palette->num_entries - 1 );
}

unsigned int
dfb_palette_search( CorePalette *palette,
                    __u8         r,
                    __u8         g,
                    __u8         b,
                    __u8         a )
{
     unsigned int index;

     /* check local cache first */
     if (palette->search_cache.index != -1 &&
         palette->search_cache.color.a == a &&
         palette->search_cache.color.r == r &&
         palette->search_cache.color.g == g &&
         palette->search_cache.color.b == b)
          return palette->search_cache.index;

     /* check the global color hash table, returns the closest match */
     if (!palette->hash_attached) {
          dfb_colorhash_attach( palette );
          palette->hash_attached = true;
     }

     index = dfb_colorhash_lookup( palette, r, g, b, a );

     /* write into local cache */
     palette->search_cache.index = index;
     palette->search_cache.color.a = a;
     palette->search_cache.color.r = r;
     palette->search_cache.color.g = g;
     palette->search_cache.color.b = b;

     return index;
}

void
dfb_palette_update( CorePalette *palette, int first, int last )
{
     CorePaletteNotification notification;

     DFB_ASSERT( palette != NULL );
     DFB_ASSERT( first >= 0 );
     DFB_ASSERT( first < palette->num_entries );
     DFB_ASSERT( last >= 0 );
     DFB_ASSERT( last < palette->num_entries );
     DFB_ASSERT( first <= last );

     notification.flags   = CPNF_ENTRIES;
     notification.palette = palette;
     notification.first   = first;
     notification.last    = last;

     /* reset cache */
     if (palette->search_cache.index >= first &&
         palette->search_cache.index <= last)
          palette->search_cache.index = -1;
     
     /* invalidate entries in colorhash */
     if (palette->hash_attached)
          dfb_colorhash_invalidate( palette );

     /* post message about palette update */
     fusion_object_dispatch( &palette->object, &notification,
                             dfb_palette_globals );
}

