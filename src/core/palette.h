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

#ifndef __PALETTE_H__
#define __PALETTE_H__

#include <directfb.h>
#include <core/coretypes.h>
#include <core/fusion/object.h>

struct _CorePalette {
     FusionObject  object;

     unsigned int  num_entries;
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


DFBResult    dfb_palette_create             ( CoreDFB       *core,
                                              unsigned int   size,
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
 * Creates a pool of palette objects.
 */
FusionObjectPool *dfb_palette_pool_create();

/*
 * Generates dfb_palette_ref(), dfb_palette_attach() etc.
 */
FUSION_OBJECT_METHODS( CorePalette, dfb_palette )


/* global reactions */

typedef enum {
     DFB_SURFACE_PALETTE_LISTENER
} DFB_PALETTE_GLOBALS;

#endif

