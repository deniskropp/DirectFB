/*
   (c) Copyright 2001-2012  The DirectFB Organization (directfb.org)
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

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <math.h>

#include <directfb.h>
#include <directfb_water.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/memcpy.h>

#include <core/state.h>

#include <display/idirectfbsurface.h>

#include "elements.h"
#include "transform.h"
#include "util.h"

#include "iwater_default.h"


D_DEBUG_DOMAIN( IWater_default, "IWater/default", "IWater Interface default Implementation" );

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IWater, default )

/**********************************************************************************************************************/

static void
IWater_Destruct( IWater *thiz )
{
     D_DEBUG_AT( IWater_default, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );
}

static DirectResult
IWater_AddRef( IWater *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IWater_Release( IWater *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IWater_Destruct( thiz );

     return DFB_OK;
}

/***********************************************************************************************************************
 ** Setting Attributes
 */

static DFBResult
SetAttribute( IWater_data                *data,
              const WaterAttributeHeader *header,
              const void                 *value )
{
     if (header->type < 0 || header->type > MAX_ATTRIBUTES - 1)
          return DFB_INVARG;

     if (data->state.attributes[header->type].Set) {
          direct_serial_increase( &data->state.attributes[header->type].serial );

          return data->state.attributes[header->type].Set( &data->state, &data->state.attributes[header->type],
                                                           header, value );
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
IWater_SetAttribute( IWater                     *thiz,
                     const WaterAttributeHeader *header,
                     const void                 *value )
{
     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, header %p, value %p )\n", __FUNCTION__, thiz, header, value );

     if (!header || !value)
          return DFB_INVARG;

     return SetAttribute( data, header, value );
}

static DFBResult
IWater_SetAttributes( IWater               *thiz,
                      const WaterAttribute *attributes,
                      unsigned int          num_attributes )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, attributes, num_attributes );

     if (!attributes)
          return DFB_INVARG;

     for (i=0; i<num_attributes; i++) {
          ret = SetAttribute( data, &attributes[i].header, attributes[i].value );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

static DFBResult
IWater_SetAttributeList( IWater                *thiz,
                         const WaterAttribute **attributes,
                         unsigned int           num_attributes )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, attributes, num_attributes );

     if (!attributes)
          return DFB_INVARG;

     for (i=0; i<num_attributes; i++) {
          if (!attributes[i])
               return DFB_INVARG;

          ret = SetAttribute( data, &attributes[i]->header, attributes[i]->value );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

/***********************************************************************************************************************
 ** Rendering Elements
 */

static DFBResult
SetDestination( IWater_data      *data,
                IDirectFBSurface *surface )
{
     IDirectFBSurface_data *surface_data;

     DIRECT_INTERFACE_GET_DATA_FROM( surface, surface_data, IDirectFBSurface );

     dfb_state_set_destination( &data->state.state, surface_data->surface );

     DFBRegion clip = { 0, 0, surface_data->surface->config.size.w, surface_data->surface->config.size.h };

     dfb_state_set_clip( &data->state.state, &clip );

     return DFB_OK;
}


static DFBResult
RenderElement( IWater_data              *data,
               const WaterElementHeader *header,
               const WaterScalar        *values,
               unsigned int              num_values )
{
     unsigned int index = WATER_ELEMENT_TYPE_INDEX( header->type );

     if (index > WATER_NUM_ELEMENT_TYPES - 1)
          return DFB_INVARG;

     if (data->Render[index])
          return data->Render[index]( &data->state, header, values, num_values );

     return DFB_UNSUPPORTED;
}


static DFBResult
IWater_RenderElement( IWater                   *thiz,
                      IDirectFBSurface         *surface,
                      const WaterElementHeader *header,
                      const WaterScalar        *values,
                      unsigned int              num_values )
{
     DIRECT_INTERFACE_GET_DATA( IWater );

     D_DEBUG_AT( IWater_default, "%s( %p, header %p, values %p [%u] )\n", __FUNCTION__, thiz, header, values, num_values );

     if (!header || !values)
          return DFB_INVARG;

     SetDestination( data, surface );

     return RenderElement( data, header, values, num_values );
}

static DFBResult
IWater_RenderElements( IWater             *thiz,
                       IDirectFBSurface   *surface,
                       const WaterElement *elements,
                       unsigned int        num_elements )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, elements, num_elements );

     if (!elements)
          return DFB_INVARG;

     SetDestination( data, surface );

     for (i=0; i<num_elements; i++) {
          ret = RenderElement( data, &elements[i].header, elements[i].values, elements[i].num_values );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

static DFBResult
IWater_RenderElementList( IWater              *thiz,
                          IDirectFBSurface    *surface,
                          const WaterElement **elements,
                          unsigned int         num_elements )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, elements, num_elements );

     if (!elements)
          return DFB_INVARG;

     SetDestination( data, surface );

     for (i=0; i<num_elements; i++) {
          if (!elements[i])
               return DFB_INVARG;

          ret = RenderElement( data, &elements[i]->header, elements[i]->values, elements[i]->num_values );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

/***********************************************************************************************************************
 ** Rendering Shapes
 */

static DFBResult
RenderShape( IWater_data            *data,
             const WaterShapeHeader *header,
             const WaterAttribute   *attributes,
             unsigned int            num_attributes,
             const WaterElement     *elements,
             unsigned int            num_elements )
{
     DFBResult    ret;
     unsigned int i;

     if (header->flags & (WSF_FILL | WSF_STROKE)) {
          D_UNIMPLEMENTED();
     }
     else {
          if (!attributes)
               return DFB_INVARG;

          for (i=0; i<num_attributes; i++) {
               ret = SetAttribute( data, &attributes[i].header, attributes[i].value );
               if (ret)
                    return ret;
          }

          if (!elements)
               return DFB_INVARG;

          for (i=0; i<num_elements; i++) {
               ret = RenderElement( data, &elements[i].header, elements[i].values, elements[i].num_values );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

static DFBResult
IWater_RenderShape( IWater                 *thiz,
                    IDirectFBSurface       *surface,
                    const WaterShapeHeader *header,
                    const WaterAttribute   *attributes,
                    unsigned int            num_attributes,
                    const WaterElement     *elements,
                    unsigned int            num_elements )
{
     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p )\n", __FUNCTION__, thiz );

     SetDestination( data, surface );

     return RenderShape( data, header, attributes, num_attributes, elements, num_elements );
}

static DFBResult
IWater_RenderShapes( IWater           *thiz,
                     IDirectFBSurface *surface,
                     const WaterShape *shapes,
                     unsigned int      num_shapes )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, shapes, num_shapes );

     if (!shapes)
          return DFB_INVARG;

     SetDestination( data, surface );

     for (i=0; i<num_shapes; i++) {
          ret = RenderShape( data, &shapes[i].header,
                             shapes[i].attributes, shapes[i].num_attributes,
                             shapes[i].elements, shapes[i].num_elements );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

static DFBResult
IWater_RenderShapeList( IWater            *thiz,
                        IDirectFBSurface  *surface,
                        const WaterShape **shapes,
                        unsigned int       num_shapes )
{
     DFBResult    ret;
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IWater)

     D_DEBUG_AT( IWater_default, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, shapes, num_shapes );

     if (!shapes)
          return DFB_INVARG;

     SetDestination( data, surface );

     for (i=0; i<num_shapes; i++) {
          if (!shapes[i])
               return DFB_INVARG;

          ret = RenderShape( data, &shapes[i]->header,
                             shapes[i]->attributes, shapes[i]->num_attributes,
                             shapes[i]->elements, shapes[i]->num_elements );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
SetAttribute_32( State                      *state,
                 Attribute                  *attribute,
                 const WaterAttributeHeader *header,
                 const void                 *value )
{
     const u32 *v32 = value;

     attribute->v32 = *v32;

     return DFB_OK;
}

static DFBResult
SetAttribute_DFBPoint( State                      *state,
                       Attribute                  *attribute,
                       const WaterAttributeHeader *header,
                       const void                 *value )
{
     const DFBPoint *point = value;

     attribute->point = *point;

     return DFB_OK;
}

static DFBResult
SetAttribute_DFBRegion( State                      *state,
                        Attribute                  *attribute,
                        const WaterAttributeHeader *header,
                        const void                 *value )
{
     const DFBRegion *region = value;

     attribute->region = *region;

     return DFB_OK;
}

static DFBResult
SetAttribute_Transform( State                      *state,
                        Attribute                  *attribute,
                        const WaterAttributeHeader *header,
                        const void                 *value )
{
     const WaterTransform *transform = value;

     int             i, m   = 0;
     WaterScalarType scalar = WST_INTEGER;

     D_DEBUG_AT( IWater_default, "%s( state %p, attribute %p, transform %p )\n", __FUNCTION__, state, attribute, transform );

     D_DEBUG_AT( IWater_default, "  -> SCALAR 0x%x\n", transform->scalar );

     scalar = transform->scalar;

     if (transform->flags & WTF_TYPE) {
          D_DEBUG_AT( IWater_default, "  -> TYPE   0x%08x\n", transform->type );

          switch (transform->type) {
               case WTT_UNKNOWN:
                    D_DEBUG_AT( IWater_default, "  ->  UNKNOWN!?\n" );
                    return DFB_INVARG;

               case WTT_ZERO:
                    D_DEBUG_AT( IWater_default, "  ->  ZERO\n" );
                    break;

               case WTT_IDENTITY:
                    D_DEBUG_AT( IWater_default, "  ->  IDENTITY\n" );

                    if (!(transform->flags & WTF_REPLACE)) {
                         D_DEBUG_AT( IWater_default, "  ->  NO REPLACE!?\n" );
                         return DFB_OK;
                    }
                    break;

               case WTT_ROTATE_Q_90:
                    D_DEBUG_AT( IWater_default, "  ->  ROTATE_Q_90\n" );
                    break;

               case WTT_ROTATE_Q_180:
                    D_DEBUG_AT( IWater_default, "  ->  ROTATE_Q_180\n" );
                    break;

               case WTT_ROTATE_Q_270:
                    D_DEBUG_AT( IWater_default, "  ->  ROTATE_Q_270\n" );
                    break;

               default:
                    if (transform->type & WTT_TRANSLATE_X) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] TRANSLATE_X\n", m );
                         m++;
                    }
                    if (transform->type & WTT_TRANSLATE_Y) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] TRANSLATE_Y\n", m );
                         m++;
                    }
                    if (transform->type & WTT_SCALE_X) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] SCALE_X\n", m );
                         m++;
                    }
                    if (transform->type & WTT_SCALE_Y) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] SCALE_Y\n", m );
                         m++;
                    }
#if D_DEBUG_ENABLED
                    if (transform->type & WTT_FLIP_X) {
                         D_DEBUG_AT( IWater_default, "  ->      FLIP_X\n" );
                    }
                    if (transform->type & WTT_FLIP_Y) {
                         D_DEBUG_AT( IWater_default, "  ->      FLIP_Y\n" );
                    }
#endif
                    if (transform->type & WTT_SKEW_X) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] SKEW_X\n", m );
                         m++;
                    }
                    if (transform->type & WTT_SKEW_Y) {
                         D_DEBUG_AT( IWater_default, "  ->  [%d] SKEW_Y\n", m );
                         m++;
                    }
                    switch (transform->type & WTT_ROTATE_MASK) {
                         case WTT_ROTATE_Q_90:
                              D_DEBUG_AT( IWater_default, "  ->      ROTATE_Q_90\n" );
                              break;

                         case WTT_ROTATE_Q_180:
                              D_DEBUG_AT( IWater_default, "  ->      ROTATE_Q_180\n" );
                              break;

                         case WTT_ROTATE_Q_270:
                              D_DEBUG_AT( IWater_default, "  ->      ROTATE_Q_270\n" );
                              break;

                         case WTT_ROTATE_FREE:
                              D_DEBUG_AT( IWater_default, "  ->  [%d] ROTATE_FREE\n", m );
                              m++;
                              break;

                         default:
                              D_BUG( "unexpected rotation flags 0x%08x", transform->type & WTT_ROTATE_MASK );
                              break;
                    }
                    break;
          }
     }

     if (transform->flags & WTF_MATRIX) {
          D_DEBUG_AT( IWater_default, "  -> MATRIX (overrides type)\n" );

          m = 6;
     }

     if (m > 6)
          return DFB_LIMITEXCEEDED;

#if D_DEBUG_ENABLED
     if (m > 0) {
          D_DEBUG_AT( IWater_default, "  -> %d values\n", m );

          switch (scalar) {
               case WST_INTEGER:
                    for (i=0; i<m; i++)
                         D_DEBUG_AT( IWater_default, "  ->  [%d] %4d\n", i, transform->matrix[i].i );
                    break;

               case WST_FIXED_16_16:
                    for (i=0; i<m; i++)
                         D_DEBUG_AT( IWater_default, "  ->  [%d] %c%4d.%05u\n",
                                     i, TEST_FIXED_16_16_VALUES_05( transform->matrix[i].i ) );
                    break;

               case WST_FLOAT:
                    for (i=0; i<m; i++)
                         D_DEBUG_AT( IWater_default, "  ->  [%d] %4.14f\n", i, transform->matrix[i].f );
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
     }
#endif

     /*
      * Simple replacement?
      */
     if (transform->flags & WTF_REPLACE) {
          WaterTransformFlags flags = (transform->flags & ~WTF_REPLACE);

          D_DEBUG_AT( IWater_default, "  => REPLACE (0x%08x)\n", flags );

          attribute->transform.flags  = flags;
          attribute->transform.scalar = scalar;

          if (flags & WTF_TYPE)
               attribute->transform.type = transform->type;

          direct_memcpy( &attribute->transform.matrix[0], &transform->matrix[0], sizeof(transform->matrix[0]) * m );

          return DFB_OK;
     }

     /*
      * Combine with current transformation...
      *
      * FIXME: Optimize better!
      */
     if (transform->flags & WTF_TYPE && scalar == attribute->transform.scalar) {
          /*
           * Scalar types are equal, check for transform types...
           */
          if (transform->type == attribute->transform.type) {
               /*
                * Transform types are equal, check for common types we can handle...
                */
               switch (transform->type) {
                    case WTT_TRANSLATE_X:
                    case WTT_TRANSLATE_Y:
                    case WTT_SKEW_X:
                    case WTT_SKEW_Y:
                    case WTT_ROTATE_FREE:
                    case WTT_TRANSLATE_X | WTT_TRANSLATE_Y:
                    case WTT_SKEW_X | WTT_SKEW_Y:
                         D_DEBUG_AT( IWater_default, "  => OPTIMIZED ADD (%d)\n", m );

                         if (WATER_SCALAR_TYPE_IS_INT(scalar)) {
                              for (i=0; i<m; i++)
                                   attribute->transform.matrix[i].i += transform->matrix[i].i;
                         }
                         else {
                              for (i=0; i<m; i++)
                                   attribute->transform.matrix[i].f += transform->matrix[i].f;
                         }
                         return DFB_OK;

                    case WTT_SCALE_X:
                    case WTT_SCALE_Y:
                    case WTT_SCALE_X | WTT_SCALE_Y:
                         D_DEBUG_AT( IWater_default, "  => OPTIMIZED MULTIPLY (%d)\n", m );

                         switch (scalar) {
                              case WST_INTEGER:
                                   for (i=0; i<m; i++)
                                        attribute->transform.matrix[i].i *= transform->matrix[i].i;
                                   break;

                              case WST_FIXED_16_16:
                                   for (i=0; i<m; i++)
                                        attribute->transform.matrix[i].i =
                                             ((long long) attribute->transform.matrix[i].i *
                                              (long long) transform->matrix[i].i + TEST_ROUND16) >> 16;
                                   break;

                              case WST_FLOAT:
                                   for (i=0; i<m; i++)
                                        attribute->transform.matrix[i].f *= transform->matrix[i].f;
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   return DFB_BUG;
                         }
                         return DFB_OK;

                    default:
                         break;
               }
          }
     }

     {
          WaterTransform trans = *transform;

          /*
           * Generic version
           *
           * Convert current to matrix if needed, convert new to matrix if needed, then multiply
           */
          TEST_Transform_TypeToMatrix_16_16( &attribute->transform );
          TEST_Transform_TypeToMatrix_16_16( &trans );

          TEST_Transform_Append_16_16( &attribute->transform, &trans );
     }

     return DFB_OK;
}

static DFBResult
SetAttribute_Gradient( State                      *state,
                       Attribute                  *attribute,
                       const WaterAttributeHeader *header,
                       const void                 *value )
{
     const WaterGradient *gradient = value;

     attribute->gradient = *gradient;

     // FIXME: values

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
SetAttribute_Pattern( State                      *state,
                      Attribute                  *attribute,
                      const WaterAttributeHeader *header,
                      const void                 *value )
{
     DFBResult                  ret;
     const WaterPatternSurface *pattern = value;

     ret = pattern->surface->AddRef( pattern->surface );
     if (ret)
          return ret;

     if (attribute->pattern.surface) {
          attribute->pattern.surface->Release( attribute->pattern.surface );
          attribute->pattern.surface = NULL;
     }

     attribute->pattern = *pattern;

     return DFB_OK;
}

static void
InitTransform( WaterTransform *transform )
{
     transform->flags = WTF_TYPE;
     transform->type  = WTT_IDENTITY;
}

static void
InitState( State   *state,
           CoreDFB *core )
{
     int i;

     for (i=0; i<MAX_ATTRIBUTES; i++) {
          direct_serial_init( &state->attributes[i].serial );
          
     }

     state->attributes[WAT_RENDER_MODE].Set            = SetAttribute_32;
     state->attributes[WAT_RENDER_OFFSET].Set          = SetAttribute_DFBPoint;
     state->attributes[WAT_RENDER_CLIP].Set            = SetAttribute_DFBRegion;
     state->attributes[WAT_RENDER_TRANSFORM].Set       = SetAttribute_Transform;
     state->attributes[WAT_RENDER_QUALITY_AA].Set      = SetAttribute_32;
     state->attributes[WAT_RENDER_QUALITY_SCALE].Set   = SetAttribute_32;
     state->attributes[WAT_RENDER_QUALITY_DITHER].Set  = SetAttribute_32;

     state->attributes[WAT_DRAW_OPTIONS].Set           = SetAttribute_32;
     state->attributes[WAT_DRAW_COLOR].Set             = SetAttribute_32;
     state->attributes[WAT_DRAW_GRADIENT].Set          = SetAttribute_Gradient;
     state->attributes[WAT_DRAW_PATTERN].Set           = SetAttribute_Pattern;
     state->attributes[WAT_DRAW_PATTERN_TILEMODE].Set  = SetAttribute_32;
     state->attributes[WAT_DRAW_PATTERN_TILECOLOR].Set = SetAttribute_32;
     state->attributes[WAT_DRAW_MASK].Set              = SetAttribute_Pattern;
     state->attributes[WAT_DRAW_MASK_TILEMODE].Set     = SetAttribute_32;
     state->attributes[WAT_DRAW_MASK_TILECOLOR].Set    = SetAttribute_32;
     state->attributes[WAT_DRAW_ALPHA].Set             = SetAttribute_32;
     state->attributes[WAT_DRAW_BLEND].Set             = SetAttribute_32;
     state->attributes[WAT_DRAW_TRANSFORM].Set         = SetAttribute_Transform;
     state->attributes[WAT_DRAW_COLORKEY].Set          = SetAttribute_32;

     state->attributes[WAT_FILL_OPTIONS].Set           = SetAttribute_32;
     state->attributes[WAT_FILL_COLOR].Set             = SetAttribute_32;
     state->attributes[WAT_FILL_GRADIENT].Set          = SetAttribute_Gradient;
     state->attributes[WAT_FILL_RULE].Set              = SetAttribute_32;
     state->attributes[WAT_FILL_PATTERN].Set           = SetAttribute_Pattern;
     state->attributes[WAT_FILL_PATTERN_TILEMODE].Set  = SetAttribute_32;
     state->attributes[WAT_FILL_PATTERN_TILECOLOR].Set = SetAttribute_32;
     state->attributes[WAT_FILL_MASK].Set              = SetAttribute_Pattern;
     state->attributes[WAT_FILL_MASK_TILEMODE].Set     = SetAttribute_32;
     state->attributes[WAT_FILL_MASK_TILECOLOR].Set    = SetAttribute_32;
     state->attributes[WAT_FILL_ALPHA].Set             = SetAttribute_32;
     state->attributes[WAT_FILL_BLEND].Set             = SetAttribute_32;
     state->attributes[WAT_FILL_TRANSFORM].Set         = SetAttribute_Transform;
     state->attributes[WAT_FILL_COLORKEY].Set          = SetAttribute_32;

     state->attributes[WAT_LINE_WIDTH].Set             = SetAttribute_32;
     state->attributes[WAT_LINE_CAPSTYLE].Set          = SetAttribute_32;
     state->attributes[WAT_LINE_JOINSTYLE].Set         = SetAttribute_32;
     state->attributes[WAT_LINE_MITER].Set             = SetAttribute_32;
     state->attributes[WAT_LINE_DASHES].Set            = SetAttribute_32;

     InitTransform( &state->attributes[WAT_RENDER_TRANSFORM].transform );
     InitTransform( &state->attributes[WAT_DRAW_TRANSFORM].transform );
     InitTransform( &state->attributes[WAT_FILL_TRANSFORM].transform );

     state->attributes[WAT_DRAW_ALPHA].scalar.i = 0xff;     // FIXME: scalar type?
     state->attributes[WAT_FILL_ALPHA].scalar.i = 0xff;

     state->attributes[WAT_LINE_MITER].scalar.i = 5;        // FIXME: scalar type?  default ???


     dfb_state_init( &state->state, core );
}

static void
InitRender( IWater_data *data )
{
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_POINT)]      = TEST_Render_Point;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_SPAN)]       = TEST_Render_Span;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_LINE)]       = TEST_Render_Line;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_LINE_STRIP)] = TEST_Render_LineStripLoop;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_LINE_LOOP)]  = TEST_Render_LineStripLoop;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_RECTANGLE)]  = TEST_Render_Rectangle;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_TRIANGLE)]   = TEST_Render_Triangle;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_TRAPEZOID)]  = TEST_Render_Trapezoid;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_QUADRANGLE)] = TEST_Render_Quadrangle;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_POLYGON)]    = TEST_Render_Polygon;
     data->Render[WATER_ELEMENT_TYPE_INDEX(WET_CIRCLE)]     = TEST_Render_Circle;
}

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... )
{
     D_DEBUG_AT( IWater_default, "%s()\n", __FUNCTION__ );

     (void) ctx;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     DFBResult  ret = DFB_INVARG;
     IDirectFB *dfb;
     IWater    *thiz = interface;
     CoreDFB   *core;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IWater)

     D_DEBUG_AT( IWater_default, "%s( %p )\n", __FUNCTION__, thiz );

     va_list tag;
     va_start(tag, interface);
     dfb = va_arg(tag, IDirectFB *);
     (void)dfb;
     core = va_arg(tag, CoreDFB *);
     va_end( tag );

     /* Check arguments. */
     if (!thiz)
          goto error;

     /* Initialize interface data. */
     data->ref  = 1;
     data->core = core;

     InitState( &data->state, core );

     InitRender( data );

     /* Initialize function pointer table. */
     thiz->AddRef                = IWater_AddRef;
     thiz->Release               = IWater_Release;

     thiz->SetAttribute          = IWater_SetAttribute;
     thiz->SetAttributes         = IWater_SetAttributes;
     thiz->SetAttributeList      = IWater_SetAttributeList;

     thiz->RenderElement         = IWater_RenderElement;
     thiz->RenderElements        = IWater_RenderElements;
     thiz->RenderElementList     = IWater_RenderElementList;

     thiz->RenderShape           = IWater_RenderShape;
     thiz->RenderShapes          = IWater_RenderShapes;
     thiz->RenderShapeList       = IWater_RenderShapeList;

     return DFB_OK;


error:
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

