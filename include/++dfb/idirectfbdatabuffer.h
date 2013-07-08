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


#ifndef IDIRECTFBDATABUFFER_H
#define IDIRECTFBDATABUFFER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBDataBuffer : public IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C> {
friend
     class IDirectFB;

public:
     PPDFB_API IDirectFBDataBuffer(IDirectFBDataBuffer_C *myptr=NULL):IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>(myptr){}

     void                    PPDFB_API Flush                   ();
     void                    PPDFB_API Finish                  ();
     void                    PPDFB_API SeekTo                  (unsigned int  offset);
     unsigned int            PPDFB_API GetPosition             ();
     unsigned int            PPDFB_API GetLength               ();

     void                    PPDFB_API WaitForData             (unsigned int  length);
     void                    PPDFB_API WaitForDataWithTimeout  (unsigned int  length,
                                                                unsigned int  seconds,
                                                                unsigned int  milli_seconds);

     unsigned int            PPDFB_API GetData                 (unsigned int  length,
                                                                void         *data);
     unsigned int            PPDFB_API PeekData                (unsigned int  length,
                                                                int           offset,
                                                                void         *data);

     bool                    PPDFB_API HasData                 ();

     void                    PPDFB_API PutData                 (const void   *data,
                                                                unsigned int  length);

     IDirectFBImageProvider  PPDFB_API CreateImageProvider     ();
     IDirectFBVideoProvider  PPDFB_API CreateVideoProvider     ();

     inline IDirectFBDataBuffer PPDFB_API & operator = (const IDirectFBDataBuffer& other){
          return IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>::operator =(other);
     }
     inline IDirectFBDataBuffer PPDFB_API & operator = (IDirectFBDataBuffer_C* other){
          return IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>::operator =(other);
     }

};

#endif
