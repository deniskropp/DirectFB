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

void IDirectFB::SetCooperativeLevel (DFBCooperativeLevel level)
{
     DFBCHECK( iface->SetCooperativeLevel (iface, level) );
}

void IDirectFB::SetVideoMode (unsigned int width,
                              unsigned int height,
                              unsigned int bpp)
{
     DFBCHECK( iface->SetVideoMode (iface, width, height, bpp) );
}

void IDirectFB::GetDeviceDescription (DFBGraphicsDeviceDescription *desc)
{
     DFBCHECK( iface->GetDeviceDescription (iface, desc) );
}

void IDirectFB::EnumVideoModes (DFBVideoModeCallback  callback,
                                void                 *callbackdata)
{
     DFBCHECK( iface->EnumVideoModes (iface, callback, callbackdata) );
}

IDirectFBSurface IDirectFB::CreateSurface (DFBSurfaceDescription &desc) const
{
     IDirectFBSurface_C *idirectfbsurface;

     DFBCHECK( iface->CreateSurface (iface, &desc, &idirectfbsurface) );

     return IDirectFBSurface (idirectfbsurface);
}

IDirectFBPalette IDirectFB::CreatePalette (DFBPaletteDescription &desc)
{
     IDirectFBPalette_C *idirectfbpalette;

     DFBCHECK( iface->CreatePalette (iface, &desc, &idirectfbpalette) );

     return IDirectFBPalette (idirectfbpalette);
}

void IDirectFB::EnumDisplayLayers (DFBDisplayLayerCallback  callback,
                                   void                    *callbackdata)
{
     DFBCHECK( iface->EnumDisplayLayers (iface, callback, callbackdata) );
}


IDirectFBScreen IDirectFB::GetScreen (DFBScreenID screen_id)
{
     IDirectFBScreen_C *idirectfbscreen;

     DFBCHECK( iface->GetScreen (iface, screen_id, &idirectfbscreen) );

     return IDirectFBScreen (idirectfbscreen);
}

void IDirectFB::EnumScreens (DFBScreenCallback  callback,
                             void              *callbackdata)
{
     DFBCHECK( iface->EnumScreens (iface, callback, callbackdata) );
}


IDirectFBDisplayLayer IDirectFB::GetDisplayLayer (DFBDisplayLayerID layer_id)
{
     IDirectFBDisplayLayer_C *idirectfbdisplaylayer;

     DFBCHECK( iface->GetDisplayLayer (iface, layer_id, &idirectfbdisplaylayer) );

     return IDirectFBDisplayLayer (idirectfbdisplaylayer);
}

void IDirectFB::EnumInputDevices (DFBInputDeviceCallback  callback,
                                  void                   *callbackdata) const
{
     DFBCHECK( iface->EnumInputDevices (iface, callback, callbackdata) );
}

IDirectFBInputDevice IDirectFB::GetInputDevice (DFBInputDeviceID device_id) const
{
     IDirectFBInputDevice_C *idirectfbinputdevice;

     DFBCHECK( iface->GetInputDevice (iface, device_id, &idirectfbinputdevice) );

     return IDirectFBInputDevice (idirectfbinputdevice);
}

IDirectFBEventBuffer IDirectFB::CreateEventBuffer () const
{
     IDirectFBEventBuffer_C *idirectfbeventbuffer;

     DFBCHECK( iface->CreateEventBuffer (iface, &idirectfbeventbuffer) );

     return IDirectFBEventBuffer (idirectfbeventbuffer);
}

IDirectFBEventBuffer IDirectFB::CreateInputEventBuffer (DFBInputDeviceCapabilities caps,
                                                        DFBBoolean                 global)
{
     IDirectFBEventBuffer_C *idirectfbeventbuffer;

     DFBCHECK( iface->CreateInputEventBuffer (iface, caps, global,
                                              &idirectfbeventbuffer) );

     return IDirectFBEventBuffer (idirectfbeventbuffer);
}

IDirectFBImageProvider IDirectFB::CreateImageProvider (const char *filename) const
{
     IDirectFBImageProvider_C *idirectfbimageprovider;

     DFBCHECK( iface->CreateImageProvider (iface, filename, &idirectfbimageprovider) );

     return IDirectFBImageProvider (idirectfbimageprovider);
}

IDirectFBVideoProvider IDirectFB::CreateVideoProvider (const char *filename)
{
     IDirectFBVideoProvider_C *idirectfbvideoprovider;

     DFBCHECK( iface->CreateVideoProvider (iface, filename, &idirectfbvideoprovider) );

     return IDirectFBVideoProvider (idirectfbvideoprovider);
}

IDirectFBFont IDirectFB::CreateFont (const char         *filename,
                                      DFBFontDescription &desc) const
{
     IDirectFBFont_C *idirectfbfont;

     DFBCHECK( iface->CreateFont (iface, filename, &desc, &idirectfbfont) );

     return IDirectFBFont (idirectfbfont);
}

IDirectFBDataBuffer IDirectFB::CreateDataBuffer (DFBDataBufferDescription &desc)
{
     IDirectFBDataBuffer_C *idirectfbdatabuffer;

     DFBCHECK( iface->CreateDataBuffer (iface, &desc, &idirectfbdatabuffer) );

     return IDirectFBDataBuffer (idirectfbdatabuffer);
}

struct timeval IDirectFB::SetClipboardData (const char   *mime_type,
                                            const void   *data,
                                            unsigned int  size)
{
     struct timeval timestamp;

     DFBCHECK( iface->SetClipboardData (iface, mime_type, data, size, &timestamp) );

     return timestamp;
}

void IDirectFB::GetClipboardData (char         **mime_type,
                                  void         **data,
                                  unsigned int  *size)
{
     DFBCHECK( iface->GetClipboardData (iface, mime_type, data, size) );
}

struct timeval IDirectFB::GetClipboardTimeStamp()
{
     struct timeval timestamp;

     DFBCHECK( iface->GetClipboardTimeStamp (iface, &timestamp) );

     return timestamp;
}

void IDirectFB::Suspend()
{
     DFBCHECK( iface->Suspend (iface) );
}

void IDirectFB::Resume()
{
     DFBCHECK( iface->Resume (iface) );
}

void IDirectFB::WaitIdle()
{
     DFBCHECK( iface->WaitIdle (iface) );
}

void IDirectFB::WaitForSync()
{
     DFBCHECK( iface->WaitForSync (iface) );
}

void *IDirectFB::GetInterface (const char *type,
                               const char *implementation,
                               void       *arg)
{
     void *interface;

     DFBCHECK( iface->GetInterface (iface, type, implementation, arg, &interface) );

     return interface;
}

