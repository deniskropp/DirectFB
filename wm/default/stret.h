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

#ifndef __STRET_H__
#define __STRET_H__

/*
 * A 'StReT' is a Stack Region Tree.
 */

typedef struct __S_StretRegion     StretRegion;

typedef enum {
     SRF_NONE       = 0x00000000,

     SRF_INPUT      = 0x00000001,
     SRF_OUTPUT     = 0x00000002,
     SRF_ACTIVE     = 0x00000004,

     SRF_OPAQUE     = 0x00000010,
     SRF_SHAPED     = 0x00000020,

     SRF_ALL        = 0x00000037
} StretRegionFlags;


typedef struct {
     void (*Update)( StretRegion     *region,
                     void            *region_data,
                     void            *update_data,
                     unsigned long    arg,
                     int              x,
                     int              y,
                     const DFBRegion *updates,
                     int              num );
} StretRegionClass;

typedef int StretRegionClassID;


DFBResult stret_class_register  ( const StretRegionClass *clazz,
                                  StretRegionClassID     *ret_id );

DFBResult stret_class_unregister( StretRegionClassID      id );



DFBResult stret_region_create ( StretRegionClassID   class_id,
                                void                *data,
                                unsigned long        arg,
                                StretRegionFlags     flags,
                                int                  x,
                                int                  y,
                                int                  width,
                                int                  height,
                                StretRegion         *parent,
                                StretRegion        **ret_region );

DFBResult stret_region_destroy( StretRegion         *region );


DFBResult stret_region_enable ( StretRegion         *region,
                                StretRegionFlags     flags );

DFBResult stret_region_disable( StretRegion         *region,
                                StretRegionFlags     flags );


DFBResult stret_region_move   ( StretRegion         *region,
                                int                  dx,
                                int                  dy );

DFBResult stret_region_resize ( StretRegion         *region,
                                int                  width,
                                int                  height );

DFBResult stret_region_restack( StretRegion         *region,
                                int                  index );


DFBResult stret_region_visible( StretRegion         *region,
                                const DFBRegion     *base,
                                DFBRegion           *ret_regions,
                                int                  max_num,
                                int                 *ret_num );

DFBResult stret_region_update ( StretRegion         *region,
                                const DFBRegion     *update,
                                void                *update_data );

#endif

