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

DFBDisplayLayerID IDirectFBDisplayLayer::GetID()
{
     DFBDisplayLayerID layer_id;

     DFBCHECK( iface->GetID (iface, &layer_id) );

     return layer_id;
}

DFBDisplayLayerDescription IDirectFBDisplayLayer::GetDescription()
{
     DFBDisplayLayerDescription desc;

     DFBCHECK( iface->GetDescription (iface, &desc) );

     return desc;
}

void IDirectFBDisplayLayer::GetSourceDescriptions (DFBDisplayLayerSourceDescription *desc)
{
     DFBCHECK( iface->GetSourceDescriptions (iface, desc) );
}

IDirectFBSurface IDirectFBDisplayLayer::GetSurface()
{
     IDirectFBSurface_C *idirectfbsurface;

     DFBCHECK( iface->GetSurface (iface, &idirectfbsurface) );

     return IDirectFBSurface (idirectfbsurface);
}

IDirectFBScreen IDirectFBDisplayLayer::GetScreen()
{
     IDirectFBScreen_C *idirectfbscreen;

     DFBCHECK( iface->GetScreen (iface, &idirectfbscreen) );

     return IDirectFBScreen (idirectfbscreen);
}

void IDirectFBDisplayLayer::SetCooperativeLevel (DFBDisplayLayerCooperativeLevel level)
{
     DFBCHECK( iface->SetCooperativeLevel (iface, level) );
}

void IDirectFBDisplayLayer::SetOpacity (__u8 opacity)
{
     DFBCHECK( iface->SetOpacity (iface, opacity) );
}

void IDirectFBDisplayLayer::SetSourceRectangle (int x,
                                                int y,
                                                int width,
                                                int height)
{
     DFBCHECK( iface->SetSourceRectangle (iface, x, y, width, height) );
}

void IDirectFBDisplayLayer::SetScreenLocation (float x,
                                               float y,
                                               float width,
                                               float height)
{
     DFBCHECK( iface->SetScreenLocation (iface, x, y, width, height) );
}

void IDirectFBDisplayLayer::SetScreenPosition (int x,
                                               int y)
{
     DFBCHECK( iface->SetScreenPosition (iface, x, y) );
}

void IDirectFBDisplayLayer::SetScreenRectangle (int x,
                                                int y,
                                                int width,
                                                int height)
{
     DFBCHECK( iface->SetScreenRectangle (iface, x, y, width, height) );
}

void IDirectFBDisplayLayer::SetClipRegions (const DFBRegion *regions,
                                            int              num_regions,
                                            DFBBoolean       positive)
{
     DFBCHECK( iface->SetClipRegions (iface, regions, num_regions, positive) );
}

void IDirectFBDisplayLayer::SetSrcColorKey (__u8 r, __u8 g, __u8 b)
{
     DFBCHECK( iface->SetSrcColorKey (iface, r, g, b) );
}

void IDirectFBDisplayLayer::SetDstColorKey (__u8 r, __u8 g, __u8 b)
{
     DFBCHECK( iface->SetDstColorKey (iface, r, g, b) );
}

int IDirectFBDisplayLayer::GetLevel()
{
     int level;

     DFBCHECK( iface->GetLevel (iface, &level) );

     return level;
}

void IDirectFBDisplayLayer::SetLevel (int level)
{
     DFBCHECK( iface->SetLevel (iface, level) );
}

int IDirectFBDisplayLayer::GetCurrentOutputField()
{
     int field;

     DFBCHECK( iface->GetCurrentOutputField (iface, &field) );

     return field;
}

void IDirectFBDisplayLayer::SetFieldParity (int field)
{
     DFBCHECK( iface->SetFieldParity (iface, field) );
}

void IDirectFBDisplayLayer::WaitForSync()
{
     DFBCHECK( iface->WaitForSync (iface) );
}

void IDirectFBDisplayLayer::GetConfiguration (DFBDisplayLayerConfig *config)
{
     DFBCHECK( iface->GetConfiguration (iface, config) );
}

void IDirectFBDisplayLayer::TestConfiguration (DFBDisplayLayerConfig      &config,
                                               DFBDisplayLayerConfigFlags *failed)
{
     DFBCHECK( iface->TestConfiguration (iface, &config, failed) );
}

void IDirectFBDisplayLayer::SetConfiguration (DFBDisplayLayerConfig &config)
{
     DFBCHECK( iface->SetConfiguration (iface, &config) );
}

void IDirectFBDisplayLayer::SetBackgroundMode (DFBDisplayLayerBackgroundMode mode)
{
     DFBCHECK( iface->SetBackgroundMode (iface, mode) );
}

void IDirectFBDisplayLayer::SetBackgroundImage (IDirectFBSurface *surface)
{
     DFBCHECK( iface->SetBackgroundImage (iface, surface->get_iface()) );
}

void IDirectFBDisplayLayer::SetBackgroundColor (__u8 r, __u8 g, __u8 b, __u8 a)
{
     DFBCHECK( iface->SetBackgroundColor (iface, r, g, b, a) );
}

void IDirectFBDisplayLayer::GetColorAdjustment (DFBColorAdjustment *adj)
{
     DFBCHECK( iface->GetColorAdjustment (iface, adj) );
}

void IDirectFBDisplayLayer::SetColorAdjustment (DFBColorAdjustment &adj)
{
     DFBCHECK( iface->SetColorAdjustment (iface, &adj) );
}

IDirectFBWindow IDirectFBDisplayLayer::CreateWindow (DFBWindowDescription &desc)
{
     IDirectFBWindow_C *idirectfbwindow;

     DFBCHECK( iface->CreateWindow (iface, &desc, &idirectfbwindow) );

     return IDirectFBWindow (idirectfbwindow);
}

IDirectFBWindow IDirectFBDisplayLayer::GetWindow (DFBWindowID window_id)
{
     IDirectFBWindow_C *idirectfbwindow;

     DFBCHECK( iface->GetWindow (iface, window_id, &idirectfbwindow) );

     return IDirectFBWindow (idirectfbwindow);
}

void IDirectFBDisplayLayer::EnableCursor (bool enable)
{
     DFBCHECK( iface->EnableCursor (iface, enable) );
}

void IDirectFBDisplayLayer::GetCursorPosition (int *x, int *y)
{
     DFBCHECK( iface->GetCursorPosition (iface, x, y) );
}

void IDirectFBDisplayLayer::WarpCursor (int x, int y)
{
     DFBCHECK( iface->WarpCursor (iface, x, y) );
}

void IDirectFBDisplayLayer::SetCursorAcceleration (int numerator,
                                                   int denominator,
                                                   int threshold)
{
     DFBCHECK( iface->SetCursorAcceleration (iface, numerator, denominator, threshold) );
}

void IDirectFBDisplayLayer::SetCursorShape (IDirectFBSurface *shape,
                                            int               hot_x,
                                            int               hot_y)
{
     DFBCHECK( iface->SetCursorShape (iface, shape->get_iface(), hot_x, hot_y) );
}

void IDirectFBDisplayLayer::SetCursorOpacity (__u8 opacity)
{
     DFBCHECK( iface->SetCursorOpacity (iface, opacity) );
}

void IDirectFBDisplayLayer::SwitchContext (DFBBoolean exclusive)
{
     DFBCHECK( iface->SwitchContext (iface, exclusive) );
}

