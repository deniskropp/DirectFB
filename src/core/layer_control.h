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

#ifndef __CORE__LAYER_CONTROL_H__
#define __CORE__LAYER_CONTROL_H__

#include <directfb.h>

#include <core/coretypes.h>


DFBResult dfb_layer_create_context          ( CoreLayer          *layer,
                                              CoreLayerContext  **ret_context );

DFBResult dfb_layer_get_active_context      ( CoreLayer          *layer,
                                              CoreLayerContext  **ret_context );

DFBResult dfb_layer_get_primary_context     ( CoreLayer          *layer,
                                              bool                activate,
                                              CoreLayerContext  **ret_context );

DFBResult dfb_layer_activate_context        ( CoreLayer          *layer,
                                              CoreLayerContext   *context );

DFBResult dfb_layer_remove_context          ( CoreLayer          *layer,
                                              CoreLayerContext   *context );

DFBResult dfb_layer_suspend                 ( CoreLayer          *layer );
DFBResult dfb_layer_resume                  ( CoreLayer          *layer );


DFBResult dfb_layer_get_current_output_field( CoreLayer          *layer,
                                              int                *field );

DFBResult dfb_layer_get_level               ( CoreLayer          *layer,
                                              int                *ret_level );

DFBResult dfb_layer_set_level               ( CoreLayer          *layer,
                                              int                 level );

DFBResult dfb_layer_wait_vsync              ( CoreLayer          *layer );

#endif
