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
     class IDirectFBSurface;
friend
     class IDirectFBWindow;

public:
     PPDFB_API IDirectFBEventBuffer(IDirectFBEventBuffer_C* myptr=NULL):IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>(myptr){}

     void                  PPDFB_API Reset                    ();

     void                  PPDFB_API WaitForEvent             ();
     bool                  PPDFB_API WaitForEventWithTimeout  (unsigned int  seconds,
                                                               unsigned int  milli_seconds);

     void                  PPDFB_API WakeUp                   ();

     bool                  PPDFB_API GetEvent                 (DFBEvent     *event);
     bool                  PPDFB_API PeekEvent                (DFBEvent     *event);
     bool                  PPDFB_API HasEvent                 ();

     void                  PPDFB_API PostEvent                (DFBEvent     &event);

     int                   PPDFB_API CreateFileDescriptor     ();

     void                  PPDFB_API EnableStatistics         (DFBBoolean           enable);
     void                  PPDFB_API GetStatistics            (DFBEventBufferStats *stats);
     
     inline IDirectFBEventBuffer PPDFB_API & operator = (const IDirectFBEventBuffer& other){
          return IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>::operator =(other);
     }
     inline IDirectFBEventBuffer PPDFB_API & operator = (IDirectFBEventBuffer_C* other){
          return IPPAny<IDirectFBEventBuffer, IDirectFBEventBuffer_C>::operator =(other);
     }

};

#endif
