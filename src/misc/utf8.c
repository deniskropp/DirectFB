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

#include "directfb.h"

#include "core/coretypes.h"

#include "utf8.h"


#define UTF8_COMPUTE(Char, Mask, Len)   \
  if ((Char & 0xe0) == 0xc0) {	        \
      Len = 2;			        \
      Mask = 0x1f;			\
  }                                     \
  else if ((Char & 0xf0) == 0xe0) {     \
      Len = 3;				\
      Mask = 0x0f;			\
  }                                     \
  else if ((Char & 0xf8) == 0xf0) {     \
      Len = 4;                          \
      Mask = 0x07;                      \
  }	                                \
  else if ((Char & 0xfc) == 0xf8) {     \
      Len = 5;	                        \
      Mask = 0x03;                      \
  }	                                \
  else if ((Char & 0xfe) == 0xfc) {     \
      Len = 6;	                        \
      Mask = 0x01;                      \
  }					\
  else                                  \
      Len = -1;

#define UTF8_GET(Result, Chars, Count, Mask, Len)	\
  (Result) = (Chars)[0] & (Mask);			\
  for ((Count) = 1; (Count) < (Len); ++(Count)) {       \
      if (((Chars)[(Count)] & 0xc0) != 0x80) {          \
	  (Result) = -1;				\
	  break;					\
      }			                                \
      (Result) <<= 6;					\
      (Result) |= ((Chars)[(Count)] & 0x3f);		\
  }

/* Actually the last two fields used to be zero. Changed it to 1
   to avoid endless looping on invalid utf8 strings  */
char dfb_utf8_skip[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

unichar dfb_utf8_get_char (const char *p)
{
     unichar result;
     unsigned char c = (unsigned char) *p;

     if (c & 0x80)
       {
         int i, mask = 0, len;

         UTF8_COMPUTE (c, mask, len);
         if (len == -1)
           return (unichar) -1;

         UTF8_GET (result, p, i, mask, len);
       }
     else
       result = (unichar) c;
         
     return result;
}
