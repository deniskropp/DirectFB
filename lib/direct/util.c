/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <string.h>

#include <fcntl.h>

#include <direct/messages.h>
#include <direct/util.h>


int
direct_try_open( const char *name1, const char *name2, int flags )
{
     int fd;

     fd = open (name1, flags);
     if (fd >= 0)
          return fd;

     if (errno != ENOENT) {
          D_PERROR( "Direct/Util: opening '%s' failed\n", name1 );
          return -1;
     }

     fd = open (name2, flags);
     if (fd >= 0)
          return fd;

     if (errno == ENOENT)
          D_PERROR( "Direct/Util: opening '%s' and '%s' failed\n", name1, name2 );
     else
          D_PERROR( "Direct/Util: opening '%s' failed\n", name2 );

     return -1;
}

void
direct_trim( char **s )
{
     int i;
     int len = strlen( *s );

     for (i = len-1; i >= 0; i--)
          if ((*s)[i] <= ' ')
               (*s)[i] = 0;
          else
               break;

     while (**s)
          if (**s <= ' ')
               (*s)++;
          else
               return;
}

