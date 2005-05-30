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

#ifndef IDIRECTFBWINDOW_H
#define IDIRECTFBWINDOW_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBWindow :public IPPAny<IDirectFBWindow, IDirectFBWindow_C>{
friend
     class IDirectFBDisplayLayer;

public:
     IDirectFBWindow(IDirectFBWindow_C* myptr=NULL):IPPAny<IDirectFBWindow, IDirectFBWindow_C>(myptr){}

     DFBWindowID           GetID             ();
     void                  GetPosition       (int                    *x,
                                              int                    *y);
     void                  GetSize           (int                    *width,
                                              int                    *height);

     IDirectFBEventBuffer  CreateEventBuffer ();
     void                  AttachEventBuffer (IDirectFBEventBuffer   *buffer);
     void                  EnableEvents      (DFBWindowEventType      mask);
     void                  DisableEvents     (DFBWindowEventType      mask);

     IDirectFBSurface      GetSurface        ();

     void                  SetOptions        (DFBWindowOptions        options);
     DFBWindowOptions      GetOptions        ();
     void                  SetColorKey       (__u8                    r,
                                              __u8                    g,
                                              __u8                    b);
     void                  SetColorKeyIndex  (unsigned int            index);
     void                  SetOpacity        (__u8                    opacity);
     void                  SetOpaqueRegion   (int                     x1,
                                              int                     y1,
                                              int                     x2,
                                              int                     y2);
     __u8                  GetOpacity        ();
     void                  SetCursorShape    (IDirectFBSurface       *shape,
                                              int                     hot_x,
                                              int                     hot_y);

     void                  RequestFocus      ();
     void                  GrabKeyboard      ();
     void                  UngrabKeyboard    ();
     void                  GrabPointer       ();
     void                  UngrabPointer     ();
     void                  GrabKey           (DFBInputDeviceKeySymbol    symbol,
                                              DFBInputDeviceModifierMask modifiers);
     void                  UngrabKey         (DFBInputDeviceKeySymbol    symbol,
                                              DFBInputDeviceModifierMask modifiers);

     void                  Move              (int                     dx,
                                              int                     dy);
     void                  MoveTo            (int                     x,
                                              int                     y);
     void                  Resize            (unsigned int            width,
                                              unsigned int            height);

     void                  SetStackingClass  (DFBWindowStackingClass  stacking_class);
     void                  Raise             ();
     void                  Lower             ();
     void                  RaiseToTop        ();
     void                  LowerToBottom     ();
     void                  PutAtop           (IDirectFBWindow        *lower);
     void                  PutBelow          (IDirectFBWindow        *upper);

     void                  Close             ();
     void                  Destroy           ();


     inline IDirectFBWindow& operator = (const IDirectFBWindow& other){
          return IPPAny<IDirectFBWindow, IDirectFBWindow_C>::operator =(other);
     }
     inline IDirectFBWindow& operator = (IDirectFBWindow_C* other){
          return IPPAny<IDirectFBWindow, IDirectFBWindow_C>::operator =(other);
     }
};

#endif
