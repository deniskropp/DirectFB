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

#ifndef ___MATROX_STATE_H__
#define ___MATROX_STATE_H__

inline void matrox_set_destination();
inline void matrox_set_clip();

inline void matrox_validate_Color();
inline void matrox_validate_color();
inline void matrox_validate_Blend();

inline void matrox_validate_Source();
inline void matrox_validate_source();

inline void matrox_validate_SrcKey();
inline void matrox_validate_srckey();

inline void matrox_set_dwgctl( DFBAccelerationMask accel );

extern int matrox_tmu;
extern int matrox_w2;
extern int matrox_h2;

#endif
