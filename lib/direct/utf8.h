/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   UTF8 routines ported from glib-2.0 and optimized

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

#ifndef __DIRECT__UTF8_H__
#define __DIRECT__UTF8_H__

#include <direct/types.h>


#define DIRECT_UTF8_SKIP(c)     (((u8)(c) < 0xc0) ? 1 : __direct_utf8_skip[(u8)(c)&0x3f])

#define DIRECT_UTF8_GET_CHAR(p) (*(const u8*)(p) < 0xc0 ? \
                                 *(const u8*)(p) : __direct_utf8_get_char((const u8*)(p)))


/*
 *  Actually the last two fields used to be zero since they indicate an
 *  invalid UTF-8 string. Changed it to 1 to avoid endless looping on
 *  invalid input.
 */
static const char __direct_utf8_skip[64] = {
     2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
     3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

static __inline__ unichar __direct_utf8_get_char( const u8 *p )
{
     int              len;
     register unichar result = p[0];

     if (result < 0xc0)
          return result;

     if (result > 0xfd)
          return (unichar) -1;

     len = __direct_utf8_skip[result & 0x3f];

     result &= 0x7c >> len;

     while (--len) {
          int c = *(++p);

          if ((c & 0xc0) != 0x80)
               return (unichar) -1;

          result = (result << 6) | (c & 0x3f);
     }

     return result;
}

#endif
