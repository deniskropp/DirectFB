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

#ifndef __UNIQUE__INPUT_SWITCH_H__
#define __UNIQUE__INPUT_SWITCH_H__

#include <directfb.h>

#include <unique/internal.h>
#include <unique/types.h>


DFBResult unique_input_switch_create      ( UniqueContext           *context,
                                            UniqueInputSwitch      **ret_switch );

DFBResult unique_input_switch_destroy     ( UniqueInputSwitch       *input_switch );

DFBResult unique_input_switch_add         ( UniqueInputSwitch       *input_switch,
                                            UniqueDevice            *device );

DFBResult unique_input_switch_remove      ( UniqueInputSwitch       *input_switch,
                                            UniqueDevice            *device );

DFBResult unique_input_switch_select      ( UniqueInputSwitch       *input_switch,
                                            UniqueDeviceClassIndex   index,
                                            UniqueInputChannel      *channel );

DFBResult unique_input_switch_set         ( UniqueInputSwitch       *input_switch,
                                            UniqueDeviceClassIndex   index,
                                            UniqueInputChannel      *channel );

DFBResult unique_input_switch_unset       ( UniqueInputSwitch       *input_switch,
                                            UniqueDeviceClassIndex   index,
                                            UniqueInputChannel      *channel );

DFBResult unique_input_switch_set_filter  ( UniqueInputSwitch       *input_switch,
                                            UniqueDeviceClassIndex   index,
                                            UniqueInputChannel      *channel,
                                            const UniqueInputEvent  *event,
                                            UniqueInputFilter      **ret_filter );

DFBResult unique_input_switch_unset_filter( UniqueInputSwitch       *input_switch,
                                            UniqueInputFilter       *filter );

DFBResult unique_input_switch_drop        ( UniqueInputSwitch       *input_switch,
                                            UniqueInputChannel      *channel );

DFBResult unique_input_switch_update      ( UniqueInputSwitch       *input_switch,
                                            UniqueInputChannel      *channel );


#endif

