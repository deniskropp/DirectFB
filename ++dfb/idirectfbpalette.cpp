/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>

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

#include "++dfb.h"
#include "++dfb_internal.h"

DFBPaletteCapabilities IDirectFBPalette::GetCapabilities()
{
     DFBPaletteCapabilities caps;

     DFBCHECK( iface->GetCapabilities (iface, &caps) );

     return caps;
}

unsigned int IDirectFBPalette::GetSize()
{
     unsigned int size;

     DFBCHECK( iface->GetSize (iface, &size) );

     return size;
}

void IDirectFBPalette::SetEntries (DFBColor     *entries,
                                   unsigned int  num_entries,
                                   unsigned int  offset)
{
     DFBCHECK( iface->SetEntries (iface, entries, num_entries, offset) );
}

void IDirectFBPalette::GetEntries (DFBColor     *entries,
                                   unsigned int  num_entries,
                                   unsigned int  offset)
{
     DFBCHECK( iface->GetEntries (iface, entries, num_entries, offset) );
}

unsigned int IDirectFBPalette::FindBestMatch (__u8 r, __u8 g, __u8 b, __u8 a)
{
     unsigned int index;

     DFBCHECK( iface->FindBestMatch (iface, r, g, b, a, &index) );

     return index;
}

IDirectFBPalette IDirectFBPalette::CreateCopy()
{
     IDirectFBPalette_C *idirectfbpalette;

     DFBCHECK( iface->CreateCopy (iface, &idirectfbpalette) );

     return IDirectFBPalette (idirectfbpalette);
}
