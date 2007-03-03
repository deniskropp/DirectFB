/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __DUFFS_DEVICE_H__
#define __DUFFS_DEVICE_H__

#define DUFF_1() \
               case 1:\
                    SET_PIXEL( D[0], S[0] );

#define DUFF_2() \
               case 3:\
                    SET_PIXEL( D[2], S[2] );\
               case 2:\
                    SET_PIXEL( D[1], S[1] );\
               DUFF_1()

#define DUFF_3() \
               case 7:\
                    SET_PIXEL( D[6], S[6] );\
               case 6:\
                    SET_PIXEL( D[5], S[5] );\
               case 5:\
                    SET_PIXEL( D[4], S[4] );\
               case 4:\
                    SET_PIXEL( D[3], S[3] );\
               DUFF_2()

#define DUFF_4() \
               case 15:\
                    SET_PIXEL( D[14], S[14] );\
               case 14:\
                    SET_PIXEL( D[13], S[13] );\
               case 13:\
                    SET_PIXEL( D[12], S[12] );\
               case 12:\
                    SET_PIXEL( D[11], S[11] );\
               case 11:\
                    SET_PIXEL( D[10], S[10] );\
               case 10:\
                    SET_PIXEL( D[9], S[9] );\
               case 9:\
                    SET_PIXEL( D[8], S[8] );\
               case 8:\
                    SET_PIXEL( D[7], S[7] );\
               DUFF_3()

#define SET_PIXEL_DUFFS_DEVICE_N( D, S, w, n ) \
do {\
     while (w) {\
          register int l = w & ((1 << n) - 1);\
          switch (l) {\
               default:\
                    l = (1 << n);\
                    SET_PIXEL( D[(1 << n)-1], S[(1 << n)-1] );\
               DUFF_##n()\
          }\
          D += l;\
          S += l;\
          w -= l;\
     }\
} while(0)

#endif
