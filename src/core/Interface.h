/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#ifndef __CORE_INTERFACE_H__
#define __CORE_INTERFACE_H__

extern "C" {
#include <core/core.h>
}

namespace DirectFB {


class Interface {
protected:
     CoreDFB *core;

     Interface( CoreDFB *core )
          :
          core( core )
     {
     }
};


class CallBuffer {
private:
     int magic;

public:
     CallBuffer( size_t buffer_min );
     virtual ~CallBuffer();

     void *prepare( int    method,
                    size_t len );
     void  commit ( size_t len = 0 );

     DFBResult flush();

protected:
     void   *buffer;
     size_t  buffer_min;
     size_t  buffer_size;
     size_t  buffer_len;
     size_t  buffer_prepared;

     virtual DFBResult flushCalls() = 0;
};



}


#endif

