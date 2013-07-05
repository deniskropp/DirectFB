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


#include <stddef.h>
#include <string.h>

#include "++dfb.h"
#include "++dfb_internal.h"

DFBException::DFBException (const char *action, DFBResult result_code)
{
     const char *tmp = action;
     
     while (*tmp != 0) {
          if (!strncmp (tmp, " IDirectFB", 10)) {
               action = tmp + 1;
               break;
          }

          tmp++;
     }

     this->action      = action;
     this->result_code = result_code;

     std::cerr << this << std::endl;
}

const char *DFBException::GetAction() const
{
     return action;
}

const char *DFBException::GetResult() const
{
     return DirectFBErrorString (result_code);
}

DFBResult DFBException::GetResultCode() const
{
     return result_code;
}

std::ostream &operator << (std::ostream &stream, DFBException *ex)
{
     stream << ex->GetAction() << " -> " << ex->GetResult();

     return stream;
}


namespace DirectFB {


void Init (int *argc, char *(*argv[]))
{
     DFBCHECK( DirectFBInit (argc, argv) );
}

IDirectFB Create ()
{
     IDirectFB_C *idirectfb;

     DFBCHECK( DirectFBCreate (&idirectfb) );

     return IDirectFB (idirectfb);
}

}

