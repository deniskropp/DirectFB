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

int IDirectFBFont::GetAscender()
{
     int ascender;

     DFBCHECK( iface->GetAscender (iface, &ascender) );

     return ascender;
}

int IDirectFBFont::GetDescender()
{
     int descender;

     DFBCHECK( iface->GetDescender (iface, &descender) );

     return descender;
}

int IDirectFBFont::GetHeight()
{
     int height;

     DFBCHECK( iface->GetHeight (iface, &height) );

     return height;
}

int IDirectFBFont::GetMaxAdvance()
{
     int max_advance;

     DFBCHECK( iface->GetMaxAdvance (iface, &max_advance) );

     return max_advance;
}

void IDirectFBFont::GetKerning (unsigned int  prev_index,
                                unsigned int  current_index,
                                int          *kern_x,
                                int          *kern_y)
{
     DFBCHECK( iface->GetKerning (iface, prev_index, current_index, kern_x, kern_y) );
}

int IDirectFBFont::GetStringWidth (const char *text, int bytes)
{
     int width;

     DFBCHECK( iface->GetStringWidth (iface, text, bytes, &width) );

     return width;
}

void IDirectFBFont::GetStringExtents (const char   *text,
                                      int           bytes,
                                      DFBRectangle *logical_rect,
                                      DFBRectangle *ink_rect)
{
     DFBCHECK( iface->GetStringExtents (iface, text, bytes,
                                        logical_rect, ink_rect) );
}

void IDirectFBFont::GetGlyphExtents  (unsigned int  index,
                                      DFBRectangle *rect,
                                      int          *advance)
{
     DFBCHECK( iface->GetGlyphExtents (iface, index, rect, advance) );
}
