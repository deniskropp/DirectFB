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
     IDirectFBScreen(IDirectFBScreen_C* myptr=NULL):IPPAny<IDirectFBScreen, IDirectFBScreen_C>(myptr){}

     DFBScreenID            GetID                 ();

     DFBScreenDescription   GetDescription        ();

     void                   EnumDisplayLayers     (DFBDisplayLayerCallback     callback,
                                                   void                       *callbackdata);

     void                   SetPowerMode          (DFBScreenPowerMode          mode);

     void                   WaitForSync           ();


     void                   GetMixerDescriptions     (DFBScreenMixerDescription    *descriptions);

     void                   GetMixerConfiguration    (int                           mixer,
                                                      DFBScreenMixerConfig         *config);

     void                   TestMixerConfiguration   (int                           mixer,
                                                      const DFBScreenMixerConfig   &config,
                                                      DFBScreenMixerConfigFlags    *failed);

     void                   SetMixerConfiguration    (int                           mixer,
                                                      const DFBScreenMixerConfig   &config);


     void                   GetEncoderDescriptions   (DFBScreenEncoderDescription  *descriptions);

     void                   GetEncoderConfiguration  (int                           encoder,
                                                      DFBScreenEncoderConfig       *config);

     void                   TestEncoderConfiguration (int                           encoder,
                                                      const DFBScreenEncoderConfig &config,
                                                      DFBScreenEncoderConfigFlags  *failed);

     void                   SetEncoderConfiguration  (int                           encoder,
                                                      const DFBScreenEncoderConfig &config);


     void                   GetOutputDescriptions    (DFBScreenOutputDescription   *descriptions);

     void                   GetOutputConfiguration   (int                           output,
                                                      DFBScreenOutputConfig        *config);

     void                   TestOutputConfiguration  (int                           output,
                                                      const DFBScreenOutputConfig  &config,
                                                      DFBScreenOutputConfigFlags   *failed);

     void                   SetOutputConfiguration   (int                           output,
                                                      const DFBScreenOutputConfig  &config);

     inline IDirectFBScreen& operator = (const IDirectFBScreen& other){
          return IPPAny<IDirectFBScreen, IDirectFBScreen_C>::operator =(other);
     }
     inline IDirectFBScreen& operator = (IDirectFBScreen_C* other){
          return IPPAny<IDirectFBScreen, IDirectFBScreen_C>::operator =(other);
     }

};

#endif
