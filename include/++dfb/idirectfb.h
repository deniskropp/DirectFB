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


#ifndef IDIRECTFB_H
#define IDIRECTFB_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFB :public IPPAny<IDirectFB, IDirectFB_C>{
public:
     PPDFB_API IDirectFB(IDirectFB_C *myptr = NULL):IPPAny<IDirectFB, IDirectFB_C>(myptr){}
     PPDFB_API ~IDirectFB(){}
     void                    PPDFB_API SetCooperativeLevel    (DFBCooperativeLevel         level);
     void                    PPDFB_API SetVideoMode           (unsigned int                width,
                                                               unsigned int                height,
                                                               unsigned int                bpp);

     void                    PPDFB_API GetDeviceDescription   (DFBGraphicsDeviceDescription *desc);
     void                    PPDFB_API EnumVideoModes         (DFBVideoModeCallback        callback,
                                                               void                       *callbackdata);

     IDirectFBSurface        PPDFB_API CreateSurface          (DFBSurfaceDescription      &desc) const;
     IDirectFBPalette        PPDFB_API CreatePalette          (DFBPaletteDescription      &desc);

     void                    PPDFB_API EnumScreens            (DFBScreenCallback           callback,
                                                               void                       *callbackdata);
     IDirectFBScreen         PPDFB_API GetScreen              (DFBScreenID                 screen_id);

     void                    PPDFB_API EnumDisplayLayers      (DFBDisplayLayerCallback     callback,
                                                               void                       *callbackdata);
     IDirectFBDisplayLayer   PPDFB_API GetDisplayLayer        (DFBDisplayLayerID           layer_id);

     void                    PPDFB_API EnumInputDevices       (DFBInputDeviceCallback      callback,
                                                               void                       *callbackdata) const;
     IDirectFBInputDevice    PPDFB_API GetInputDevice         (DFBInputDeviceID            device_id) const;
     IDirectFBEventBuffer    PPDFB_API CreateEventBuffer      () const;
     IDirectFBEventBuffer    PPDFB_API CreateInputEventBuffer (DFBInputDeviceCapabilities caps,
                                                               DFBBoolean                 global = DFB_FALSE);

     IDirectFBImageProvider  PPDFB_API CreateImageProvider    (const char                 *filename) const;
     IDirectFBVideoProvider  PPDFB_API CreateVideoProvider    (const char                 *filename);
     IDirectFBFont           PPDFB_API CreateFont             (const char                 *filename,
                                                               DFBFontDescription         &desc) const ;
     IDirectFBDataBuffer     PPDFB_API CreateDataBuffer       (DFBDataBufferDescription   &desc);

     struct timeval          PPDFB_API SetClipboardData       (const char                 *mime_type,
                                                               const void                 *data,
                                                               unsigned int                size);
     void                    PPDFB_API GetClipboardData       (char                      **mime_type,
                                                               void                      **data,
                                                               unsigned int               *size);
     struct timeval          PPDFB_API GetClipboardTimeStamp  ();

     void                    PPDFB_API Suspend                ();
     void                    PPDFB_API Resume                 ();
     void                    PPDFB_API WaitIdle               ();
     void                    PPDFB_API WaitForSync            ();

     void                   PPDFB_API *GetInterface           (const char                 *type,
                                                               const char                 *implementation,
                                                               void                       *arg);

     IDirectFBSurface        PPDFB_API GetSurface             (DFBSurfaceID                surface_id) const;

     inline IDirectFB PPDFB_API & operator = (const IDirectFB& other){
          return IPPAny<IDirectFB, IDirectFB_C>::operator =(other);
     }
     inline IDirectFB PPDFB_API & operator = (IDirectFB_C* other){
          return IPPAny<IDirectFB, IDirectFB_C>::operator =(other);
     }
};

#endif
