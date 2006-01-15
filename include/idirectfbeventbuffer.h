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

#ifndef IDIRECTFBEVENTBUFFER_H
#define IDIRECTFBEVENTBUFFER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBEventBuffer : public IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C> {
friend
     class IDirectFB;
friend
     class IDirectFBInputDevice;
friend
     class IDirectFBWindow;

public:
     IDirectFBEventBuffer(IDirectFBEventBuffer_C* myptr=NULL):IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>(myptr){}

     void                  Reset                    ();

     void                  WaitForEvent             ();
     bool                  WaitForEventWithTimeout  (unsigned int  seconds,
                                                     unsigned int  milli_seconds);

     void                  WakeUp                   ();

     bool                  GetEvent                 (DFBEvent     *event);
     bool                  PeekEvent                (DFBEvent     *event);
     bool                  HasEvent                 ();

     void                  PostEvent                (DFBEvent     &event);

     int                   CreateFileDescriptor     ();

     void                  EnableStatistics         (DFBBoolean           enable);
     void                  GetStatistics            (DFBEventBufferStats *stats);
     
     inline IDirectFBEventBuffer& operator = (const IDirectFBEventBuffer& other){
          return IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>::operator =(other);
     }
     inline IDirectFBEventBuffer& operator = (IDirectFBEventBuffer_C* other){
          return IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>::operator =(other);
     }

};

#endif
