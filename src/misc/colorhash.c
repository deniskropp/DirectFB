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

#include "config.h"

#include <pthread.h>

#include "core/palette.h"

#include "misc/mem.h"
#include "misc/util.h"
#include "gfx/convert.h"

#include "colorhash.h"

#define HASH_SIZE 1021

typedef struct
{
  unsigned int  pixel;
  unsigned int  index;
  CorePalette  *palette;
} Colorhash;

static Colorhash       *hash       = NULL;
static unsigned int     hash_users = 0;
static pthread_mutex_t  hash_mutex = PTHREAD_MUTEX_INITIALIZER;


static inline void colorhash_lock( void )
{
     pthread_mutex_lock( &hash_mutex );
}

static inline void colorhash_unlock( void )
{
     pthread_mutex_unlock( &hash_mutex );
}

void colorhash_attach( CorePalette *palette ) {
     colorhash_lock();

     if (!hash) {
          DFB_ASSERT( !hash_users );

          hash = DFBCALLOC( HASH_SIZE, sizeof (Colorhash) );
     }

     hash_users++;

     colorhash_unlock();
}

void colorhash_detach( CorePalette *palette ) {
     colorhash_lock();

     DFB_ASSERT( hash_users > 0 );
     DFB_ASSERT( hash != NULL );
     
     hash_users--;

     if (!hash_users) {
          /* no more users, free allocated resources */
          DFBFREE( hash );
          hash = NULL;
     }

     colorhash_unlock();
}

unsigned int colorhash_lookup( CorePalette *palette,
                               __u8         r,
                               __u8         g,
                               __u8         b,
                               __u8         a )
{
     unsigned int pixel = PIXEL_ARGB(a, r, g, b);
     unsigned int index = (pixel ^ (unsigned int) palette) % HASH_SIZE;

     colorhash_lock();

     DFB_ASSERT( hash != NULL );

     /* try a lookup in the hash table */
     if (hash[index].palette == palette && hash[index].pixel == pixel) {
          /* set the return value */
          index = hash[index].index;
     }
     else { /* look for the closest match */
          DFBColor *entries = palette->entries;
          int min_diff = 0;
          unsigned int i, min_index = 0;
          
          for (i = 0; i < palette->num_entries; i++) {
               int diff = (((ABS( (int) entries[i].r - (int) r ) +
                             ABS( (int) entries[i].g - (int) g ) +
                             ABS( (int) entries[i].b - (int) b )) << 3) +
                           ABS( (int) entries[i].a - (int) a) );

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


void colorhash_invalidate( CorePalette *palette )
{
     unsigned int index = HASH_SIZE - 1;

     colorhash_lock();

     DFB_ASSERT( hash != NULL );

     /* invalidate all entries owned by this palette */
     do {
          if (hash[index].palette == palette)
               hash[index].palette = NULL;
     } while (--index);
     
     colorhash_unlock();
}
