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


#ifndef IDIRECTFBIMAGEPROVIDER_H
#define IDIRECTFBIMAGEPROVIDER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBImageProvider : public IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>{
friend
     class IDirectFB;
friend
     class IDirectFBDataBuffer;

public:
     PPDFB_API IDirectFBImageProvider(IDirectFBImageProvider_C* myptr=NULL):IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>(myptr){}

     void                    PPDFB_API GetSurfaceDescription (DFBSurfaceDescription *dsc);
     void                    PPDFB_API GetImageDescription   (DFBImageDescription   *dsc);

     void                    PPDFB_API RenderTo              (IDirectFBSurface      *destination,
                                                              DFBRectangle          *destination_rect);

     void                    PPDFB_API SetRenderCallback     (DIRenderCallback       callback,
                                                              void                  *callback_data);

     inline IDirectFBImageProvider PPDFB_API & operator = (const IDirectFBImageProvider& other){
          return IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>::operator =(other);
     }
     inline IDirectFBImageProvider PPDFB_API & operator = (IDirectFBImageProvider_C* other){
          return IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>::operator =(other);
     }
};

#endif
