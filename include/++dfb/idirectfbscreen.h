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


#ifndef IDIRECTFBSCREEN_H
#define IDIRECTFBSCREEN_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBScreen : public IPPAny<IDirectFBScreen, IDirectFBScreen_C> {
friend
     class IDirectFB;
friend
     class IDirectFBDisplayLayer;

public:
     PPDFB_API IDirectFBScreen(IDirectFBScreen_C* myptr=NULL):IPPAny<IDirectFBScreen, IDirectFBScreen_C>(myptr){}

     DFBScreenID            PPDFB_API GetID                 ();

     DFBScreenDescription   PPDFB_API GetDescription        ();

     void                   PPDFB_API GetSize               (int                        *width,
                                                             int                        *height);

     void                   PPDFB_API EnumDisplayLayers     (DFBDisplayLayerCallback     callback,
                                                             void                       *callbackdata);

     void                   PPDFB_API SetPowerMode          (DFBScreenPowerMode          mode);

     void                   PPDFB_API WaitForSync           ();


     void                   PPDFB_API GetMixerDescriptions     (DFBScreenMixerDescription    *descriptions);

     void                   PPDFB_API GetMixerConfiguration    (int                           mixer,
                                                                DFBScreenMixerConfig         *config);

     void                   PPDFB_API TestMixerConfiguration   (int                           mixer,
                                                                const DFBScreenMixerConfig   &config,
                                                                DFBScreenMixerConfigFlags    *failed);

     void                   PPDFB_API SetMixerConfiguration    (int                           mixer,
                                                                const DFBScreenMixerConfig   &config);


     void                   PPDFB_API GetEncoderDescriptions   (DFBScreenEncoderDescription  *descriptions);

     void                   PPDFB_API GetEncoderConfiguration  (int                           encoder,
                                                                DFBScreenEncoderConfig       *config);

     void                   PPDFB_API TestEncoderConfiguration (int                           encoder,
                                                                const DFBScreenEncoderConfig &config,
                                                                DFBScreenEncoderConfigFlags  *failed);

     void                   PPDFB_API SetEncoderConfiguration  (int                           encoder,
                                                                const DFBScreenEncoderConfig &config);


     void                   PPDFB_API GetOutputDescriptions    (DFBScreenOutputDescription   *descriptions);

     void                   PPDFB_API GetOutputConfiguration   (int                           output,
                                                                DFBScreenOutputConfig        *config);

     void                   PPDFB_API TestOutputConfiguration  (int                           output,
                                                                const DFBScreenOutputConfig  &config,
                                                                DFBScreenOutputConfigFlags   *failed);

     void                   PPDFB_API SetOutputConfiguration   (int                           output,
                                                                const DFBScreenOutputConfig  &config);

     inline IDirectFBScreen PPDFB_API & operator = (const IDirectFBScreen& other){
          return IPPAny<IDirectFBScreen, IDirectFBScreen_C>::operator =(other);
     }
     inline IDirectFBScreen PPDFB_API & operator = (IDirectFBScreen_C* other){
          return IPPAny<IDirectFBScreen, IDirectFBScreen_C>::operator =(other);
     }

};

#endif
