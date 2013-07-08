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


#ifndef __WATER_TEST__TRANSFORM_H__
#define __WATER_TEST__TRANSFORM_H__

void TEST_Transform_TypeToMatrix_16_16( WaterTransform *transform );

void TEST_Transform_Append_16_16( WaterTransform *transform, const WaterTransform *other );

void TEST_Transform_TypeToMatrix( WaterTransform *transform );

void TEST_Transform_Append( WaterTransform *transform, const WaterTransform *other );

void TEST_Transform_XY( WaterScalarType    scalar,
                        const WaterScalar *matrix,
                        int               *x,
                        int               *y );

void TEST_Transform_XY_float( WaterTransform    *transform,
                              float             *x,
                              float             *y );

void TEST_Transform_Points( const WaterTransform *transform,
                            DFBPoint             *points,
                            unsigned int          num_points );

void TEST_Transform_Rectangles( const WaterTransform *transform,
                                DFBRectangle         *rects,
                                unsigned int          num_rects );

void TEST_Transform_Regions( const WaterTransform *transform,
                             DFBRegion            *regions,
                             unsigned int          num_regions );

void TEST_Transform_Triangles( const WaterTransform *transform,
                               DFBTriangle          *triangles,
                               unsigned int          num_triangles );

#endif
