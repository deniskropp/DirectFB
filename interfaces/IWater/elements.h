/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


#ifndef __WATER_TEST__ELEMENTS_H__
#define __WATER_TEST__ELEMENTS_H__

#include "iwater_default.h"

/* Elements */

DFBResult TEST_Render_Point        ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Span         ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Line         ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_LineStripLoop( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Rectangle    ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Triangle     ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Trapezoid    ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Quadrangle   ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult
TEST_Render_Trapezoid_To_Quadrangle( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values,
                                     WaterElementHeader       *ret_element,
                                     WaterScalar              *ret_values,
                                     unsigned int             *ret_num_values );

DFBResult TEST_Render_Polygon      ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );

DFBResult TEST_Render_Circle       ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values );




DFBResult
TEST_Render_Rectangle_To_FillQuad  ( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values,
                                     WaterElementHeader       *ret_element,
                                     WaterScalar              *ret_values,
                                     unsigned int             *ret_num_values );

#endif
