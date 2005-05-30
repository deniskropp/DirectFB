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
     IDirectFBImageProvider(IDirectFBImageProvider_C* myptr=NULL):IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>(myptr){}

     void                    GetSurfaceDescription (DFBSurfaceDescription *dsc);
     void                    GetImageDescription   (DFBImageDescription   *dsc);

     void                    RenderTo              (IDirectFBSurface      *destination,
                                                    DFBRectangle          *destination_rect);

     void                    SetRenderCallback     (DIRenderCallback       callback,
                                                    void                  *callback_data);

     inline IDirectFBImageProvider& operator = (const IDirectFBImageProvider& other){
          return IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>::operator =(other);
     }
     inline IDirectFBImageProvider& operator = (IDirectFBImageProvider_C* other){
          return IPPAny<IDirectFBImageProvider, IDirectFBImageProvider_C>::operator =(other);
     }
};

#endif
