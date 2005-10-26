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

void IDirectFBDataBuffer::Flush()
{
     DFBCHECK( iface->Flush (iface) );
}

void IDirectFBDataBuffer::Finish()
{
     DFBCHECK( iface->Finish (iface) );
}

void IDirectFBDataBuffer::SeekTo (unsigned int offset)
{
     DFBCHECK( iface->SeekTo (iface, offset) );
}

unsigned int IDirectFBDataBuffer::GetPosition ()
{
     unsigned int position;

     DFBCHECK( iface->GetPosition (iface, &position) );

     return position;
}

unsigned int IDirectFBDataBuffer::GetLength ()
{
     unsigned int length;

     DFBCHECK( iface->GetLength (iface, &length) );

     return length;
}

void IDirectFBDataBuffer::WaitForData (unsigned int length)
{
     DFBCHECK( iface->WaitForData (iface, length) );
}

void IDirectFBDataBuffer::WaitForDataWithTimeout (unsigned int length,
                                                  unsigned int seconds,
                                                  unsigned int milli_seconds)
{
     DFBCHECK( iface->WaitForDataWithTimeout (iface, length, seconds, milli_seconds) );
}

unsigned int IDirectFBDataBuffer::GetData (unsigned int  length,
                                           void         *data)
{
     DFBResult ret;
     unsigned int read_length = 0;

     ret = iface->GetData (iface, length, data, &read_length);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return read_length;
}

unsigned int IDirectFBDataBuffer::PeekData (unsigned int  length,
                                            int           offset,
                                            void         *data)
{
     DFBResult ret;
     unsigned int read_length = 0;

     ret = iface->PeekData (iface, length, offset, data, &read_length);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return read_length;
}

bool IDirectFBDataBuffer::HasData ()
{
     DFBResult ret;

     ret = iface->HasData (iface);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return (ret == DFB_OK);
}

void IDirectFBDataBuffer::PutData (const void   *data,
                                   unsigned int  length)
{
     DFBCHECK( iface->PutData (iface, data, length) );
}

IDirectFBImageProvider IDirectFBDataBuffer::CreateImageProvider ()
{
     IDirectFBImageProvider_C *idirectfbimageprovider;

     DFBCHECK( iface->CreateImageProvider (iface, &idirectfbimageprovider) );

     return IDirectFBImageProvider (idirectfbimageprovider);
}

IDirectFBVideoProvider IDirectFBDataBuffer::CreateVideoProvider ()
{
     IDirectFBVideoProvider_C *idirectfbvideoprovider;

     DFBCHECK( iface->CreateVideoProvider (iface, &idirectfbvideoprovider) );

     return IDirectFBVideoProvider (idirectfbvideoprovider);
}

