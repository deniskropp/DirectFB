/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan82@cheapnet.it>.

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

#ifndef NVIDIA_TABLES_H
#define NVIDIA_TABLES_H


static const __u32 nvFIFO[][2] =
{
     {0x00000000, 0x80000000}, /* Rop         */
     {0x00000800, 0x80000001}, /* Clip        */
     {0x00001000, 0x80000002}, /* Pattern     */
     {0x00001800, 0x80000010}, /* Triangle    */
     {0x00002000, 0x80000014}, /* StretchBlit */
     {0x00002800, 0x80000012}, /* Rectange    */
     {0x00003000, 0x80000013}, /* Line        */
     {0x00003800, 0x80000011}  /* Blt         */
};


static const __u32 nvPRAMIN[][2] =
{
     {0x00000508, 0x01008043}, /* Rop */
     {0x0000050A, 0x00000000},
     {0x0000050B, 0x00000000},

     {0x0000050C, 0x01008019}, /* Clip */
     {0x0000050E, 0x00000000},
     {0x0000050F, 0x00000000},

     {0x00000510, 0x01008018}, /* Pattern */
     {0x00000512, 0x00000000},
     {0x00000513, 0x00000000},

     {0x00000514, 0x0100A01D}, /* Triangle */
     {0x00000516, 0x11401140},
     {0x00000517, 0x00000000},

     {0x00000518, 0x0100A01F}, /* Blit */
     {0x0000051A, 0x11401140},
     {0x0000051B, 0x00000000},

     {0x0000051C, 0x0100A01E}, /* Rectangle */
     {0x0000051E, 0x11401140},
     {0x0000051F, 0x00000000},

     {0x00000520, 0x0100A01C}, /* Line */
     {0x00000522, 0x11401140},
     {0x00000523, 0x00000000},

     {0x00000524, 0x0100A037}, /* StretchBlit */
     {0x00000526, 0x11401140},
     {0x00000527, 0x00000000}

};


static const __u32 nvPRAMIN_ARGB1555[][2] =
{
     {0x00000509, 0x00000902},
     {0x0000050D, 0x00000902},
     {0x00000511, 0x00000902},
     {0x00000515, 0x00000902},
     {0x00000519, 0x00000902},
     {0x0000051D, 0x00000902},
     {0x00000521, 0x00000902},
     {0x00000525, 0x00000902}
};

static const __u32 nvPRAMIN_RGB16[][2] =
{

     {0x00000509, 0x00000C02},
     {0x0000050D, 0x00000C02},
     {0x00000511, 0x00000C02},
     {0x00000515, 0x00000C02},
     {0x00000519, 0x00000C02},
     {0x0000051D, 0x00000C02},
     {0x00000521, 0x00000C02},
     {0x00000525, 0x00000C02}
};

static const __u32 nvPRAMIN_RGB32[][2] =
{

     {0x00000509, 0x00000E02},
     {0x0000050D, 0x00000E02},
     {0x00000511, 0x00000E02},
     {0x00000515, 0x00000E02},
     {0x00000519, 0x00000E02},
     {0x0000051D, 0x00000E02},
     {0x00000521, 0x00000E02},
     {0x00000525, 0x00000E02}
};

static const __u32 nvPRAMIN_ARGB[][2] =
{
     {0x00000509, 0x00000D02},
     {0x0000050D, 0x00000D02},
     {0x00000511, 0x00000D02},
     {0x00000515, 0x00000D02},
     {0x00000519, 0x00000D02},
     {0x0000051D, 0x00000D02},
     {0x00000521, 0x00000D02},
     {0x00000525, 0x00000D02}
};


static const __u32 nvPGRAPH_ARGB1555[][2] =
{
     {0x000001C9, 0x00440444},     /* 0x0724 */
     {0x00000186, 0x00004061},     /* 0x0618 */
     {0x0000020C, 0x09080808}      /* 0x0830 */
};

static const __u32 nvPGRAPH_RGB16[][2] =
{
     {0x000001C9, 0x00550555},
     {0x00000186, 0x000050A2},
     {0x0000020C, 0x0C0C0C0C}
};

static const __u32 nvPGRAPH_RGB32[][2] =
{
     {0x000001C9, 0x00770777},
     {0x00000186, 0x000070E5},
     {0x0000020C, 0x0E0E0E0E}
};

static const __u32 nvPGRAPH_ARGB[][2] =
{
     {0x000001C9, 0x00CC0CCC},
     {0x00000186, 0x0000C0D5},
     {0x0000020C, 0x0E0D0D0D}
};

static const __u32 nvPGRAPH_YUY2[][2] =
{
     {0x000001C9, 0x00EE0EEE},
     {0x00000186, 0x0000E125},
     {0x0000020C, 0x0E0D0D0D}
};

static const __u32 nvPGRAPH_UYVY[][2] =
{
     {0x000001C9, 0x00FF0FFF},
     {0x00000186, 0x0000F135},
     {0x0000020C, 0x0F0D0D0D}
};


#endif /* NVIDIA_TABLES_H */
