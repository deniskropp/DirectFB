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

#ifndef __PALETTE_H__
#define __PALETTE_H__

#include <directfb.h>
#include <core/coretypes.h>
#include <core/fusion/object.h>

struct _CorePalette {
     FusionObject  object;

     int           num_entries;
     DFBColor     *entries;

     struct {
          int      index;
          DFBColor color;
     } search_cache;
  
     bool          hash_attached;
};

typedef enum {
     CPNF_ENTRIES = 0x00000001,
     CPNF_DESTROY = 0x00000002
} CorePaletteNotificationFlags;

typedef struct {
     CorePaletteNotificationFlags  flags;
     CorePalette                  *palette;
     int                           first;
     int                           last;
} CorePaletteNotification;


DFBResult    dfb_palette_create             ( unsigned int   size,
                                              CorePalette  **ret_palette );

void         dfb_palette_generate_rgb332_map( CorePalette   *palette );
void         dfb_palette_generate_rgb121_map( CorePalette   *palette );

unsigned int dfb_palette_search             ( CorePalette   *palette,
                                              __u8           r,
                                              __u8           g,
                                              __u8           b,
                                              __u8           a );

void         dfb_palette_update             ( CorePalette   *palette,
                                              int            first,
                                              int            last );


/*
 * creates a palette pool
 */
FusionObjectPool *dfb_palette_pool_create();

static inline void
dfb_palette_pool_destroy( FusionObjectPool *pool )
{
     fusion_object_pool_destroy( pool );
}

static inline FusionResult
dfb_palette_attach( CorePalette *palette,
                    React        react,
                    void        *ctx,
                    Reaction    *reaction )
{
     return fusion_object_attach( &palette->object, react, ctx, reaction );
}

static inline FusionResult
dfb_palette_detach( CorePalette *palette,
                    Reaction    *reaction )
{
     return fusion_object_detach( &palette->object, reaction );
}

static inline FusionResult
dfb_palette_attach_global( CorePalette    *palette,
                           int             react_index,
                           void           *ctx,
                           GlobalReaction *reaction )
{
     return fusion_object_attach_global( &palette->object,
                                         react_index, ctx, reaction );
}

static inline FusionResult
dfb_palette_detach_global( CorePalette    *palette,
                           GlobalReaction *reaction )
{
     return fusion_object_detach_global( &palette->object, reaction );
}

static inline FusionResult
dfb_palette_ref( CorePalette *palette )
{
     return fusion_object_ref( &palette->object );
}

static inline FusionResult
dfb_palette_unref( CorePalette *palette )
{
     return fusion_object_unref( &palette->object );
}

static inline FusionResult
dfb_palette_link( CorePalette **link,
                  CorePalette  *palette )
{
     return fusion_object_link( (FusionObject**) link, &palette->object );
}

static inline FusionResult
dfb_palette_unlink( CorePalette *palette )
{
     return fusion_object_unlink( &palette->object );
}


/* global reactions */

typedef enum {
     DFB_SURFACE_PALETTE_LISTENER
} DFB_PALETTE_GLOBALS;

#endif

