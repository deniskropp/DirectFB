/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   UTF8 routines ported from glib-2.0

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

#ifndef __UTF8_H__
#define __UTF8_H__

#include <core/coretypes.h>

extern const char dfb_utf8_skip[256];

#define dfb_utf8_next_char(p) \
     (char *)((p) + dfb_utf8_skip[*(unsigned char *)(p)])

static inline char *dfb_utf8_prev_char (const char *p)
{
     while ((*(--p) & 0xc0) == 0x80)
          ;
     return (char *)p;
}

unichar dfb_utf8_get_char (const char *p);

#endif
