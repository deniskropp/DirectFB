/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_water.h>

static const DirectFBPixelFormatNames( format_names );
static const DirectFBWaterElementTypeNames( m_element_types );

/**********************************************************************************************************************/

static int                   m_width;
static int                   m_height;
static DFBSurfacePixelFormat m_format;

/**********************************************************************************************************************/

typedef DFBResult (*TestFunc)( IWater *water, IDirectFBSurface *surface );

/**********************************************************************************************************************/

static DFBResult Test_Simple        ( IWater *water, IDirectFBSurface *surface );

static DFBResult Test_RenderElement ( IWater *water, IDirectFBSurface *surface );
static DFBResult Test_RenderElements( IWater *water, IDirectFBSurface *surface );

static DFBResult Test_RenderShape   ( IWater *water, IDirectFBSurface *surface );

static DFBResult Test_RenderShapes  ( IWater *water, IDirectFBSurface *surface );

/**********************************************************************************************************************/

static DFBResult RunTest( TestFunc func, IWater *water, IDirectFBSurface *surface );

/**********************************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     DFBSurfaceDescription  desc;
     IDirectFB             *dfb;
     IDirectFBSurface      *surface;
     IWater                *water;

     D_INFO( "Tests/Water: Starting up...\n" );

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return -3;
     }

     /* Fill surface description, flipping primary. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     /* Set width or height? */
     if (m_width > 0) {
          desc.flags |= DSDESC_WIDTH;
          desc.width  = m_width;
     }
     if (m_height > 0) {
          desc.flags |= DSDESC_HEIGHT;
          desc.height = m_height;
     }

     /* Set pixel format? */
     if (m_format != DSPF_UNKNOWN) {
          desc.flags       |= DSDESC_PIXELFORMAT;
          desc.pixelformat  = m_format;
     }

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &surface );
     if (ret) {
          D_DERROR( ret, "IDirectFB::CreateSurface() failed!\n" );
          dfb->Release( dfb );
          return -4;
     }

     /* Get the extended rendering interface. */
     ret = dfb->GetInterface( dfb, "IWater", NULL, dfb, (void**) &water );
     if (ret) {
          DirectFBError( "IDirectFB::GetInterface( 'IWater' ) failed", ret );
          surface->Release( surface );
          dfb->Release( dfb );
          return -5;
     }


     D_INFO( "Tests/Water: Got render interface, running tests...\n" );

     RunTest( Test_Simple, water, surface );

     RunTest( Test_RenderElement, water, surface );

     RunTest( Test_RenderElements, water, surface );

     RunTest( Test_RenderShape, water, surface );

     RunTest( Test_RenderShapes, water, surface );


     D_INFO( "Tests/Water: Dumping surface...\n" );

     unlink( "dfbrender.pgm" );
     unlink( "dfbrender.ppm" );
     surface->Dump( surface, "dfbrender", NULL );


     D_INFO( "Tests/Water: Shutting down...\n" );

     /* Release the render interface. */
     water->Release( water );

     /* Release the surface. */
     surface->Release( surface );

     /* Release the super interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     int i = 0;

     fprintf (stderr, "\nDirectFB Render Test (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -m, --mode    <width>x<height>  Set test surface size (default 800x600)\n");
     fprintf (stderr, "   -f, --format  <pixelformat>     Change the pixel format\n");
     fprintf (stderr, "   -h, --help                      Show this help message\n");
     fprintf (stderr, "   -v, --version                   Print version information\n");
     fprintf (stderr, "\n");

     fprintf (stderr, "Known pixel formats:\n");

     while (format_names[i].format != DSPF_UNKNOWN) {
          DFBSurfacePixelFormat format = format_names[i].format;

          fprintf (stderr, "   %-10s %2d bits, %d bytes",
                   format_names[i].name, DFB_BITS_PER_PIXEL(format),
                   DFB_BYTES_PER_PIXEL(format));

          if (DFB_PIXELFORMAT_HAS_ALPHA(format))
               fprintf (stderr, "   ALPHA");

          if (DFB_PIXELFORMAT_IS_INDEXED(format))
               fprintf (stderr, "   INDEXED");

          if (DFB_PLANAR_PIXELFORMAT(format)) {
               int planes = DFB_PLANE_MULTIPLY(format, 1000);

               fprintf (stderr, "   PLANAR (x%d.%03d)",
                        planes / 1000, planes % 1000);
          }

          fprintf (stderr, "\n");

          ++i;
     }
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_mode( const char *arg )
{
     if (sscanf( arg, "%dx%d", &m_width, &m_height ) != 2 || m_width < 1 || m_height < 1) {
          fprintf (stderr, "\nInvalid mode specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_format( const char *arg )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               m_format = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static bool
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return false;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return false;
          }

          if (strcmp (arg, "-m") == 0 || strcmp (arg, "--mode") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_mode( argv[n] ))
                    return false;

               continue;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[n] ))
                    return false;

               continue;
          }

          print_usage (argv[0]);

          return false;
     }

     return true;
}

/**********************************************************************************************************************/

static DFBResult
RunTest( TestFunc          func,
         IWater  *water,
         IDirectFBSurface *surface )
{
     /* Clear the buffer first... */
     surface->Clear( surface, 0xc0, 0xb0, 0x90, 0xff );

     /* Run the actual test... */
     func( water, surface );

     /* Show the buffer! */
     surface->Flip( surface, NULL, DSFLIP_NONE );
     sleep( 10 );

     return DFB_OK;
}

/**********************************************************************************************************************/

#define _T(x...)  \
     do {                                                                       \
          DFBResult ret = x;                                                    \
                                                                                \
          if (ret) {                                                            \
               D_DERROR( ret, "Tests/Water: '%s' failed!\n", #x );          \
               /*return ret;*/                                                      \
          }                                                                     \
     } while (0)

/**********************************************************************************************************************/

static DFBResult
Test_Simple( IWater *water, IDirectFBSurface *surface )
{
     WaterScalar test_rect[4] = {{ 130 },{ 190 },{ 400 },{ 200 }};

     D_INFO( "Tests/Water: Testing SetAttribute( COLOR ) and RenderElement( RECTANGLE )...\n" );

     WaterAttributeHeader attribute;
     WaterElementHeader   element;
     WaterColor           fill_color = { 0xff, 0x20, 0x80, 0xff };
     WaterColor           draw_color = { 0xff, 0xf8, 0xf4, 0xd8 };

     /*
      * Set the fill color via SetAttribute()
      */
     attribute.type  = WAT_FILL_COLOR;
     attribute.flags = WAF_NONE;

     _T( water->SetAttribute( water, &attribute, &fill_color ) );

     /*
      * Set the draw color via SetAttribute()
      */
     attribute.type  = WAT_DRAW_COLOR;
     attribute.flags = WAF_NONE;

     _T( water->SetAttribute( water, &attribute, &draw_color ) );

     /*
      * Render a rectangle via RenderElement()
      */
     element.type   = WET_RECTANGLE;
     element.flags  = WEF_FILL | WEF_DRAW;
     element.scalar = WST_INTEGER;

     _T( water->RenderElement( water, surface, &element, test_rect, 4 ) );

     return DFB_OK;
}

/**********************************************************************************************************************/

/*
 * X,Y values (5) for five points, a line strip/loop, a triangle strip, a rectangle strip, a polygon...
 */
static const WaterScalar m_xy_vals5[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */

     { .i =  93 },     /* X */
     { .i = 366 },     /* Y */

     { .i = 707 },     /* X */
     { .i = 366 }      /* Y */
};

/*
 * X,Y values (6) for six points, three lines, a line strip/loop, two triangles, a triangle fan,
 * three rectangles, a rectangle strip, a quadrangle strip, a polygon...
 */
static const WaterScalar m_xy_vals6[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */

     { .i =  93 },     /* X */
     { .i = 366 },     /* Y */

     { .i = 707 },     /* X */
     { .i = 366 },     /* Y */

     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */
};

/*
 * X,Y values (8) for eight points, four lines, a line strip/loop, a triangle fan,
 * four rectangles, a rectangle strip, two quadrangles, a quadrangle strip, a polygon...
 */
static const WaterScalar m_xy_vals8[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */

     { .i =  93 },     /* X */
     { .i = 366 },     /* Y */

     { .i = 707 },     /* X */
     { .i = 366 },     /* Y */

     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */

     { .i = 400 },     /* X */
     { .i = 150 },     /* Y */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */
};

/*
 * X,Y,W,H values (5) for five rectangles...
 */
static const WaterScalar m_xywh_vals5[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */
     { .i =  12 },     /* W */
     { .i =  20 },     /* H */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */
     { .i =  40 },     /* W */
     { .i =  20 },     /* H */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */
     { .i =  12 },     /* W */
     { .i =  20 },     /* H */

     { .i =  93 },     /* X */
     { .i = 366 },     /* Y */
     { .i =  20 },     /* W */
     { .i =  12 },     /* H */

     { .i = 707 },     /* X */
     { .i = 366 },     /* Y */
     { .i =  20 },     /* W */
     { .i =  12 }      /* H */
};

/*
 * X,Y,L/R values (3) for three spans, a trapezoid strip, three circles...
 */
static const WaterScalar m_xyl_vals3[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */
     { .i = 100 },     /* Length / Radius */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */
     { .i = 200 },     /* Length / Radius */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */
     { .i = 100 },     /* Length / Radius */
};

/*
 * X,Y,L/R values (4) for four spans, two trapezoids, a trapezoid strip, four circles...
 */
static const WaterScalar m_xyl_vals4[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */
     { .i = 100 },     /* Length / Radius */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */
     { .i = 200 },     /* Length / Radius */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */
     { .i = 100 },     /* Length / Radius */

     { .i = 400 },     /* X */
     { .i = 150 },     /* Y */
     { .i =  50 },     /* Length / Radius */
};

/*
 * X,Y,RX,RY values for three ellipses...
 */
static const WaterScalar m_xyrxry_vals3[] = {
     { .i = 211 },     /* X */
     { .i =   5 },     /* Y */
     { .i =  70 },     /* Radius X */
     { .i = 100 },     /* Radius Y */

     { .i = 400 },     /* X */
     { .i = 588 },     /* Y */
     { .i = 200 },     /* Radius X */
     { .i =  50 },     /* Radius Y */

     { .i = 589 },     /* X */
     { .i =   5 },     /* Y */
     { .i =  70 },     /* Radius X */
     { .i = 100 },     /* Radius Y */
};

/*
 * Test elements, each one of a type.
 */
static const WaterElement m_test_elements[] = {
     /*
      * Basic types
      */
#if 0
     {
          .etype      = WET_POINT,
          .flags      = WEF_DRAW,
          .values     = m_xy_vals5,
          .num_values = D_ARRAY_SIZE(m_xy_vals5)
     },
     {
          .etype      = WET_SPAN,
          .flags      = WEF_DRAW,
          .values     = m_xyl_vals4,
          .num_values = D_ARRAY_SIZE(m_xyl_vals4)
     },
     {
          .etype      = WET_LINE,
          .flags      = WEF_DRAW,
          .values     = m_xy_vals6,
          .num_values = D_ARRAY_SIZE(m_xy_vals6)
     },
     {
          .etype      = WET_LINE_STRIP,
          .flags      = WEF_DRAW,
          .values     = m_xy_vals6,
          .num_values = D_ARRAY_SIZE(m_xy_vals6)
     },
     {
          .etype      = WET_LINE_LOOP,
          .flags      = WEF_DRAW,
          .values     = m_xy_vals5,
          .num_values = D_ARRAY_SIZE(m_xy_vals5)
     },
     {
          .etype      = WET_TRIANGLE,
          .flags      = WEF_DRAW | WEF_FILL,
          .values     = m_xy_vals6,
          .num_values = D_ARRAY_SIZE(m_xy_vals6)
     },
     {
          .etype      = WET_TRIANGLE_FAN,
          .flags      = WEF_DRAW | WEF_FILL,
          .values     = m_xy_vals5,
          .num_values = D_ARRAY_SIZE(m_xy_vals5)
     },
     {
          .etype      = WET_TRIANGLE_STRIP,
          .flags      = WEF_DRAW | WEF_FILL,
          .values     = m_xy_vals5,
          .num_values = D_ARRAY_SIZE(m_xy_vals5)
     },
     {
          .etype      = WET_RECTANGLE,
          .flags      = WEF_DRAW | WEF_FILL,
          .values     = m_xywh_vals5,
          .num_values = D_ARRAY_SIZE(m_xywh_vals5)
     },
     {
          .etype      = WET_RECTANGLE_STRIP,
          .flags      = WEF_DRAW | WEF_FILL,
          .values     = m_xy_vals5,
          .num_values = D_ARRAY_SIZE(m_xy_vals5)
     },
#endif
     {
          .header.type  = WET_TRAPEZOID,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xyl_vals4,
          .num_values   = D_ARRAY_SIZE(m_xyl_vals4)
     },
     {
          .header.type  = WET_TRAPEZOID_STRIP,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xyl_vals4,
          .num_values   = D_ARRAY_SIZE(m_xyl_vals4)
     },
     {
          .header.type  = WET_QUADRANGLE,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xy_vals8,
          .num_values   = D_ARRAY_SIZE(m_xy_vals8)
     },
     {
          .header.type  = WET_QUADRANGLE_STRIP,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xy_vals6,
          .num_values   = D_ARRAY_SIZE(m_xy_vals6)
     },
     {
          .header.type  = WET_POLYGON,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xy_vals5,
          .num_values   = D_ARRAY_SIZE(m_xy_vals5)
     },

     /*
      * Advanced types
      */
     {
          .header.type  = WET_CIRCLE,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xyl_vals3,
          .num_values   = D_ARRAY_SIZE(m_xyl_vals3)
     },
     {
          .header.type  = WET_ELLIPSE,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_xyrxry_vals3,
          .num_values   = D_ARRAY_SIZE(m_xyrxry_vals3)
     },

     /* TODO: arcs and curves */
};

static DFBResult
Test_RenderElement( IWater *water, IDirectFBSurface *surface )
{
     int i;

     /*
      * Render elements
      */
     for (i=0; i<D_ARRAY_SIZE(m_test_elements); i++) {
          unsigned int index = WATER_ELEMENT_TYPE_INDEX( m_test_elements[i].header.type  );

          D_ASSERT( index < D_ARRAY_SIZE(m_element_types) );

          surface->Clear( surface, 0xc0, 0xb0, 0x90, 0xff );

          D_INFO( "Tests/Water: Testing RenderElement( %s )...\n", m_element_types[index].name );

          /*
           * Render element
           */
          _T( water->RenderElements( water, surface,
                                     &m_test_elements[i],
                                     1 ) );

          surface->Flip( surface, NULL, DSFLIP_NONE );
          sleep( 2 );
     }

     return DFB_OK;
}

static DFBResult
Test_RenderElements( IWater *water, IDirectFBSurface *surface )
{
     D_INFO( "Tests/Water: Testing RenderElements( %d )...\n", D_ARRAY_SIZE(m_test_elements) );

     /*
      * Render elements
      */
     _T( water->RenderElements( water, surface, m_test_elements, D_ARRAY_SIZE(m_test_elements) ) );

     return DFB_OK;
}

/**********************************************************************************************************************/

/* Values for the Rectangle Element */
static const WaterScalar m_test_shape_rect_vals[] = {
     { .i = 100 }, { .i = 100 },    /* 100,100 */
     { .i = 200 }, { .i = 200 },    /* 200x200 */
};

/* Values for the Triangle AND Line Strip Elements */
static const WaterScalar m_test_shape_tri_vals[] = {
     { .i = 150 }, { .i = 120 },    /* 150,120 */
     { .i = 200 }, { .i = 190 },    /* 200,190 */
     { .i = 100 }, { .i = 200 },    /* 100,200 */

     { .i = 250 }, { .i = 220 },    /* 250,220 */
     { .i = 300 }, { .i = 290 },    /* 300,290 */
     { .i = 200 }, { .i = 300 },    /* 200,300 */

     { .i = 350 }, { .i = 220 },    /* 350,220 */
     { .i = 400 }, { .i = 290 },    /* 400,290 */
     { .i = 300 }, { .i = 300 },    /* 300,300 */
};

/* -> Stream of Elements... */
static const WaterElement m_test_shape_elements[] = {
     {
          .header.type  = WET_RECTANGLE,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_test_shape_rect_vals,
          .num_values   = D_ARRAY_SIZE(m_test_shape_rect_vals)
     },
     {
          .header.type  = WET_TRIANGLE,
          .header.flags = WEF_DRAW | WEF_FILL,
          .values       = m_test_shape_tri_vals,
          .num_values   = D_ARRAY_SIZE(m_test_shape_tri_vals)
     },
     {
          .header.type  = WET_LINE_STRIP,
          .header.flags = WEF_DRAW,
          .values       = m_test_shape_tri_vals,
          .num_values   = D_ARRAY_SIZE(m_test_shape_tri_vals)
     },
};

static const WaterColor      m_test_shape_attributes_red_fill   = { 0xff, 0xff, 0x42, 0x23 };
static const WaterColor      m_test_shape_attributes_red_draw   = { 0xff, 0x23, 0x42, 0xff };
static const WaterRenderMode m_test_shape_attributes_red_render = WRM_ANTIALIAS;

/* -> Stream of Attributes... */
static const WaterAttribute m_test_shape_attributes_red[] = {
     {
          .header.flags = WAF_NONE,
          .header.type  = WAT_FILL_COLOR,
          .value        = &m_test_shape_attributes_red_fill
     },
     {
          .header.flags = WAF_NONE,
          .header.type  = WAT_DRAW_COLOR,
          .value        = &m_test_shape_attributes_red_draw
     },
     {
          .header.flags = WAF_NONE,
          .header.type  = WAT_RENDER_MODE,
          .value        = &m_test_shape_attributes_red_render
     },
};

static DFBResult
Test_RenderShape( IWater *water, IDirectFBSurface *surface )
{
     WaterShapeHeader header = {
          .flags = WSF_NONE,
     };

     D_INFO( "Tests/Water: Testing RenderShape()...\n" );

     /*
      * Render the shape defined by flags, opacity and a joint
      */
     _T( water->RenderShape( water, surface,
                             &header,
                             m_test_shape_attributes_red,
                             D_ARRAY_SIZE(m_test_shape_attributes_red),
                             m_test_shape_elements,
                             D_ARRAY_SIZE(m_test_shape_elements) ) );

     return DFB_OK;
}

/**********************************************************************************************************************/

/* Test shapes */
static const WaterShape m_test_shapes[] = {{
     .header.flags   = WSF_NONE,
     .attributes     = m_test_shape_attributes_red,
     .num_attributes = D_ARRAY_SIZE(m_test_shape_attributes_red),
     .elements       = m_test_shape_elements,
     .num_elements   = D_ARRAY_SIZE(m_test_shape_elements),
}};

static DFBResult
Test_RenderShapes( IWater *water, IDirectFBSurface *surface )
{
     D_INFO( "Tests/Water: Testing RenderShapes( %d )...\n", D_ARRAY_SIZE(m_test_shapes) );

     /*
      * Render the shapes defined in the array
      */
     _T( water->RenderShapes( water, surface, m_test_shapes, D_ARRAY_SIZE(m_test_shapes) ) );

     return DFB_OK;
}

