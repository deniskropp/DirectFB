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

#ifndef ___RADEON_STATE_H__
#define ___RADEON_STATE_H__

#include "radeon.h"

void radeon_set_destination( RADEONDriverData *adrv,
                             RADEONDeviceData *adev,
                             CardState        *state );
void radeon_set_source( RADEONDriverData *adrv,
                        RADEONDeviceData *adev,
                        CardState        *state );

void radeon_set_blittingflags( RADEONDriverData *adrv,
                               RADEONDeviceData *adev,
                               CardState        *state );

void radeon_set_clip( RADEONDriverData *adrv,
                      RADEONDeviceData *adev,
                      CardState        *state );

void radeon_set_color( RADEONDriverData *adrv,
                       RADEONDeviceData *adev,
                       CardState        *state );

void radeon_set_src_colorkey( RADEONDriverData *adrv,
                              RADEONDeviceData *adev,
                              CardState        *state );

void radeon_set_blending_function( RADEONDriverData *adrv,
                                   RADEONDeviceData *adev,
                                   CardState        *state );


#endif
