/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/palette.h>
#include <core/colorhash.h>

#include <misc/util.h>
#include <gfx/convert.h>

#define HASH_SIZE 2777

typedef struct {
     unsigned int  pixel;
     unsigned int  index;
     CorePalette  *palette;
} Colorhash;

typedef struct {
     Colorhash             *hash;
     unsigned int           hash_users;
     FusionSkirmish         hash_lock;
} ColorhashField;

static ColorhashField *hash_field = NULL;


DFB_CORE_PART( colorhash, 0, sizeof(ColorhashField) )


static DFBResult
dfb_colorhash_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     D_ASSERT( hash_field == NULL );

     hash_field = data_shared;

     fusion_skirmish_init( &hash_field->hash_lock );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_join( CoreDFB *core, void *data_local, void *data_shared )
{
     D_ASSERT( hash_field == NULL );

     hash_field = data_shared;

     return DFB_OK;
}

static DFBResult
dfb_colorhash_shutdown( CoreDFB *core, bool emergency )
{
     D_ASSERT( hash_field != NULL );

     fusion_skirmish_destroy( &hash_field->hash_lock );

     hash_field = NULL;

     return DFB_OK;
}

static DFBResult
dfb_colorhash_leave( CoreDFB *core, bool emergency )
{
     D_ASSERT( hash_field != NULL );

     hash_field = NULL;

     return DFB_OK;
}

static DFBResult
dfb_colorhash_suspend( CoreDFB *core )
{
     D_ASSERT( hash_field != NULL );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_resume( CoreDFB *core )
{
     D_ASSERT( hash_field != NULL );

     return DFB_OK;
}


static inline void
colorhash_lock( void )
{
     D_ASSERT( hash_field != NULL );

     fusion_skirmish_prevail( &hash_field->hash_lock );
}

static inline void
colorhash_unlock( void )
{
     D_ASSERT( hash_field != NULL );

     fusion_skirmish_dismiss( &hash_field->hash_lock );
}

void
dfb_colorhash_attach( CorePalette *palette )
{
     D_ASSERT( hash_field != NULL );

     colorhash_lock();

     if (!hash_field->hash) {
          D_ASSERT( !hash_field->hash_users );

          hash_field->hash = SHCALLOC( HASH_SIZE, sizeof (Colorhash) );
     }

     hash_field->hash_users++;

     colorhash_unlock();
}

void
dfb_colorhash_detach( CorePalette *palette )
{
     D_ASSERT( hash_field != NULL );

     colorhash_lock();

     D_ASSERT( hash_field->hash_users > 0 );
     D_ASSERT( hash_field->hash != NULL );

     hash_field->hash_users--;

     if (!hash_field->hash_users) {
          /* no more users, free allocated resources */
          SHFREE( hash_field->hash );
          hash_field->hash = NULL;
     }

     colorhash_unlock();
}

unsigned int
dfb_colorhash_lookup( CorePalette *palette,
                      __u8         r,
                      __u8         g,
                      __u8         b,
                      __u8         a )
{
     Colorhash    *hash;
     unsigned int  pixel = PIXEL_ARGB(a, r, g, b);
     unsigned int  index = (pixel ^ (unsigned int) palette) % HASH_SIZE;

     D_ASSERT( hash_field != NULL );
     D_ASSERT( hash_field->hash != NULL );

     colorhash_lock();

     hash = hash_field->hash;

     /* try a lookup in the hash table */
     if (hash[index].palette == palette && hash[index].pixel == pixel) {
          /* set the return value */
          index = hash[index].index;
     } else { /* look for the closest match */
          DFBColor *entries = palette->entries;
          int min_diff = 0;
          unsigned int i, min_index = 0;

          for (i = 0; i < palette->num_entries; i++) {
               int diff;

               if (a) {
                    int r_diff = (int) entries[i].r - (int) r;
                    int g_diff = (int) entries[i].g - (int) g;
                    int b_diff = (int) entries[i].b - (int) b;
                    int a_diff = (int) entries[i].a - (int) a;

                    diff = (r_diff * r_diff + g_diff * g_diff +
                            b_diff * b_diff + ((a_diff * a_diff) >> 6));
               }
               else
                    diff = entries[i].a;


               if (i == 0 || diff < min_diff) {
                    min_diff = diff;
                    min_index = i;
               }

               if (!diff)
                    break;
          }

          /* store the matching entry in the hash table */
          hash[index].pixel   = pixel;
          hash[index].index   = min_index;
          hash[index].palette = palette;

          /* set the return value */
          index = min_index;
     }

     colorhash_unlock();

     return index;
}

void
dfb_colorhash_invalidate( CorePalette *palette )
{
     Colorhash    *hash;
     unsigned int  index = HASH_SIZE - 1;

     D_ASSERT( hash_field != NULL );
     D_ASSERT( hash_field->hash != NULL );

     hash = hash_field->hash;

     colorhash_lock();

     /* invalidate all entries owned by this palette */
     do {
          if (hash[index].palette == palette)
               hash[index].palette = NULL;
     } while (index--);

     colorhash_unlock();
}

