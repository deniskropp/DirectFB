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

#ifndef IDIRECTFB_H
#define IDIRECTFB_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFB {
friend
     class DirectFB;

public:
     void                    SetCooperativeLevel    (DFBCooperativeLevel         level);
     void                    SetVideoMode           (unsigned int                width,
                                                     unsigned int                height,
                                                     unsigned int                bpp);

     void                    GetDeviceDescription   (DFBGraphicsDeviceDescription *desc);
     void                    EnumVideoModes         (DFBVideoModeCallback        callback,
                                                     void                       *callbackdata);

     IDirectFBSurface        CreateSurface          (DFBSurfaceDescription      &desc);
     IDirectFBPalette        CreatePalette          (DFBPaletteDescription      &desc);

     void                    EnumScreens            (DFBScreenCallback           callback,
                                                     void                       *callbackdata);
     IDirectFBScreen         GetScreen              (DFBScreenID                 screen_id);

     void                    EnumDisplayLayers      (DFBDisplayLayerCallback     callback,
                                                     void                       *callbackdata);
     IDirectFBDisplayLayer   GetDisplayLayer        (DFBDisplayLayerID           layer_id);

     void                    EnumInputDevices       (DFBInputDeviceCallback      callback,
                                                     void                       *callbackdata);
     IDirectFBInputDevice    GetInputDevice         (DFBInputDeviceID            device_id);
     IDirectFBEventBuffer    CreateEventBuffer      ();
     IDirectFBEventBuffer    CreateInputEventBuffer (DFBInputDeviceCapabilities caps,
                                                     DFBBoolean                 global = DFB_FALSE);

     IDirectFBImageProvider  CreateImageProvider    (const char                 *filename);
     IDirectFBVideoProvider  CreateVideoProvider    (const char                 *filename);
     IDirectFBFont           CreateFont             (const char                 *filename,
                                                     DFBFontDescription         &desc);
     IDirectFBDataBuffer     CreateDataBuffer       (DFBDataBufferDescription   &desc);

     struct timeval          SetClipboardData       (const char                 *mime_type,
                                                     const void                 *data,
                                                     unsigned int                size);
     void                    GetClipboardData       (char                      **mime_type,
                                                     void                      **data,
                                                     unsigned int               *size);
     struct timeval          GetClipboardTimeStamp  ();

     void                    Suspend                ();
     void                    Resume                 ();
     void                    WaitIdle               ();
     void                    WaitForSync            ();

     void                   *GetInterface           (const char                 *type,
                                                     const char                 *implementation,
                                                     void                       *arg);


     __DFB_PLUS_PLUS__INTERFACE_CLASS( IDirectFB );
};

#endif
