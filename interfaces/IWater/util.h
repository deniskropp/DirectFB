/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
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

#ifndef __WATER_TEST__UTIL_H__
#define __WATER_TEST__UTIL_H__


#define TEST_ROUND20     0x80000
#define TEST_ROUND16     0x8000
#define TEST_ROUND8      0x80


#define TEST_SCALAR_TO_12_20(s,t)  (((t) == WST_INTEGER)     ? ((s).i << 20) :                           \
                                    ((t) == WST_FIXED_16_16) ? ((s).i <<  4) :                           \
                                    ((t) == WST_FLOAT)       ? ((s).f * (1 << 20)) : 0 )

#define TEST_SCALAR_TO_16_16(s,t)  (((t) == WST_INTEGER)     ? ((s).i << 16) :                           \
                                    ((t) == WST_FIXED_16_16) ? ((s).i      ) :                           \
                                    ((t) == WST_FLOAT)       ? ((int)((s).f * (1 << 16))) : 0 )

#define TEST_SCALAR_TO_FLOAT(s,t)  (((t) == WST_INTEGER)     ? ((s).i                 ) :                \
                                    ((t) == WST_FIXED_16_16) ? ((s).i / (float)(1<<16)) :                \
                                    ((t) == WST_FLOAT)       ? ((float)(s).f              ) : 0 )


#define TEST_FIXED_12_20_VALUES_07(v)   (((v) <= 0) ? '-' : ' '), (ABS(v) >> 20),                             \
                                        (((v) >= 0) ?                                                         \
                                                  (int)(9999999ULL * (  (v)  & 0xfffffULL) / 0xfffffULL) :    \
                                                  (int)(9999999ULL * ((-(v)) & 0xfffffULL) / 0xfffffULL))

#define TEST_FIXED_16_16_VALUES_05(v)   (((v) <= 0) ? '-' : ' '), (ABS(v) >> 16),                             \
                                        (((v) >= 0) ?                                                         \
                                                  (int)(99999ULL * (  (v)  & 0xffffULL) / 0xffffULL) :        \
                                                  (int)(99999ULL * ((-(v)) & 0xffffULL) / 0xffffULL))

#define TEST_FIXED_24_8_VALUES_03(v)    (((v) <= 0) ? '-' : ' '), (ABS(v) >>  8),                             \
                                        (((v) >= 0) ?                                                         \
                                                  (int)(999ULL * (  (v)  & 0xffULL) / 0xffULL) :              \
                                                  (int)(999ULL * ((-(v)) & 0xffULL) / 0xffULL))


#define TEST_MULT2_ADD_12_20(a,b,c,d)   ((int)(((long long)(a) * (long long)(b) +                        \
                                                (long long)(c) * (long long)(d) + TEST_ROUND20) >> 20))

#define TEST_MULT2_ADD_16_16(a,b,c,d)   ((int)(((long long)(a) * (long long)(b) +                        \
                                                (long long)(c) * (long long)(d) + TEST_ROUND16) >> 16))

#define TEST_MULT2_ADD_24_8(a,b,c,d)    ((int)(((long long)(a) * (long long)(b) +                        \
                                                (long long)(c) * (long long)(d) + TEST_ROUND8) >> 8))


#define TEST_ANY_TRANSFORM(t)      ( (((t)->flags & WTF_TYPE) && (t)->type && (t)->type != WTT_IDENTITY) ||   \
                                     (((t)->flags & WTF_MATRIX)) )

#define TEST_NONRECT_TRANSFORM(t)  ( (((t)->flags & WTF_TYPE) && ((t)->type & (WTT_SKEW_X|WTT_SKEW_Y|WTT_ROTATE_FREE))) ||   \
                                     (((t)->flags & WTF_MATRIX) && ((t)->matrix[1].i || (t)->matrix[3].i)) )



#endif
