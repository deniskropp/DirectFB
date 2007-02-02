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

#include "++dfb.h"
#include "++dfb_internal.h"

DFBWindowID IDirectFBWindow::GetID()
{
     DFBWindowID window_id;

     DFBCHECK( iface->GetID (iface, &window_id) );

     return window_id;
}

void IDirectFBWindow::GetPosition (int *x, int *y)
{
     DFBCHECK( iface->GetPosition (iface, x, y) );
}

void IDirectFBWindow::GetSize (int *width,
                               int *height)
{
     DFBCHECK( iface->GetSize (iface, width, height) );
}

IDirectFBEventBuffer IDirectFBWindow::CreateEventBuffer()
{
     IDirectFBEventBuffer_C *idirectfbeventbuffer;

     DFBCHECK( iface->CreateEventBuffer (iface, &idirectfbeventbuffer) );

     return IDirectFBEventBuffer (idirectfbeventbuffer);
}

void IDirectFBWindow::AttachEventBuffer (IDirectFBEventBuffer *buffer)
{
     DFBCHECK( iface->AttachEventBuffer (iface, buffer->get_iface()) );
}

void IDirectFBWindow::DetachEventBuffer (IDirectFBEventBuffer *buffer)
{
     DFBCHECK( iface->DetachEventBuffer (iface, buffer->get_iface()) );
}

void IDirectFBWindow::EnableEvents (DFBWindowEventType mask)
{
     DFBCHECK( iface->EnableEvents (iface, mask) );
}

void IDirectFBWindow::DisableEvents (DFBWindowEventType mask)
{
     DFBCHECK( iface->DisableEvents (iface, mask) );
}

IDirectFBSurface IDirectFBWindow::GetSurface()
{
     IDirectFBSurface_C *idirectfbsurface;

     DFBCHECK( iface->GetSurface (iface, &idirectfbsurface) );

     return IDirectFBSurface (idirectfbsurface);
}

void IDirectFBWindow::SetOptions (DFBWindowOptions options)
{
     DFBCHECK( iface->SetOptions (iface, options) );
}

DFBWindowOptions IDirectFBWindow::GetOptions ()
{
     DFBWindowOptions options;

     DFBCHECK( iface->GetOptions (iface, &options) );

     return options;
}

void IDirectFBWindow::SetColorKey (__u8 r, __u8 g, __u8 b)
{
     DFBCHECK( iface->SetColorKey (iface, r, g, b) );
}

void IDirectFBWindow::SetColorKeyIndex (unsigned int index)
{
     DFBCHECK( iface->SetColorKeyIndex (iface, index) );
}

void IDirectFBWindow::SetOpacity (__u8 opacity)
{
     DFBCHECK( iface->SetOpacity (iface, opacity) );
}

void IDirectFBWindow::SetOpaqueRegion (int x1, int y1, int x2, int y2)
{
     DFBCHECK( iface->SetOpaqueRegion (iface, x1, y1, x2, y2) );
}

__u8 IDirectFBWindow::GetOpacity()
{
     __u8 opacity;

     DFBCHECK( iface->GetOpacity (iface, &opacity) );

     return opacity;
}

void IDirectFBWindow::SetCursorShape (IDirectFBSurface *shape,
                                      int               hot_x,
                                      int               hot_y)
{
     DFBCHECK( iface->SetCursorShape (iface, shape->get_iface(), hot_x, hot_y) );
}

void IDirectFBWindow::RequestFocus()
{
     DFBCHECK( iface->RequestFocus (iface) );
}

void IDirectFBWindow::GrabKeyboard()
{
     DFBCHECK( iface->GrabKeyboard (iface) );
}

void IDirectFBWindow::UngrabKeyboard()
{
     DFBCHECK( iface->UngrabKeyboard (iface) );
}

void IDirectFBWindow::GrabPointer()
{
     DFBCHECK( iface->GrabPointer (iface) );
}

void IDirectFBWindow::UngrabPointer()
{
     DFBCHECK( iface->UngrabPointer (iface) );
}

void IDirectFBWindow::GrabKey (DFBInputDeviceKeySymbol    symbol,
                               DFBInputDeviceModifierMask modifiers)
{
     DFBCHECK( iface->GrabKey (iface, symbol, modifiers) );
}

void IDirectFBWindow::UngrabKey (DFBInputDeviceKeySymbol    symbol,
                                 DFBInputDeviceModifierMask modifiers)
{
     DFBCHECK( iface->UngrabKey (iface, symbol, modifiers) );
}

void IDirectFBWindow::Move (int dx, int dy)
{
     DFBCHECK( iface->Move (iface, dx, dy) );
}

void IDirectFBWindow::MoveTo (int x, int y)
{
     DFBCHECK( iface->MoveTo (iface, x, y) );
}

void IDirectFBWindow::Resize (int width,
                              int height)
{
     DFBCHECK( iface->Resize (iface, width, height) );
}

void IDirectFBWindow::SetStackingClass (DFBWindowStackingClass stacking_class)
{
     DFBCHECK( iface->SetStackingClass (iface, stacking_class) );
}

void IDirectFBWindow::Raise()
{
     DFBCHECK( iface->Raise (iface) );
}

void IDirectFBWindow::Lower()
{
     DFBCHECK( iface->Lower (iface) );
}

void IDirectFBWindow::RaiseToTop()
{
     DFBCHECK( iface->RaiseToTop (iface) );
}

void IDirectFBWindow::LowerToBottom()
{
     DFBCHECK( iface->LowerToBottom (iface) );
}

void IDirectFBWindow::PutAtop (IDirectFBWindow *lower)
{
     DFBCHECK( iface->PutAtop (iface, lower->iface) );
}

void IDirectFBWindow::PutBelow (IDirectFBWindow *upper)
{
     DFBCHECK( iface->PutBelow (iface, upper->iface) );
}

void IDirectFBWindow::Close()
{
     DFBCHECK( iface->Close (iface) );
}

void IDirectFBWindow::Destroy()
{
     DFBCHECK( iface->Destroy (iface) );
}

void IDirectFBWindow::SetBounds (int x,
                                 int y,
                                 int width,
                                 int height)
{
     DFBCHECK( iface->SetBounds (iface, x, y, width, height) );
}

void IDirectFBWindow::ResizeSurface (int width,
                                     int height)
{
     DFBCHECK( iface->ResizeSurface (iface, width, height) );
}

