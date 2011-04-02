/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#ifndef __IWATER_DEFAULT_H__
#define __IWATER_DEFAULT_H__

#include <directfb.h>

#include <core/coretypes.h>


#define MAX_ATTRIBUTES   256


typedef struct __Water_Attribute Attribute;
typedef struct __Water_State     State;


typedef DFBResult (*SetAttributeHandler)( State                      *state,
                                          Attribute                  *attribute,
                                          const WaterAttributeHeader *header,
                                          const void                 *value );

struct __Water_Attribute {
     DirectSerial             serial;

     union {
          u32                 v32;

          WaterScalar         scalar;

          WaterRenderMode     render_mode;
          DFBPoint            point;
          DFBRegion           region;
          WaterTransform      transform;
          WaterQualityLevel   quality;
          WaterPaintOptions   paint_options;
          WaterColor          color;
          WaterTileMode       tile_mode;
          WaterFillRule       fill_rule;
          WaterLineCapStyle   line_cap_style;
          WaterLineJoinStyle  line_join_style;

          WaterGradient       gradient;
          WaterPatternSurface pattern;

          void               *pointer;
     };


     SetAttributeHandler      Set;
};

struct __Water_State {
     Attribute                attributes[MAX_ATTRIBUTES];


     CardState                state;
};


typedef DFBResult (*RenderElementHandler)( State                    *state,
                                           const WaterElementHeader *header,
                                           const WaterScalar        *values,
                                           unsigned int              num_values );

/*
 * private data struct of IWater
 */
typedef struct {
     int                      ref;             /* reference counter */

     CoreDFB                 *core;

     State                    state;
     RenderElementHandler     Render[WATER_NUM_ELEMENT_TYPES];
} IWater_data;


#endif
