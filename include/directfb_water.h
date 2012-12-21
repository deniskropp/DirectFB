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

#ifndef __DIRECTFB__WATER_H__
#define __DIRECTFB__WATER_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <direct/interface.h>
#include <direct/list.h>

/*
 * The IWater interface version
 */
#define IWATER_INTERFACE_VERSION  1


/*
 * Water Rendering
 */
D_DECLARE_INTERFACE( IWater )


/***********************************************************************************************************************
 ** IDs
 */

typedef unsigned long WaterPatternID;


/***********************************************************************************************************************
 ** Basic Types
 */

/*
 * A single integer, fixed or floating point value (union)
 *
 * See also WaterScalarType
 */
typedef union {
     int                 i;        /* Ordinary or fixed point integer value */
     float               f;        /* Floating point value */
} WaterScalar;

/*
 * Type of scalar [4]
 *
 * See also WaterScalar
 */
typedef enum {
     WST_UNKNOWN         = 0x0,    /* Unknown/invalid scalar type */

     WST_INTEGER         = 0x1,    /* Ordinary (pure) integer value (32 bit) */
     WST_FIXED_16_16     = 0x2,    /* Fixed point with 16 <b>sign</b>+<b>integral</b> and 16 <b>fractional</b> bits */
     WST_FIXED_24_8      = 0x3,    /* Fixed point with 24 <b>sign</b>+<b>integral</b> and 8 <b>fractional</b> bits */
     WST_FLOAT           = 0x4     /* Single precision floating point value (32 bit) */
} WaterScalarType;

/*
 * Macro determines if scalar is an ordinary (pure) integer or fixed point integer
 */
#define WATER_SCALAR_TYPE_IS_INT(type)       ((type) >= 0x1 && (type) <= 0x3)

/*
 * Macro determines if scalar is a single precision float
 */
#define WATER_SCALAR_TYPE_IS_FLOAT(type)     ((type) == WST_FLOAT)

/*
 * The way values are applied [4]
 */
typedef enum {
     WOP_SET             = 0x0,    /* Set the value */

     WOP_ADD             = 0x1,    /* Add the value */
     WOP_SUB             = 0x2,    /* Subtract the value */
     WOP_MUL             = 0x3,    /* Multiply with the value */
     WOP_DIV             = 0x4     /* Divide by the value */
} WaterOperator;


/***********************************************************************************************************************
 ** Transformation
 */

/*
 * Flags choosing predefined or free transformation and other things [8]
 */
typedef enum {
     WTF_NONE            = 0x00,   /* None of these */

     WTF_TYPE            = 0x01,   /* The <b>type</b> of the matrix has been set or determined */
     WTF_MATRIX          = 0x02,   /* The <b>matrix</b> is filled with all 3x3 values */
     WTF_REPLACE         = 0x10,   /* Replace previous transform completely, otherwise append to it */

     WTF_ALL             = 0x13    /* All of these */
} WaterTransformFlags;

/*
 * Common types of transformations [16]
 */
typedef enum {
     WTT_IDENTITY        = 0x0000, /* No specific type, arbitrary transform */

     WTT_UNKNOWN         = 0x0001,
     WTT_ZERO            = 0x0002,


     WTT_TRANSLATE_X     = 0x0004,
     WTT_TRANSLATE_Y     = 0x0008,

     WTT_TRANSLATE_MASK  = 0x000C,


     WTT_SCALE_X         = 0x0010,
     WTT_SCALE_Y         = 0x0020,

     WTT_SCALE_MASK      = 0x0030,


     WTT_FLIP_X          = 0x0040,
     WTT_FLIP_Y          = 0x0080,

     WTT_FLIP_MASK       = 0x00C0,


     WTT_SKEW_X          = 0x0100,
     WTT_SKEW_Y          = 0x0200,

     WTT_SKEW_MASK       = 0x0300,


     WTT_ROTATE_Q_90     = 0x1000,
     WTT_ROTATE_Q_180    = 0x2000,
     WTT_ROTATE_Q_270    = 0x4000,

     WTT_ROTATE_Q_MASK   = 0x7000,

     WTT_ROTATE_FREE     = 0x8000,

     WTT_ROTATE_MASK     = 0xF000,


     WTT_ALL             = 0xF3FF
} WaterTransformType;

/*
 * Transform
 */
typedef struct {
     WaterTransformFlags flags  : 8;    /* Transformation flags */

     WaterScalarType     scalar : 4;    /* Set scalar type for matrix entries */
     unsigned int        _rsrv1 : 4;    /* <i>reserved bits</i> */
     WaterTransformType  type   : 16;   /* Predefined type of transformation (using none, one or two values in <b>matrix</b>) */

     WaterScalar         matrix[9];     /* All nine entries of the 3x3 matrix from left to right, top to bottom */
} WaterTransform;


/***********************************************************************************************************************
 ** Attribute Values
 */

/*
 * Options generally applying to rendering or the destination [16]
 */
typedef enum {
     WRM_NONE            = 0x0000, /* None of these */

     WRM_ANTIALIAS       = 0x0001, /* Use anti-aliasing */
     WRM_COLORKEY        = 0x0002, /* Only write to pixels having a specific value */
     WRM_KEYPROTECT      = 0x0004, /* Ensure that a certain pixel value is not written,
                                      e.g. by toggling the LSB when the protected value is to be written */

     WRM_ALL             = 0x0007  /* All of these */
} WaterRenderMode;

/*
 * Options regarding the source of drawing/filling operations [16]
 */
typedef enum {
     WPO_NONE            = 0x0000, /* None of these */

     WPO_COLOR           = 0x0001, /* Use solid color, modulating RGB of pattern if used in conjunction */
     WPO_GRADIENT        = 0x0002, /* Use color gradient, modulating RGB of pattern if used in conjunction */
     WPO_PATTERN         = 0x0004, /* Use source pattern, surface containing ARGB values */
     WPO_MASK            = 0x0008, /* Use source mask, surface containing A(RGB) values */

     WPO_ALPHA           = 0x0010, /* Use alpha factor, modulating alpha of color|gradient|pattern|mask */
     WPO_BLEND           = 0x0020, /* Use alpha blending, applying source/destination blend functions */

     WPO_PREMULTIPLY     = 0x0040, /* Premultiply alpha values, modulating RGB when reading the pattern data */
     WPO_COLORKEY        = 0x0080, /* Skip pixels having a specific value, not writing the pixel at all */

     WPO_TILE_PATTERN    = 0x0100, /* Use tiling mode for pattern data, see WaterTileMode */
     WPO_TILE_MASK       = 0x0200, /* Use tiling mode for mask data, see WaterTileMode */

     WPO_ALL             = 0x03FF  /* All of these */
} WaterPaintOptions;

/*
 * Tiling mode for patterns and masks [4]
 *
 * To apply any of these tile modes, WPO_TILE_PATTERN and/or WPO_TILE_MASK must be set.
 * Otherwise the output will be bound by pattern/mask limits.
 *
 * See also WaterPaintOptions::WPO_TILE_PATTERN and WaterPaintOptions::WPO_TILE_MASK
 */
typedef enum {
     WTM_NONE            = 0x0,    /* Single tile, output bound by pattern/mask limits */

     WTM_SCALE           = 0x1,    /* Single tile, but scaled to cover all of the destination. */
     WTM_COLOR           = 0x2,    /* Single tile, but the area that is not covered is filled with a color, see also
                                      WaterAttributeType::WAT_DRAW_TILECOLOR, WaterAttributeType::WAT_FILL_TILECOLOR. */

     WTM_REPEAT          = 0x3,    /* Tiles are simply repeated */
     WTM_FLIP_X          = 0x4,    /* Tiles are repeated, but flipped horizontally each column */
     WTM_FLIP_Y          = 0x5,    /* Tiles are repeated, but flipped vertically each row */
     WTM_FLIP_XY         = 0x6     /* Tiles are flipped horizontally/vertically each column/row */
} WaterTileMode;

/*
 * Quality levels [4]
 */
typedef enum {
     WQL_FAIR            = 0x0,    /* Performance should be OK, quality still acceptable (use best possible and fast) */
     WQL_FAST            = 0x1,    /* Performance should be best, quality is secondary (use best possible at fastest) */
     WQL_BEST            = 0x2,    /* Quality should be best, no matter what performance will */
     WQL_OFF             = 0x3     /* Disable setting, always using worst quality */
} WaterQualityLevel;

/*
 * Blend modes [8]
 */
typedef enum {
     WBM_SRC             = 0x00,   /* The source is copied to the destination. The destination is not used as input. */
     WBM_DST             = 0x01,   /* The destination is left untouched. */

     WBM_SRCOVER         = 0x02,   /* The source is composited over the destination. */
     WBM_DSTOVER         = 0x03,   /* The destination is composited over the source and the result replaces the destination. */

     WBM_SRCIN           = 0x04,   /* The part of the source lying inside of the destination replaces the destination. */
     WBM_DSTIN           = 0x05,   /* The part of the destination lying inside of the source replaces the destination. */

     WBM_SRCOUT          = 0x06,   /* The part of the source lying outside of the destination replaces the destination. */
     WBM_DSTOUT          = 0x07,   /* The part of the destination lying outside of the source replaces the destination. */

     WBM_SRCATOP         = 0x08,   /* The part of the source lying inside of the destination is composited onto the destination. */
     WBM_DSTATOP         = 0x09,   /* The part of the destination lying inside of the source is
                                      composited over the source and replaces the destination. */

     WBM_CLEAR           = 0x0A,   /* Both the color and the alpha of the destination are cleared. */
     WBM_XOR             = 0x0B,   /* The part of the source that lies outside of the destination is
                                      combined with the part of the destination that lies outside of the source. */

     WBM_ADD             = 0x0C,   /* The source is added to the destination and replaces the destination. */
     WBM_SATURATE        = 0x0D,   /* The source is added to the destination with saturation and replaces the destination. */
     WBM_MULTIPLY        = 0x0E,   /* The source is multiplied by the destination and replaces the destination. */

     WBM_SCREEN          = 0x0F,   /* The source and destination are complemented and then multiplied and then replace the destination. */
     WBM_OVERLAY         = 0x10,   /* Multiplies or screens the colors, dependent on the destination color. */

     WBM_DARKEN          = 0x11,   /* Selects the darker of the destination and source colors. */
     WBM_LIGHTEN         = 0x12,   /* Selects the lighter of the destination and source colors. */

     WBM_COLORDODGE      = 0x13,   /* Brightens the destination color to reflect the source color. */
     WBM_COLORBURN       = 0x14,   /* Darkens the destination color to reflect the source color. */

     WBM_HARDLIGHT       = 0x15,   /* Multiplies or screens the colors, dependent on the source color value. */
     WBM_SOFTLIGHT       = 0x16,   /* Darkens or lightens the colors, dependent on the source color value. */

     WBM_DIFFERENCE      = 0x17,   /* Subtracts the darker of the two constituent colors from the lighter. */
     WBM_EXCLUSION       = 0x18,   /* Produces an effect similar to that of 'difference', but appears as lower contrast. */

     WBM_HSL_HUE         = 0x19,   /*  */
     WBM_HSL_SATURATION  = 0x1A,   /*  */
     WBM_HSL_COLOR       = 0x1B,   /*  */
     WBM_HSL_LUMINOSITY  = 0x1C    /*  */
} WaterBlendMode;

/*
 * Fill rules [4]
 */
typedef enum {
     WFR_NONZERO         = 0x0,    /* This rule determines the "insideness" of a point on the canvas by drawing a ray
                                      from that point to infinity in any direction and then examining the places where
                                      a segment of the shape crosses the ray. */
     WFR_EVENODD         = 0x1     /* This rule determines the "insideness" of a point on the canvas by drawing a ray
                                      from that point to infinity in any direction and counting the number of path
                                      segments from the given shape that the ray crosses. */
} WaterFillRule;

/*
 * Style of line end points [4]
 *
 * See also WaterAttributeType::WAT_LINE_STYLE and WaterAttributeType::WAT_LINE_CAP_STYLE
 */
typedef enum {
     WLCS_BUTT           = 0x0,    /* Cut the line perpendicular where the cut hits the end points */
     WLCS_ROUND          = 0x1,    /* WLCS_BUTT  with half a circle being appended (radius is half of line width) */
     WLCS_SQUARE         = 0x2     /* WLCS_BUTT  cut <b>off/after end point</b> (longer by half of line width) */
} WaterLineCapStyle;

/*
 * Style of line junctions [4]
 *
 * See also WaterAttributeType::WAT_LINE_STYLE and WaterAttributeType::WAT_LINE_JOIN_STYLE
 */
typedef enum {
     WLJS_MITER          = 0x0,    /* Draw miter (sharp edge), see also WaterAttributeType::WAT_LINE_MITER_LIMIT
                                      and WaterAttributeType::WAT_LINE_STYLE */
     WLJS_ROUND          = 0x1,    /* Round corners with a circle around the junction point (half line width radius) */
     WLJS_BEVEL          = 0x2     /* Cut off corner at half of the line width further than the junction point */
} WaterLineJoinStyle;

/**********************************************************************************************************************/

/*
 * A simple color definition
 *
 * See also WaterAttributeType::WAT_DRAW_COLOR and WaterAttributeType::WAT_FILL_COLOR
 *
 * API FIXME: Add more color definitions using better precision, example below.
 */
typedef struct {
     u8                  a;        /* Alpha channel value (0-255) */
     u8                  r;        /* Red channel value (0-255) */
     u8                  g;        /* Green channel value (0-255) */
     u8                  b;        /* Blue channel value (0-255) */
} WaterColor;

#if 0
typedef struct {
     WaterScalarType     scalar;

     union {
          struct {
               WaterScalar    a;
               WaterScalar    r;
               WaterScalar    g;
               WaterScalar    b;
          };

          struct {
               WaterScalar    a;
               WaterScalar    y;
               WaterScalar    u;
               WaterScalar    v;
          };
     };
} WaterColor;
#endif


/***********************************************************************************************************************
 ** Attributes
 */

/*
 * Attributes include all settings of a rendering context [8]
 *
 * Each attribute can be changed, affecting rendering, e.g. in a network of attributes and elements.
 *
 * There are different groups of attributes:
 * - <b>Rendering</b> attributes affecting both drawing and filling, WAT_RENDER_...
 * - <b>Drawing</b> attributes for the drawn outline, WAT_DRAW_...
 * - <b>Filling</b> attributes for the filled area, same as drawing plus WaterFillRule, WAT_FILL_...
 * - <b>Line</b> attributes for the style of the drawn outline, WAT_LINE_...
 */
typedef enum {
     WAT_UNKNOWN                = 0x00, /* Unknown/invalid attribute */

     /*
      * Rendering
      */
     WAT_RENDER_MODE            = 0x01, /* Select anti-aliasing etc. see WaterRenderMode */
     WAT_RENDER_OFFSET          = 0x02, /* Set/add an <b>offset</b> to apply, being independent from any transformation */
     WAT_RENDER_CLIP            = 0x03, /* Set/add a <b>clipping region</b>, allowing 0-n rectangles being specified, using
                                           multiple passes if more rectangles are defined than hardware supports */
     WAT_RENDER_TRANSFORM       = 0x04, /* Modify transformation of coordinates, see WaterTransform */
     WAT_RENDER_QUALITY_AA      = 0x05, /* Select quality level for anti-aliased edges, see WaterQualityLevel */
     WAT_RENDER_QUALITY_SCALE   = 0x06, /* Select quality level for scaling pattern/mask data, see WaterQualityLevel */
     WAT_RENDER_QUALITY_DITHER  = 0x07, /* Select quality level for low accuracy pixel formats, see WaterQualityLevel */

     /*
      * Drawing (outlines)
      */
     WAT_DRAW_OPTIONS           = 0x10, /* <b>Draw color</b>, <b>gradient</b>, <b>pattern</b>, <b>mask</b> etc. see WaterPaintOptions */
     WAT_DRAW_COLOR             = 0x11, /* Color for <b>drawing</b>, overridden by element colors (per group), see WaterColor */
     WAT_DRAW_GRADIENT          = 0x12, /* Gradient for <b>drawing</b>, see WaterGradient */
     WAT_DRAW_PATTERN           = 0x13, /* Select pattern surface for <b>drawing</b> */
     WAT_DRAW_PATTERN_TILEMODE  = 0x14, /* Tiling mode for <b>drawing</b> with WaterPaintOptions::WPO_TILE_PATTERN */
     WAT_DRAW_PATTERN_TILECOLOR = 0x15, /* Border color for <b>drawing</b> with WaterPaintOptions::WPO_TILE_PATTERN
                                           and WaterTileMode::WTM_COLOR, see WaterColor */
     WAT_DRAW_MASK              = 0x16, /* Select mask surface for <b>drawing</b> */
     WAT_DRAW_MASK_TILEMODE     = 0x17, /* Tiling mode for <b>drawing</b> with WaterPaintOptions::WPO_TILE_MASK */
     WAT_DRAW_MASK_TILECOLOR    = 0x18, /* Border color for <b>drawing</b> with WaterPaintOptions::WPO_TILE_MASK
                                           and WaterTileMode::WTM_COLOR, see WaterColor */
     WAT_DRAW_ALPHA             = 0x19, /* Change alpha factor for <b>drawing</b>, see WaterScalar */
     WAT_DRAW_BLEND             = 0x1A, /* Select a predefined set of blend functions for <b>drawing</b>, see WaterBlendMode */
     WAT_DRAW_TRANSFORM         = 0x1B, /* Set the transformation from the source, see WaterTransform */
     WAT_DRAW_COLORKEY          = 0x1C, /* Colorkey for <b>drawing</b>, see WaterColor */

     /*
      * Filling (areas)
      */
     WAT_FILL_OPTIONS           = 0x20, /* <b>Fill color</b>, <b>gradient</b>, <b>pattern</b>, <b>mask</b> etc. see WaterPaintOptions */
     WAT_FILL_COLOR             = 0x21, /* Color for <b>filling</b>, overridden by element colors (per group), see WaterColor */
     WAT_FILL_GRADIENT          = 0x22, /* Gradient for <b>filling</b>, see WaterGradient */
     WAT_FILL_RULE              = 0x23, /* Choose the <b>fill rule</b>, see WaterFillRule and WaterShapeFlags */
     WAT_FILL_PATTERN           = 0x24, /* Select pattern surface for <b>filling</b>, see WaterPattern */
     WAT_FILL_PATTERN_TILEMODE  = 0x25, /* Tiling mode for <b>filling</b> with WaterPaintOptions::WPO_TILE_PATTERN */
     WAT_FILL_PATTERN_TILECOLOR = 0x26, /* Border color for <b>filling</b> with WaterPaintOptions::WPO_TILE_PATTERN
                                           and WaterTileMode::WTM_COLOR, see WaterColor */
     WAT_FILL_MASK              = 0x27, /* Select mask surface for <b>filling</b> */
     WAT_FILL_MASK_TILEMODE     = 0x28, /* Tiling mode for <b>filling</b> with WaterPaintOptions::WPO_TILE_MASK */
     WAT_FILL_MASK_TILECOLOR    = 0x29, /* Border color for <b>filling</b> with WaterPaintOptions::WPO_TILE_MASK
                                           and WaterTileMode::WTM_COLOR, see WaterColor */
     WAT_FILL_ALPHA             = 0x2A, /* Change alpha factor for <b>filling</b>, see WaterScalar */
     WAT_FILL_BLEND             = 0x2B, /* Select a predefined set of blend functions for <b>filling</b>, see WaterBlendMode */
     WAT_FILL_TRANSFORM         = 0x2C, /* Set the transformation from the source, see WaterTransform */
     WAT_FILL_COLORKEY          = 0x2D, /* Colorkey for <b>filling</b>, see WaterColor */

     /*
      * Line Attributes (drawing)
      */
     WAT_LINE_WIDTH             = 0x30, /* Set <b>line width</b> for strokes, see WaterScalar */
     WAT_LINE_CAPSTYLE          = 0x31, /* Change <b>line cap</b> style, see WaterLineCapStyle */
     WAT_LINE_JOINSTYLE         = 0x32, /* Change <b>line join</b> style, see WaterLineJoinStyle */
     WAT_LINE_MITER             = 0x33, /* Change <b>miter limit</b>, see WaterScalar */
     WAT_LINE_DASHES            = 0x34  /* Change <b>dashes</b> */
} WaterAttributeType;

/*
 * Flags per attribute [8]
 */
typedef enum {
     WAF_NONE                 = 0x00,   /* None of these */

     WAF_OPERATOR             = 0x01,   /* Indicates that a WaterOperator is set in <b>op</b> */

     WAF_ALL                  = 0x01    /* All of these */
} WaterAttributeFlags;

/*
 * Header of an attribute in a stream or array
 */
typedef struct {
     WaterAttributeType       type     :  8;   /* Attribute type */
     WaterAttributeFlags      flags    :  8;   /* Attribute flags */
     WaterScalarType          scalar   :  4;   /* Set scalar type for values (if required) */
     WaterOperator            operatOr :  4;   /* Choose way of applying the value(s) */
} WaterAttributeHeader;

typedef struct {
     WaterAttributeHeader     header;

     const void              *value;
} WaterAttribute;


/***********************************************************************************************************************
 ** Gradients
 */

/*
 * Gradient Types [4]
 */
typedef enum {
     WGT_NONE                 = 0x0,

     WGT_LINEAR               = 0x1,    /* Linear gradient       values:  x1 y1 x2 y2       [x color] */
     WGT_RADIAL               = 0x2,    /* Radial gradient       values:  x1 y1 r1 x2 y2 r2 [x color] */
     WGT_CONICAL              = 0x3     /* Conical gradient      values:  x  y  a           [x color] */
} WaterGradientType;

/*
 * Gradient Flags [4]
 */
typedef enum {
     WGF_NONE                 = 0x00,

     WGF_ALL                  = 0x00
} WaterGradientFlags;

/*
 * Header of a gradient in a stream or array
 */
typedef struct {
     WaterGradientType        type   : 4;
     WaterGradientFlags       flags  : 4;
     WaterScalarType          scalar : 4;   /* Set scalar type for values */
} WaterGradientHeader;

typedef struct {
     WaterGradientHeader      header;

     u32                      num_values;    /* Number of values present in value array */
     const WaterScalar       *values;        /* Points to an array of values */
} WaterGradient;


/***********************************************************************************************************************
 ** Patterns
 */

/*
 * Pattern Types [4]
 */
typedef enum {
     WPT_SURFACE         = 0x0     /* Surface pattern       values:  id */
} WaterPatternType;

/*
 * Pattern Flags [4]
 */
typedef enum {
     WPF_NONE
} WaterPatternFlags;

/*
 * Header of a pattern in a stream or array
 */
typedef struct {
     WaterPatternType    type   : 4;
     WaterPatternFlags   flags  : 4;
     WaterScalarType     scalar : 4;   /* Set scalar type for values */

     DFBPoint            offset;
} WaterPatternHeader;

typedef struct {
     WaterPatternHeader  header;

     IDirectFBSurface   *surface;
} WaterPatternSurface;


/***********************************************************************************************************************
 ** Elements
 */

/*
 * Macro for generation of WaterElementType constants
 *
 * <pre>
 * |----:----|----:----|bbbc:bbbb|aaaa:aaaa|
 * </pre>
 *
 * <b>a</b>) Element type <b>index</b><br/>
 * <b>b</b>) Number of coordinate values for <b>initial</b> element<br/>
 * <b>c</b>) Number of coordinate values for <b>additional</b> elements, e.g. in a line strip<br/>
 *
 * In case of a polygon the number of elements is always one, but the initial and additional
 * count are chosen as three and one for generic validation of number of values.
 */
#define WATER_ELEMENT_TYPE( index, initial, additional )                        \
     (                                                                          \
      (((index)      & 0xff) <<  0) |                                           \
      (((initial)    & 0x0f) <<  8) |                                           \
      (((additional) & 0x0f) << 12)                                             \
     )

#define WATER_ELEMENT_TYPE_INDEX( type )               (((type) >>  0) & 0x7f)

#define WATER_ELEMENT_TYPE_VALUES_INITIAL( type )      (((type) >> 13) & 0x0f)

#define WATER_ELEMENT_TYPE_VALUES_ADDITIONAL( type )   (((type) >> 17) & 0x07)

/*
 * Compound type for basic and advanced elements with additional information [16]
 *
 * The symbols in the descriptions declare the essential element values for each item, e.g. a line or a rectangle.
 *
 * Values in brackets are non-essential for further items, e.g. in a line strip or smooth curve.
 */
typedef enum {
     WET_UNKNOWN              = 0,      /* Invalid or unknown / unspecified element type */

     WET_POINT                = WATER_ELEMENT_TYPE(  0, 2, 2 ), /* x y */
     WET_SPAN                 = WATER_ELEMENT_TYPE(  1, 3, 3 ), /* x y l */

     WET_LINE                 = WATER_ELEMENT_TYPE(  2, 4, 4 ), /* x1 y1 x2 y2 */
     WET_LINE_STRIP           = WATER_ELEMENT_TYPE(  3, 4, 2 ), /* [x1 y1] x2 y2 */
     WET_LINE_LOOP            = WATER_ELEMENT_TYPE(  4, 4, 2 ), /* [x1 y1] x2 y2 */

     WET_TRIANGLE             = WATER_ELEMENT_TYPE(  5, 6, 6 ), /* x1 y1 x2 y2 x3 y3 */
     WET_TRIANGLE_FAN         = WATER_ELEMENT_TYPE(  6, 6, 2 ), /* [x1 y1 x2 y2] x3 y3 */
     WET_TRIANGLE_STRIP       = WATER_ELEMENT_TYPE(  7, 6, 2 ), /* [x1 y1 x2 y2] x3 y3 */

     WET_RECTANGLE            = WATER_ELEMENT_TYPE(  8, 4, 4 ), /* x1 y1 x2 y2 */
     WET_RECTANGLE_STRIP      = WATER_ELEMENT_TYPE(  9, 4, 2 ), /* [x1 y1] x2 y2 */

     WET_TRAPEZOID            = WATER_ELEMENT_TYPE( 10, 6, 6 ), /* x1 y1 l1 x2 y2 l2 */
     WET_TRAPEZOID_STRIP      = WATER_ELEMENT_TYPE( 11, 6, 3 ), /* [x1 y1 l1] x2 y2 l2 */

     WET_QUADRANGLE           = WATER_ELEMENT_TYPE( 12, 8, 8 ), /* x1 y1 x2 y2 x3 y3 x4 y4 */
     WET_QUADRANGLE_STRIP     = WATER_ELEMENT_TYPE( 13, 8, 4 ), /* [x1 y1 x2 y2] x3 y3 x4 y4 */

     WET_POLYGON              = WATER_ELEMENT_TYPE( 14, 6, 2 ), /* [x1 y1 x2 y2] x3 y3 */


     WET_CIRCLE               = WATER_ELEMENT_TYPE( 15, 3, 3 ), /* x y r */
     WET_ELLIPSE              = WATER_ELEMENT_TYPE( 16, 4, 4 ), /* x y rx ry */

     WET_ARC_CIRCLE           = WATER_ELEMENT_TYPE( 17, 4, 4 ), /* x y r a f */
     WET_ARC_ELLIPSE          = WATER_ELEMENT_TYPE( 18, 5, 5 ), /* x y rx ry a f */

     WET_QUAD_CURVE           = WATER_ELEMENT_TYPE( 19, 4, 4 ), /* x y x2 y2 */
     WET_QUAD_CURVE_STRIP     = WATER_ELEMENT_TYPE( 20, 4, 2 ), /* x y [x2 y2] */

     WET_CUBIC_CURVE          = WATER_ELEMENT_TYPE( 21, 6, 6 ), /* x y x2 y2 x3 y3 */
     WET_CUBIC_CURVE_STRIP    = WATER_ELEMENT_TYPE( 22, 6, 4 )  /* x y [x2 y2] x3 y3 */
} WaterElementType;

#define WATER_NUM_ELEMENT_TYPES    23

/*
 * Flags per element [12]
 */
typedef enum {
     WEF_NONE                 = 0x000,  /* None of these */

     WEF_DRAW                 = 0x001,  /* Draw outline (low level flag), see also WaterShapeFlags */
     WEF_FILL                 = 0x002,  /* Fill area (low level flag), see also WaterShapeFlags */

     WEF_CONTINUE             = 0x010,  /* Skip first x/y, using last x/y from previous element */
     WEF_CLOSE                = 0x020,

     WEF_ARC_LARGE            = 0x100,  /* Large arc flag */
     WEF_ARC_SWEEP            = 0x200,  /* Sweep flag */

     WEF_ALL                  = 0x333   /* All of these */
} WaterElementFlags;

/*
 * Base header of an element in a stream or array
 */
typedef struct {
     WaterElementType         type   : 16;   /* Element type */
     WaterElementFlags        flags  : 12;   /* Element flags */
     WaterScalarType          scalar :  4;   /* Scalar type */
} WaterElementHeader;

typedef struct {
     WaterElementHeader       header;

     const WaterScalar       *values;        /* Points to an array of values */
     u32                      num_values;    /* Number of values present in value array */

     const u32               *indices;       /* Points to an array of indices */
     u32                      num_indices;   /* Number of indices present in indices array */
} WaterElement;


/***********************************************************************************************************************
 ** Shapes
 */

/*
 * Flags per shape [8]
 */
typedef enum {
     WSF_NONE                 = 0x00,   /* None of these */

     WSF_STROKE               = 0x01,   /* Draw along outlines of elements */
     WSF_FILL                 = 0x02,   /* Fill area of shape according to fill rule, see WaterFillRule */

     WSF_OPACITY              = 0x10,   /* Apply opacity to the shape (all joints are lit) */

     WSF_ALL                  = 0x13    /* All of these */
} WaterShapeFlags;

typedef struct {
     WaterShapeFlags          flags  :  8;   /* Flags for this shape */
     WaterScalarType          scalar :  4;   /* Scalar type for opacity */
     unsigned int             _rsrv1 : 20;   /* <i>reserved bits</i> */

     WaterScalar              opacity;       /* Opacity of shape, applied only when WSF_OPACITY is set */
} WaterShapeHeader;

typedef struct {
     WaterShapeHeader         header;

     const WaterAttribute    *attributes;
     u32                      num_attributes;

     const WaterElement      *elements;
     u32                      num_elements;
} WaterShape;



/**********
 * IWater *
 **********/

/*
 * <i>No summary yet...</i>
 */
D_DEFINE_INTERFACE(   IWater,

   /** Attributes **/

     /*
      * Set a single attribute.
      */
     DFBResult (*SetAttribute) (
          IWater                        *thiz,
          const WaterAttributeHeader    *header,
          const void                    *value
     );

     /*
      * Set a number of attributes (all in one buffer).
      */
     DFBResult (*SetAttributes) (
          IWater                        *thiz,
          const WaterAttribute          *attributes,
          unsigned int                   num_attributes
     );

     /*
      * Set a number of attributes (list of pointers).
      */
     DFBResult (*SetAttributeList) (
          IWater                        *thiz,
          const WaterAttribute         **attributes,
          unsigned int                   num_attributes
     );


   /** Elements **/

     /*
      * Render a single element.
      */
     DFBResult (*RenderElement) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterElementHeader      *header,
          const WaterScalar             *values,
          unsigned int                   num_values
     );

     /*
      * Render a single indexed element.
      */
     DFBResult (*RenderElementIndexed) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterElementHeader      *header,
          const WaterScalar             *values,
          unsigned int                   num_values,
          const unsigned int            *indices,
          unsigned int                   num_indices
     );

     /*
      * Render a number of elements (all in one buffer).
      */
     DFBResult (*RenderElements) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterElement            *elements,
          unsigned int                   num_elements
     );

     /*
      * Render a number of elements (list of pointers).
      */
     DFBResult (*RenderElementList) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterElement           **elements,
          unsigned int                   num_elements
     );


   /** Shapes **/

     /*
      * Render a single shape.
      */
     DFBResult (*RenderShape) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterShapeHeader        *header,
          const WaterAttribute          *attributes,
          unsigned int                   num_attributes,
          const WaterElement            *elements,
          unsigned int                   num_elements
     );

     /*
      * Render a number of shapes (all in one buffer).
      */
     DFBResult (*RenderShapes) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterShape              *shapes,
          unsigned int                   num_shapes
     );

     /*
      * Render a number of shapes (list of pointers).
      */
     DFBResult (*RenderShapeList) (
          IWater                        *thiz,
          IDirectFBSurface              *surface,
          const WaterShape             **shapes,
          unsigned int                   num_shapes
     );
)


#ifdef __cplusplus
}
#endif

#endif

