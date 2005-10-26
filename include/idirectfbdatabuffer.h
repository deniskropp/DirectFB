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

#ifndef IDIRECTFBDATABUFFER_H
#define IDIRECTFBDATABUFFER_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBDataBuffer : public IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C> {
friend
     class IDirectFB;

public:
     IDirectFBDataBuffer(IDirectFBDataBuffer_C *myptr=NULL):IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>(myptr){}

     void                    Flush                   ();
     void                    Finish                  ();
     void                    SeekTo                  (unsigned int  offset);
     unsigned int            GetPosition             ();
     unsigned int            GetLength               ();

     void                    WaitForData             (unsigned int  length);
     void                    WaitForDataWithTimeout  (unsigned int  length,
                                                      unsigned int  seconds,
                                                      unsigned int  milli_seconds);

     unsigned int            GetData                 (unsigned int  length,
                                                      void         *data);
     unsigned int            PeekData                (unsigned int  length,
                                                      int           offset,
                                                      void         *data);

     bool                    HasData                 ();

     void                    PutData                 (const void   *data,
                                                      unsigned int  length);

     IDirectFBImageProvider  CreateImageProvider     ();
     IDirectFBVideoProvider  CreateVideoProvider     ();

     inline IDirectFBDataBuffer& operator = (const IDirectFBDataBuffer& other){
          return IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>::operator =(other);
     }
     inline IDirectFBDataBuffer& operator = (IDirectFBDataBuffer_C* other){
          return IPPAny<IDirectFBDataBuffer, IDirectFBDataBuffer_C>::operator =(other);
     }

};

#endif
