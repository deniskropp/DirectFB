/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef ___MATROX_H__
#define ___MATROX_H__

extern volatile __u8 *mmio_base;
extern int g400;

extern GfxCard *matrox;

extern int m_Source;
extern int m_source;

extern int m_Color;
extern int m_color;

extern int m_SrcKey;
extern int m_srckey;

extern int m_Blend;

extern int src_pixelpitch;


static inline unsigned int log2( unsigned int val )
{
     unsigned int ret = 0;

     while (val >>= 1)
          ret++;

     return ret;
}

#endif
