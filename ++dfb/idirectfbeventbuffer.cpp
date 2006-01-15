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

void IDirectFBEventBuffer::Reset()
{
     DFBCHECK( iface->Reset (iface) );
}

void IDirectFBEventBuffer::WaitForEvent()
{
     DFBCHECK( iface->WaitForEvent (iface) );
}

bool IDirectFBEventBuffer::WaitForEventWithTimeout (unsigned int seconds,
                                                    unsigned int milli_seconds)
{
     DFBResult ret;

     ret = iface->WaitForEventWithTimeout (iface, seconds, milli_seconds);

     if (ret != DFB_OK  &&  ret != DFB_TIMEOUT)
          throw new DFBException(__PRETTY_FUNCTION__, ret);

     return (ret == DFB_OK);
}

void IDirectFBEventBuffer::WakeUp ()
{
     DFBCHECK( iface->WakeUp (iface) );
}

bool IDirectFBEventBuffer::GetEvent (DFBEvent *event)
{
     DFBResult ret;

     ret = iface->GetEvent (iface, event);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return (ret == DFB_OK);
}

bool IDirectFBEventBuffer::PeekEvent (DFBEvent *event)
{
     DFBResult ret;

     ret = iface->PeekEvent (iface, event);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return (ret == DFB_OK);
}

bool IDirectFBEventBuffer::HasEvent ()
{
     DFBResult ret;

     ret = iface->HasEvent (iface);

     if (ret != DFB_OK  &&  ret != DFB_BUFFEREMPTY)
          throw new DFBException (__PRETTY_FUNCTION__, ret);

     return (ret == DFB_OK);
}

void IDirectFBEventBuffer::PostEvent (DFBEvent &event)
{
     DFBCHECK( iface->PostEvent (iface, &event) );
}

int IDirectFBEventBuffer::CreateFileDescriptor ()
{
     int fd;

     DFBCHECK( iface->CreateFileDescriptor (iface, &fd) );

     return fd;
}

void IDirectFBEventBuffer::EnableStatistics (DFBBoolean enable)
{
     DFBCHECK( iface->EnableStatistics (iface, enable) );
}

void IDirectFBEventBuffer::GetStatistics (DFBEventBufferStats *stats)
{
     DFBCHECK( iface->GetStatistics (iface, stats) );
}
