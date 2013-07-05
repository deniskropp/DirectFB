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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <string.h>

#include <arpa/inet.h>

#include <direct/util.h>

/**********************************************************************************************************************/

int
direct_strcasecmp( const char *a, const char *b )
{
     return strcasecmp( a, b );
}

int
direct_strncasecmp( const char *a, const char *b, size_t bytes )
{
     return strncasecmp( a, b, bytes );
}

int
direct_vsscanf( const char *str, const char *format, va_list args )
{
     return vsscanf( str, format, args );
}

const char *
direct_inet_ntop( int af, const void* src, char* dst, int cnt )
{
     return inet_ntop( af, src, dst, cnt );
}
