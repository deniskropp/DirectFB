/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
     PPDFB_API IDirectFBFont(IDirectFBFont_C* myptr=NULL):IPPAny<IDirectFBFont, IDirectFBFont_C>(myptr){}

     int            PPDFB_API GetAscender      () const;
     int            PPDFB_API GetDescender     () const;
     int            PPDFB_API GetHeight        () const;
     int            PPDFB_API GetMaxAdvance    () const;

     void           PPDFB_API GetKerning       (unsigned int         prev_index,
                                                unsigned int         current_index,
                                                int                 *kern_x,
                                                int                 *kern_y) const;

     void           PPDFB_API GetStringBreak   (const char          *text,
                                                int                  bytes,
                                                int                  max_width,
                                                int                 *ret_width,
                                                int                 *ret_str_length,
                                                const char         **ret_next_line) const;

     int            PPDFB_API GetStringWidth   (const char          *text,
                                                int                  bytes = -1) const;
     void           PPDFB_API GetStringExtents (const char          *text,
                                                int                  bytes,
                                                DFBRectangle        *logical_rect,
                                                DFBRectangle        *ink_rect) const;
     void           PPDFB_API GetGlyphExtents  (unsigned int         index,
                                                DFBRectangle        *rect,
                                                int                 *advance) const;

     void           PPDFB_API SetEncoding      (DFBTextEncodingID        encoding);
     void           PPDFB_API EnumEncodings    (DFBTextEncodingCallback  callback,
                                                void                    *callbackdata);
     void           PPDFB_API FindEncoding     (const char              *name,
                                                DFBTextEncodingID       *encoding);
     
     
     inline IDirectFBFont PPDFB_API & operator = (const IDirectFBFont& other){
          return IPPAny<IDirectFBFont, IDirectFBFont_C>::operator =(other);
     }
     inline IDirectFBFont PPDFB_API & operator = (IDirectFBFont_C* other){
          return IPPAny<IDirectFBFont, IDirectFBFont_C>::operator =(other);
     }

};

#endif
