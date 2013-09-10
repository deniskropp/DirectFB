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


#ifndef IDIRECTFBDISPLAYLAYER_H
#define IDIRECTFBDISPLAYLAYER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBDisplayLayer : public IPPAny<IDirectFBDisplayLayer, IDirectFBDisplayLayer_C> {
friend
     class IDirectFB;

public:
     PPDFB_API IDirectFBDisplayLayer(IDirectFBDisplayLayer_C* myptr=NULL):IPPAny<IDirectFBDisplayLayer, IDirectFBDisplayLayer_C>(myptr){}

     DFBDisplayLayerID      PPDFB_API GetID                 ();

     DFBDisplayLayerDescription
                            PPDFB_API GetDescription        ();
                            
     void                   PPDFB_API GetSourceDescriptions (DFBDisplayLayerSourceDescription *desc);

     IDirectFBSurface       PPDFB_API GetSurface            ();

     IDirectFBScreen        PPDFB_API GetScreen             ();

     void                   PPDFB_API SetCooperativeLevel   (DFBDisplayLayerCooperativeLevel level);
     void                   PPDFB_API SetOpacity            (u8                            opacity);
     void                   PPDFB_API SetSourceRectangle    (int                             x,
                                                             int                             y,
                                                             int                             width,
                                                             int                             height);
     void                   PPDFB_API SetScreenLocation     (float                           x,
                                                             float                           y,
                                                             float                           width,
                                                             float                           height);
     void                   PPDFB_API SetScreenPosition     (int                             x,
                                                             int                             y);
     void                   PPDFB_API SetScreenRectangle    (int                             x,
                                                             int                             y,
                                                             int                             width,
                                                             int                             height);
     void                   PPDFB_API SetClipRegions        (const DFBRegion                *regions,
                                                             int                             num_regions,
                                                             DFBBoolean                      positive);
     void                   PPDFB_API SetSrcColorKey        (u8                            r,
                                                             u8                            g,
                                                             u8                            b);
     void                   PPDFB_API SetDstColorKey        (u8                            r,
                                                             u8                            g,
                                                             u8                            b);
     int                    PPDFB_API GetLevel              ();
     void                   PPDFB_API SetLevel              (int                             level);
     int                    PPDFB_API GetCurrentOutputField ();
     void                   PPDFB_API SetFieldParity        (int                             field);
     void                   PPDFB_API WaitForSync           ();

     void                   PPDFB_API GetConfiguration      (DFBDisplayLayerConfig          *config);
     void                   PPDFB_API TestConfiguration     (DFBDisplayLayerConfig          &config,
                                                             DFBDisplayLayerConfigFlags     *failed = NULL);
     void                   PPDFB_API SetConfiguration      (DFBDisplayLayerConfig          &config);

     void                   PPDFB_API SetBackgroundMode     (DFBDisplayLayerBackgroundMode   mode);
     void                   PPDFB_API SetBackgroundImage    (IDirectFBSurface               *surface);
     void                   PPDFB_API SetBackgroundColor    (u8                            r,
                                                             u8                            g,
                                                             u8                            b,
                                                             u8                            a = 0xFF);

     void                   PPDFB_API GetColorAdjustment    (DFBColorAdjustment             *adj);
     void                   PPDFB_API SetColorAdjustment    (DFBColorAdjustment             &adj);

     IDirectFBWindow        PPDFB_API CreateWindow          (DFBWindowDescription           &desc);
     IDirectFBWindow        PPDFB_API GetWindow             (DFBWindowID                     window_id);

     void                   PPDFB_API EnableCursor          (bool                            enable);
     void                   PPDFB_API GetCursorPosition     (int                            *x,
                                                             int                            *y);
     void                   PPDFB_API WarpCursor            (int                             x,
                                                             int                             y);
     void                   PPDFB_API SetCursorAcceleration (int                             numerator,
                                                             int                             denominator,
                                                             int                             threshold);
     void                   PPDFB_API SetCursorShape        (IDirectFBSurface               *shape,
                                                             int                             hot_x,
                                                             int                             hot_y);
     void                   PPDFB_API SetCursorOpacity      (u8                            opacity);

     void                   PPDFB_API SwitchContext         (DFBBoolean                      exclusive);
     void                   PPDFB_API SetSurface            (IDirectFBSurface               *surface);

     inline IDirectFBDisplayLayer PPDFB_API & operator = (const IDirectFBDisplayLayer& other){
          return IPPAny<IDirectFBDisplayLayer, IDirectFBDisplayLayer_C>::operator =(other);
     }
     inline IDirectFBDisplayLayer PPDFB_API & operator = (IDirectFBDisplayLayer_C* other){
          return IPPAny<IDirectFBDisplayLayer, IDirectFBDisplayLayer_C>::operator =(other);
     }
};

#endif
