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


#ifndef IDIRECTFBWINDOW_H
#define IDIRECTFBWINDOW_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBWindow :public IPPAny<IDirectFBWindow, IDirectFBWindow_C>{
friend
     class IDirectFBDisplayLayer;

public:
     PPDFB_API IDirectFBWindow(IDirectFBWindow_C* myptr=NULL):IPPAny<IDirectFBWindow, IDirectFBWindow_C>(myptr){}

     DFBWindowID           PPDFB_API GetID             ();
     void                  PPDFB_API GetPosition       (int                    *x,
                                                        int                    *y);
     void                  PPDFB_API GetSize           (int                    *width,
                                                        int                    *height);

     IDirectFBEventBuffer  PPDFB_API CreateEventBuffer ();
     void                  PPDFB_API AttachEventBuffer (IDirectFBEventBuffer   *buffer);
     void                  PPDFB_API DetachEventBuffer (IDirectFBEventBuffer   *buffer);
     void                  PPDFB_API EnableEvents      (DFBWindowEventType      mask);
     void                  PPDFB_API DisableEvents     (DFBWindowEventType      mask);

     IDirectFBSurface      PPDFB_API GetSurface        ();

     void                  PPDFB_API SetOptions        (DFBWindowOptions        options);
     DFBWindowOptions      PPDFB_API GetOptions        ();
     void                  PPDFB_API SetColorKey       (u8                    r,
                                                        u8                    g,
                                                        u8                    b);
     void                  PPDFB_API SetColorKeyIndex  (unsigned int            index);
     void                  PPDFB_API SetOpacity        (u8                    opacity);
     void                  PPDFB_API SetOpaqueRegion   (int                     x1,
                                                        int                     y1,
                                                        int                     x2,
                                                        int                     y2);
     u8                    PPDFB_API GetOpacity        ();
     void                  PPDFB_API SetCursorShape    (IDirectFBSurface       *shape,
                                                        int                     hot_x,
                                                        int                     hot_y);

     void                  PPDFB_API RequestFocus      ();
     void                  PPDFB_API GrabKeyboard      ();
     void                  PPDFB_API UngrabKeyboard    ();
     void                  PPDFB_API GrabPointer       ();
     void                  PPDFB_API UngrabPointer     ();
     void                  PPDFB_API GrabKey           (DFBInputDeviceKeySymbol    symbol,
                                                        DFBInputDeviceModifierMask modifiers);
     void                  PPDFB_API UngrabKey         (DFBInputDeviceKeySymbol    symbol,
                                                        DFBInputDeviceModifierMask modifiers);

     void                  PPDFB_API Move              (int                     dx,
                                                        int                     dy);
     void                  PPDFB_API MoveTo            (int                     x,
                                                        int                     y);
     void                  PPDFB_API Resize            (int                     width,
                                                        int                     height);

     void                  PPDFB_API SetStackingClass  (DFBWindowStackingClass  stacking_class);
     void                  PPDFB_API Raise             ();
     void                  PPDFB_API Lower             ();
     void                  PPDFB_API RaiseToTop        ();
     void                  PPDFB_API LowerToBottom     ();
     void                  PPDFB_API PutAtop           (IDirectFBWindow        *lower);
     void                  PPDFB_API PutBelow          (IDirectFBWindow        *upper);

     void                  PPDFB_API Close             ();
     void                  PPDFB_API Destroy           ();

     void                  PPDFB_API SetBounds         (int                     x,
                                                        int                     y,
                                                        int                     width,
                                                        int                     height);

     void                  PPDFB_API ResizeSurface     (int                     width,
                                                        int                     height);

     void                  PPDFB_API BeginUpdates      (const DFBRegion        *update = NULL);

     void                  PPDFB_API SetDstGeometry    (DFBWindowGeometry       *geometry);
     void                  PPDFB_API SetSrcGeometry    (DFBWindowGeometry       *geometry);

     void                  PPDFB_API SetApplicationID  (unsigned long           application_id);
     unsigned long         PPDFB_API GetApplicationID  ();


     inline IDirectFBWindow PPDFB_API & operator = (const IDirectFBWindow& other){
          return IPPAny<IDirectFBWindow, IDirectFBWindow_C>::operator =(other);
     }
     inline IDirectFBWindow PPDFB_API & operator = (IDirectFBWindow_C* other){
          return IPPAny<IDirectFBWindow, IDirectFBWindow_C>::operator =(other);
     }
};

#endif
