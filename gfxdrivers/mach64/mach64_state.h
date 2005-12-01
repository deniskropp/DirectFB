/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef ___MACH64_STATE_H__
#define ___MACH64_STATE_H__

#include "mach64.h"

void mach64_set_destination( Mach64DriverData *mdrv,
                             Mach64DeviceData *mdev,
                             CardState        *state );
void mach64gt_set_destination( Mach64DriverData *mdrv,
                               Mach64DeviceData *mdev,
                               CardState        *state );

void mach64_set_source( Mach64DriverData *mdrv,
                        Mach64DeviceData *mdev,
                        CardState        *state );
void mach64gt_set_source( Mach64DriverData *mdrv,
                          Mach64DeviceData *mdev,
                          CardState        *state );
void mach64gt_set_source_scale( Mach64DriverData *mdrv,
                                Mach64DeviceData *mdev,
                                CardState        *state );

void mach64_set_clip( Mach64DriverData *mdrv,
                      Mach64DeviceData *mdev,
                      CardState        *state );

void mach64_set_color( Mach64DriverData *mdrv,
                       Mach64DeviceData *mdev,
                       CardState        *state );

void mach64_set_color_3d( Mach64DriverData *mdrv,
                          Mach64DeviceData *mdev,
                          CardState        *state );

void mach64_set_color_tex( Mach64DriverData *mdrv,
                           Mach64DeviceData *mdev,
                           CardState        *state );

void mach64_set_src_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev,
                              CardState        *state );

void mach64_set_src_colorkey_scale( Mach64DriverData *mdrv,
                                    Mach64DeviceData *mdev,
                                    CardState        *state );

void mach64_set_dst_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev,
                              CardState        *state );

void mach64_disable_colorkey( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev );

void mach64_set_draw_blend( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            CardState        *state );

void mach64_set_blit_blend( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            CardState        *state );

#endif
