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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <math.h>


#include <directfb.h>

#include <core/surfaces.h>
#include <core/palette.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "idirectfbpalette.h"



static void
IDirectFBPalette_Destruct( IDirectFBPalette *thiz )
{
     IDirectFBPalette_data *data = (IDirectFBPalette_data*)thiz->priv;

     if (data->palette)
          dfb_palette_unref( data->palette );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBPalette_AddRef( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_Release( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     if (--data->ref == 0)
          IDirectFBPalette_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_GetCapabilities( IDirectFBPalette       *thiz,
                                  DFBPaletteCapabilities *caps )
{
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     palette = data->palette;
     if (!palette)
          return DFB_DESTROYED;

     if (!caps)
          return DFB_INVARG;

     /* FIXME: no caps yet */
     *caps = DPCAPS_NONE;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_GetSize( IDirectFBPalette *thiz,
                          unsigned int     *size )
{
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     palette = data->palette;
     if (!palette)
          return DFB_DESTROYED;

     if (!size)
          return DFB_INVARG;

     *size = palette->num_entries;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_SetEntries( IDirectFBPalette *thiz,
                             const DFBColor   *entries,
                             unsigned int      num_entries,
                             unsigned int      offset )
{
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     palette = data->palette;
     if (!palette)
          return DFB_DESTROYED;

     if (!entries  ||  offset + num_entries > palette->num_entries)
          return DFB_INVARG;

     if (num_entries) {
          direct_memcpy( palette->entries + offset, entries, num_entries * sizeof(DFBColor));

          dfb_palette_update( palette, offset, offset + num_entries - 1 );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_GetEntries( IDirectFBPalette *thiz,
                             DFBColor         *entries,
                             unsigned int      num_entries,
                             unsigned int      offset )
{
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     palette = data->palette;
     if (!palette)
          return DFB_DESTROYED;

     if (!entries  ||  offset + num_entries > palette->num_entries)
          return DFB_INVARG;

     direct_memcpy( entries, palette->entries + offset, num_entries * sizeof(DFBColor));

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_FindBestMatch( IDirectFBPalette *thiz,
                                u8                r,
                                u8                g,
                                u8                b,
                                u8                a,
                                unsigned int     *index )
{
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     if (!index)
          return DFB_INVARG;

     palette = data->palette;
     if (!palette)
          return DFB_DESTROYED;

     *index = dfb_palette_search( palette, r, g, b, a );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_CreateCopy( IDirectFBPalette  *thiz,
                             IDirectFBPalette **interface )
{
     DFBResult         ret;
     IDirectFBPalette *iface;
     CorePalette      *palette = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBPalette)

     if (!data->palette)
          return DFB_DESTROYED;

     if (!interface)
          return DFB_INVARG;

     ret = dfb_palette_create( NULL,    /* FIXME */
                               data->palette->num_entries, &palette );
     if (ret)
          return ret;

     direct_memcpy( palette->entries, data->palette->entries,
                    palette->num_entries * sizeof(DFBColor));

     dfb_palette_update( palette, 0, palette->num_entries - 1 );


     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( iface, palette );

     dfb_palette_unref( palette );

     if (!ret)
          *interface = iface;

     return ret;
}

/******/

DFBResult IDirectFBPalette_Construct( IDirectFBPalette *thiz,
                                      CorePalette      *palette )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBPalette)

     if (dfb_palette_ref( palette )) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return DFB_FAILURE;
     }

     data->ref     = 1;
     data->palette = palette;


     thiz->AddRef          = IDirectFBPalette_AddRef;
     thiz->Release         = IDirectFBPalette_Release;

     thiz->GetCapabilities = IDirectFBPalette_GetCapabilities;
     thiz->GetSize         = IDirectFBPalette_GetSize;

     thiz->SetEntries      = IDirectFBPalette_SetEntries;
     thiz->GetEntries      = IDirectFBPalette_GetEntries;
     thiz->FindBestMatch   = IDirectFBPalette_FindBestMatch;

     thiz->CreateCopy      = IDirectFBPalette_CreateCopy;

     return DFB_OK;
}

