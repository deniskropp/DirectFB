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

#ifndef IDIRECTFBVIDEOPROVIDER_H
#define IDIRECTFBVIDEOPROVIDER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBVideoProvider : public IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>{
friend
     class IDirectFB;
friend
     class IDirectFBDataBuffer;

public:
     IDirectFBVideoProvider(IDirectFBVideoProvider_C* myptr=NULL):IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>(myptr){}

     DFBVideoProviderCapabilities GetCapabilities       ();
     void                         GetSurfaceDescription (DFBSurfaceDescription *dsc);
     void                         GetStreamDescription  (DFBStreamDescription  *dsc);

     void                         PlayTo                (IDirectFBSurface      *destination,
                                                         DFBRectangle          *destination_rect = NULL,
                                                         DVFrameCallback        callback = NULL,
                                                         void                  *ctx = NULL);
     void                         Stop                  ();
     DFBVideoProviderStatus       GetStatus             ();
     
     void                         SeekTo                (double                 seconds);
     double                       GetPos                ();
     double                       GetLength             ();

     void                         GetColorAdjustment    (DFBColorAdjustment    *adj);
     void                         SetColorAdjustment    (DFBColorAdjustment    &adj);

     void                         SendEvent             (DFBEvent              &evt);

     void                         SetPlaybackFlags      (DFBVideoProviderPlaybackFlags flags);

     void                         SetSpeed              (double                 multiplier);
     double                       GetSpeed              ();

     void                         SetVolume             (float                  level);
     float                        GetVolume             ();
     
     inline IDirectFBVideoProvider& operator = (const IDirectFBVideoProvider& other){
          return IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>::operator =(other);
     }
     inline IDirectFBVideoProvider& operator = (IDirectFBVideoProvider_C* other){
          return IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>::operator =(other);
     }
};

#endif
