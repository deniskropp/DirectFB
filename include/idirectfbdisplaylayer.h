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

#ifndef IDIRECTFBDISPLAYLAYER_H
#define IDIRECTFBDISPLAYLAYER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBDisplayLayer {
friend
     class IDirectFB;

public:
     DFBDisplayLayerID      GetID                 ();

     DFBDisplayLayerDescription
                            GetDescription        ();

     IDirectFBSurface       GetSurface            ();

     IDirectFBScreen        GetScreen             ();

     void                   SetCooperativeLevel   (DFBDisplayLayerCooperativeLevel level);
     void                   SetOpacity            (__u8                            opacity);
     void                   SetSourceRectangle    (int                             x,
                                                   int                             y,
                                                   int                             width,
                                                   int                             height);
     void                   SetScreenLocation     (float                           x,
                                                   float                           y,
                                                   float                           width,
                                                   float                           height);
     void                   SetSrcColorKey        (__u8                            r,
                                                   __u8                            g,
                                                   __u8                            b);
     void                   SetDstColorKey        (__u8                            r,
                                                   __u8                            g,
                                                   __u8                            b);
     int                    GetLevel              ();
     void                   SetLevel              (int                             level);
     int                    GetCurrentOutputField ();
     void                   SetFieldParity        (int                             field);
     void                   WaitForSync           ();

     void                   GetConfiguration      (DFBDisplayLayerConfig          *config);
     void                   TestConfiguration     (DFBDisplayLayerConfig          &config,
                                                   DFBDisplayLayerConfigFlags     *failed = NULL);
     void                   SetConfiguration      (DFBDisplayLayerConfig          &config);

     void                   SetBackgroundMode     (DFBDisplayLayerBackgroundMode   mode);
     void                   SetBackgroundImage    (IDirectFBSurface               *surface);
     void                   SetBackgroundColor    (__u8                            r,
                                                   __u8                            g,
                                                   __u8                            b,
                                                   __u8                            a = 0xFF);

     void                   GetColorAdjustment    (DFBColorAdjustment             *adj);
     void                   SetColorAdjustment    (DFBColorAdjustment             &adj);

     IDirectFBWindow        CreateWindow          (DFBWindowDescription           &desc);
     IDirectFBWindow        GetWindow             (DFBWindowID                     window_id);

     void                   EnableCursor          (bool                            enable);
     void                   GetCursorPosition     (int                            *x,
                                                   int                            *y);
     void                   WarpCursor            (int                             x,
                                                   int                             y);
     void                   SetCursorAcceleration (int                             numerator,
                                                   int                             denominator,
                                                   int                             threshold);
     void                   SetCursorShape        (IDirectFBSurface               *shape,
                                                   int                             hot_x,
                                                   int                             hot_y);
     void                   SetCursorOpacity      (__u8                            opacity);


     __DFB_PLUS_PLUS__INTERFACE_CLASS( IDirectFBDisplayLayer );
};

#endif
