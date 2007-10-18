/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/palette.h>
#include <core/colorhash.h>

#include <misc/util.h>
#include <gfx/convert.h>


D_DEBUG_DOMAIN( Core_ColorHash, "Core/ColorHash", "DirectFB ColorHash Core" );


#define HASH_SIZE 823

typedef struct {
     unsigned int  pixel;
     unsigned int  index;
     CorePalette  *palette;
} Colorhash;

/**********************************************************************************************************************/

typedef struct {
     int                     magic;

     Colorhash              *hash;
     unsigned int            hash_users;
     FusionSkirmish          hash_lock;

     FusionSHMPoolShared    *shmpool;
} DFBColorHashCoreShared;

struct __DFB_DFBColorHashCore {
     int                     magic;

     CoreDFB                *core;

     DFBColorHashCoreShared *shared;
};

DFB_CORE_PART( colorhash_core, ColorHashCore );

/**********************************************************************************************************************/

static DFBColorHashCore *core_colorhash; /* FIXME */


static DFBResult
dfb_colorhash_core_initialize( CoreDFB                *core,
                               DFBColorHashCore       *data,
                               DFBColorHashCoreShared *shared )
{
     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_initialize( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     core_colorhash = data; /* FIXME */

     data->core   = core;
     data->shared = shared;

     shared->shmpool = dfb_core_shmpool( core );

     fusion_skirmish_init( &shared->hash_lock, "Colorhash Core", dfb_core_world(core) );

     D_MAGIC_SET( data, DFBColorHashCore );
     D_MAGIC_SET( shared, DFBColorHashCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_join( CoreDFB                *core,
                         DFBColorHashCore       *data,
                         DFBColorHashCoreShared *shared )
{
     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_join( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBColorHashCoreShared );

     core_colorhash = data; /* FIXME */

     data->core   = core;
     data->shared = shared;

     D_MAGIC_SET( data, DFBColorHashCore );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_shutdown( DFBColorHashCore *data,
                             bool              emergency )
{
     DFBColorHashCoreShared *shared;

     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_shutdown( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     shared = data->shared;

     fusion_skirmish_destroy( &shared->hash_lock );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_leave( DFBColorHashCore *data,
                          bool              emergency )
{
     DFBColorHashCoreShared *shared;

     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_leave( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     shared = data->shared;

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_suspend( DFBColorHashCore *data )
{
     DFBColorHashCoreShared *shared;

     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_suspend( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     shared = data->shared;

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_resume( DFBColorHashCore *data )
{
     DFBColorHashCoreShared *shared;

     D_DEBUG_AT( Core_ColorHash, "dfb_colorhash_core_resume( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     shared = data->shared;

     return DFB_OK;
}

/**********************************************************************************************************************/

void
dfb_colorhash_attach( DFBColorHashCore *core,
                      CorePalette      *palette )
{
     DFBColorHashCoreShared *shared;

     D_ASSUME( core != NULL );

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = core_colorhash;

     shared = core->shared;

     fusion_skirmish_prevail( &shared->hash_lock );

     if (!shared->hash) {
          D_ASSERT( shared->hash_users == 0 );

          shared->hash = SHCALLOC( shared->shmpool, HASH_SIZE, sizeof (Colorhash) );
     }

     shared->hash_users++;

     fusion_skirmish_dismiss( &shared->hash_lock );
}

void
dfb_colorhash_detach( DFBColorHashCore *core,
                      CorePalette      *palette )
{
     DFBColorHashCoreShared *shared;

     D_ASSUME( core != NULL );

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = core_colorhash;

     shared = core->shared;

     D_ASSERT( shared->hash_users > 0 );
     D_ASSERT( shared->hash != NULL );

     fusion_skirmish_prevail( &shared->hash_lock );

     shared->hash_users--;

     if (!shared->hash_users) {
          /* no more users, free allocated resources */
          SHFREE( shared->shmpool, shared->hash );
          shared->hash = NULL;
     }

     fusion_skirmish_dismiss( &shared->hash_lock );
}

unsigned int
dfb_colorhash_lookup( DFBColorHashCore *core,
                      CorePalette      *palette,
                      __u8              r,
                      __u8              g,
                      __u8              b,
                      __u8              a )
{
     unsigned int            pixel = PIXEL_ARGB(a, r, g, b);
     unsigned int            index = (pixel ^ (unsigned long) palette) % HASH_SIZE;
     DFBColorHashCoreShared *shared;

//     D_ASSUME( core != NULL );

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = core_colorhash;

     shared = core->shared;

     D_ASSERT( shared->hash != NULL );

     fusion_skirmish_prevail( &shared->hash_lock );

     /* try a lookup in the hash table */
     if (shared->hash[index].palette == palette && shared->hash[index].pixel == pixel) {
          /* set the return value */
          index = shared->hash[index].index;
     } else { /* look for the closest match */
          DFBColor *entries = palette->entries;
          int min_diff = 0;
          unsigned int i, min_index = 0;

          for (i = 0; i < palette->num_entries; i++) {
               int diff;

               int r_diff = (int) entries[i].r - (int) r;
               int g_diff = (int) entries[i].g - (int) g;
               int b_diff = (int) entries[i].b - (int) b;
               int a_diff = (int) entries[i].a - (int) a;

               if (a)
                    diff = (r_diff * r_diff + g_diff * g_diff +
                            b_diff * b_diff + ((a_diff * a_diff) >> 6));
               else
                    diff = (r_diff + g_diff + b_diff + (a_diff * a_diff));

               if (i == 0 || diff < min_diff) {
                    min_diff = diff;
                    min_index = i;
               }

               if (!diff)
                    break;
          }

          /* store the matching entry in the hash table */
          shared->hash[index].pixel   = pixel;
          shared->hash[index].index   = min_index;
          shared->hash[index].palette = palette;

          /* set the return value */
          index = min_index;
     }

     fusion_skirmish_dismiss( &shared->hash_lock );

     return index;
}

void
dfb_colorhash_invalidate( DFBColorHashCore *core,
                          CorePalette      *palette )
{
     unsigned int            index = HASH_SIZE - 1;
     DFBColorHashCoreShared *shared;

     D_ASSUME( core != NULL );

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = core_colorhash;

     shared = core->shared;

     D_ASSERT( shared->hash != NULL );

     fusion_skirmish_prevail( &shared->hash_lock );

     /* invalidate all entries owned by this palette */
     do {
          if (shared->hash[index].palette == palette)
               shared->hash[index].palette = NULL;
     } while (index--);

     fusion_skirmish_dismiss( &shared->hash_lock );
}

