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

class IDirectFBFont : public IPPAny<IDirectFBFont, IDirectFBFont_C> {
friend
     class IDirectFB;
friend
     class IDirectFBSurface;

public:
     IDirectFBFont(IDirectFBFont_C* myptr=NULL):IPPAny<IDirectFBFont, IDirectFBFont_C>(myptr){}

     int            GetAscender      () const;
     int            GetDescender     () const;
     int            GetHeight        () const;
     int            GetMaxAdvance    () const;

     void           GetKerning       (unsigned int         prev_index,
                                      unsigned int         current_index,
                                      int                 *kern_x,
                                      int                 *kern_y) const;

     int            GetStringWidth   (const char          *text,
                                      int                  bytes = -1) const;
     void           GetStringExtents (const char          *text,
                                      int                  bytes,
                                      DFBRectangle        *logical_rect,
                                      DFBRectangle        *ink_rect) const;
     void           GetGlyphExtents  (unsigned int         index,
                                      DFBRectangle        *rect,
                                      int                 *advance) const;

     inline IDirectFBFont& operator = (const IDirectFBFont& other){
          return IPPAny<IDirectFBFont, IDirectFBFont_C>::operator =(other);
     }
     inline IDirectFBFont& operator = (IDirectFBFont_C* other){
          return IPPAny<IDirectFBFont, IDirectFBFont_C>::operator =(other);
     }

};

#endif
