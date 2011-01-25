/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__UUID_H__
#define __DIRECT__UUID_H__

#include <direct/types.h>


/*
 * Direct UUID
 */
typedef union {
     struct {
          u32  ___u32_x1[1];
          u16  ___u16_x2[2];
          u8   ____u8_x8[8];
     };

//     struct {
          u64  ___u64_x2[2];
//     };

//     struct {
          u32  ___u32_x4[4];
//     };

//     struct {
          u16  ___u16_x8[8];
//     };

//     struct {
          u8   __u8_x16[16];
//     };
} DirectUUID;

/**********************************************************************************************************************/

/*
 * We're using 64bit hash keys...
 *
 *
 *   u64 value = 2166136261UL;
 *
 *   for (i=0; i<4; i++) {
 *        value ^= (uuid)->___u32_x4[i];
 *        value *= 16777619ULL;
 *   }
 *
 */
#define D_UUID_HASH( uuid )         ((u64         )( ((( ((( ((( (((2166136261ULL                             \
                                                                    ^ (uuid)->___u32_x4[0])) * 16777619ULL)   \
                                                                ^ (uuid)->___u32_x4[1])) * 16777619ULL)       \
                                                            ^ (uuid)->___u32_x4[2])) * 16777619ULL)           \
                                                        ^ (uuid)->___u32_x4[3])) * 16777619ULL) ))

/**********************************************************************************************************************/

#define D_UUID_EQUAL( u1, u2 )                                                                                \
          ( (u1) == (u2) ||                                                                                   \
            (((u1)->___u64_x2[0] == (u2)->___u64_x2[0]) && ((u1)->___u64_x2[1] == (u2)->___u64_x2[1])) )

#define D_UUID_EMPTY( uuid )                                                                                  \
          ( (uuid)->___u64_x2[0] == 0 && (uuid)->___u64_x2[1] == 0 )

#define D_UUID_VALS( uuid )                                                                                   \
          (uuid)->___u32_x1[0], (uuid)->___u16_x2[0], (uuid)->___u16_x2[1],                                   \
          (uuid)->____u8_x8[0], (uuid)->____u8_x8[1], (uuid)->____u8_x8[2], (uuid)->____u8_x8[3],             \
          (uuid)->____u8_x8[4], (uuid)->____u8_x8[5], (uuid)->____u8_x8[6], (uuid)->____u8_x8[7]

#define D_UUID_FORMAT     "%08x-%04x-%04x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x"

#define D_UUID_LOG( Domain, LEVEL, u, msg )                                                                   \
     D_LOG( Domain, LEVEL, "%s == "D_UUID_FORMAT" ==\n",                                                      \
                           msg, D_UUID_VALS( u ) );

#define D_UUID_DEBUG_AT( Domain, u, msg )                                                                     \
     D_UUID_LOG( Domain, DEBUG, u, msg )

/**********************************************************************************************************************/

#define D_UUID_NULL  \
                   (DirectUUID){{ { 0 }, { 0, 0 }, \
                                  { 0, 0, 0, 0, 0, 0, 0, 0 } }}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

void DIRECT_API direct_uuid_generate( DirectUUID *ret_id );

#endif

