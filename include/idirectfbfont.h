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

#ifndef IDIRECTFBFONT_H
#define IDIRECTFBFONT_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBFont {
friend
     class IDirectFB;
friend
     class IDirectFBSurface;

public:
     int            GetAscender      ();
     int            GetDescender     ();
     int            GetHeight        ();
     int            GetMaxAdvance    ();

     void           GetKerning       (unsigned int         prev_index,
                                      unsigned int         current_index,
                                      int                 *kern_x,
                                      int                 *kern_y);

     int            GetStringWidth   (const char          *text,
                                      int                  bytes = -1);
     void           GetStringExtents (const char          *text,
                                      int                  bytes,
                                      DFBRectangle        *logical_rect,
                                      DFBRectangle        *ink_rect);
     void           GetGlyphExtents  (unsigned int         index,
                                      DFBRectangle        *rect,
                                      int                 *advance);


     __DFB_PLUS_PLUS__INTERFACE_CLASS( IDirectFBFont );
};

#endif
