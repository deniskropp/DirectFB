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

DFBScreenID IDirectFBScreen::GetID()
{
     DFBScreenID screen_id;

     DFBCHECK( iface->GetID (iface, &screen_id) );

     return screen_id;
}

DFBScreenDescription IDirectFBScreen::GetDescription()
{
     DFBScreenDescription desc;

     DFBCHECK( iface->GetDescription (iface, &desc) );

     return desc;
}

void IDirectFBScreen::EnumDisplayLayers (DFBDisplayLayerCallback  callback,
                                         void                    *callbackdata)
{
     DFBCHECK( iface->EnumDisplayLayers (iface, callback, callbackdata) );
}

void IDirectFBScreen::WaitForSync()
{
     DFBCHECK( iface->WaitForSync (iface) );
}

void IDirectFBScreen::SetPowerMode (DFBScreenPowerMode mode)
{
     DFBCHECK( iface->SetPowerMode (iface, mode) );
}

