/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

void matrox_set_destination( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CoreSurface      *destination );
void matrox_set_clip( MatroxDriverData *mdrv,
                      MatroxDeviceData *mdev,
                      DFBRegion        *clip );

void matrox_validate_Color( MatroxDriverData *mdrv,
                            MatroxDeviceData *mdev,
                            CardState        *state );
void matrox_validate_color( MatroxDriverData *mdrv,
                            MatroxDeviceData *mdev,
                            CardState        *state );

void matrox_validate_drawBlend( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state );
void matrox_validate_blitBlend( MatroxDriverData *mdrv,
                                MatroxDeviceData *mdev,
                                CardState        *state );

void matrox_validate_Source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state );
void matrox_validate_source( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state );

void matrox_validate_SrcKey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state );
void matrox_validate_srckey( MatroxDriverData *mdrv,
                             MatroxDeviceData *mdev,
                             CardState        *state );

#endif
