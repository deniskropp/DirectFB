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
     PPDFB_API IDirectFBVideoProvider(IDirectFBVideoProvider_C* myptr=NULL):IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>(myptr){}

     DFBVideoProviderCapabilities PPDFB_API GetCapabilities       ();
     void                         PPDFB_API GetSurfaceDescription (DFBSurfaceDescription *dsc);
     void                         PPDFB_API GetStreamDescription  (DFBStreamDescription  *dsc);

     void                         PPDFB_API PlayTo                (IDirectFBSurface      *destination,
                                                                   DFBRectangle          *destination_rect = NULL,
                                                                   DVFrameCallback        callback = NULL,
                                                                   void                  *ctx = NULL);
     void                         PPDFB_API Stop                  ();
     DFBVideoProviderStatus       PPDFB_API GetStatus             ();
     
     void                         PPDFB_API SeekTo                (double                 seconds);
     double                       PPDFB_API GetPos                ();
     double                       PPDFB_API GetLength             ();

     void                         PPDFB_API GetColorAdjustment    (DFBColorAdjustment    *adj);
     void                         PPDFB_API SetColorAdjustment    (DFBColorAdjustment    &adj);

     void                         PPDFB_API SendEvent             (DFBEvent              &evt);

     void                         PPDFB_API SetPlaybackFlags      (DFBVideoProviderPlaybackFlags flags);

     void                         PPDFB_API SetSpeed              (double                 multiplier);
     double                       PPDFB_API GetSpeed              ();

     void                         PPDFB_API SetVolume             (float                  level);
     float                        PPDFB_API GetVolume             ();


     inline IDirectFBVideoProvider PPDFB_API & operator = (const IDirectFBVideoProvider& other){
          return IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>::operator =(other);
     }
     inline IDirectFBVideoProvider PPDFB_API & operator = (IDirectFBVideoProvider_C* other){
          return IPPAny<IDirectFBVideoProvider, IDirectFBVideoProvider_C>::operator =(other);
     }
};

#endif
