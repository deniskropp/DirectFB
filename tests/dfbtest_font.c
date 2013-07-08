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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <direct/messages.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>

static const DirectFBPixelFormatNames( format_names );

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     int i = 0;

     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Font Test (version %s) ==\n", DIRECTFB_VERSION);
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

     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options] <file>\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h,  --help                        Show this help message\n");
     fprintf (stderr, "  -v,  --version                     Print version information\n");
     fprintf (stderr, "  -o,  --outline                     Render outlined fonts\n");
     fprintf (stderr, "  -ow, --outline-width   <width>     Change outline width (default 1)\n");
     fprintf (stderr, "  -oo, --outline-opacity <opacity>   Change outline opacity (default 255)\n");

     return -1;
}

/**********************************************************************************************************************/

static IDirectFBFont *
CreateFont( IDirectFB *dfb, const char *url, int size, DFBFontAttributes attributes, int outline_width, int outline_opacity )
{
     DFBResult           ret;
     DFBFontDescription  fdesc;
     IDirectFBFont      *font;

     /* Create the font. */
     fdesc.flags           = DFDESC_HEIGHT | DFDESC_ATTRIBUTES | DFDESC_OUTLINE_WIDTH | DFDESC_OUTLINE_OPACITY;
     fdesc.height          = size;
     fdesc.attributes      = attributes;
     fdesc.outline_width   = outline_width;
     fdesc.outline_opacity = outline_opacity;

     ret = dfb->CreateFont( dfb, url, &fdesc, &font );
     if (ret) {
          D_DERROR( ret, "DFBTest/Font: IDirectFB::CreateFont( '%s' ) failed!\n", url );
          return NULL;
     }

     return font;
}

static void
RenderChecker( IDirectFBSurface *surface, int tile_width, int tile_height )
{
     int width, height;
     int x, y;

     surface->GetSize( surface, &width, &height );

     for (y=0; y<height; y+=tile_height) {
          for (x=0; x<width; x+=tile_width) {
               if ((x/tile_width + y/tile_height) & 1)
                    surface->SetColor( surface, 0x55, 0x55, 0x55, 0xff );
               else
                    surface->SetColor( surface, 0x99, 0x99, 0x99, 0xff );

               surface->FillRectangle( surface, x, y, tile_width, tile_height );
          }
     }
}

int
main( int argc, char *argv[] )
{
     int                    i;
     DFBResult              ret;
     DFBSurfaceDescription  desc;
     IDirectFB             *dfb;
     IDirectFBSurface      *dest            = NULL;
     const char            *url             = NULL;
     DFBFontAttributes      attributes      = DFFA_NONE;
     DFBSurfaceTextFlags    text_flags      = DSTF_TOPLEFT;
     int                    outline_width   = 0x10000;
     int                    outline_opacity = 255;
     const DFBColorID       color_ids[2]    = { DCID_PRIMARY, DCID_OUTLINE };
     const DFBColor         colors[2]       = { { 0xff, 0xff, 0xff, 0xff },
                                                { 0xff, 0x00, 0x80, 0xff } };

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Font: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_blit version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else if (strcmp (arg, "-o") == 0 || strcmp (arg, "--outline") == 0) {
               attributes |= DFFA_OUTLINED;
               text_flags |= DSTF_OUTLINE;
          }
          else if (strcmp (arg, "-ow") == 0 || strcmp (arg, "--outline-width") == 0) {
               if (++i == argc)
                    return print_usage( argv[0] );

               if (sscanf( argv[i], "%d", &outline_width ) != 1)
                    return print_usage( argv[0] );

               outline_width <<= 16;
          }
          else if (strcmp (arg, "-oo") == 0 || strcmp (arg, "--outline-opacity") == 0) {
               if (++i == argc)
                    return print_usage( argv[0] );

               if (sscanf( argv[i], "%d", &outline_opacity ) != 1)
                    return print_usage( argv[0] );
          }
          else if (!url)
               url = arg;
          else
               return print_usage( argv[0] );
     }

     /* Check if we got an URL. */
     if (!url)
          return print_usage( argv[0] );

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Font: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &dest );
     if (ret) {
          D_DERROR( ret, "DFBTest/Font: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     dest->GetSize( dest, &desc.width, &desc.height );
     dest->GetPixelFormat( dest, &desc.pixelformat );

     D_INFO( "DFBTest/Font: Destination is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );

     for (i=10; i<50; i++) {
          IDirectFBFont *font;

          font = CreateFont( dfb, url, i, attributes, outline_width, outline_opacity );

          RenderChecker( dest, 64, 64 );

          dest->SetColors( dest, color_ids, colors, 2 );

          dest->SetFont( dest, font );
          dest->DrawString( dest, "Test String AVAWA", -1, 100, 100, text_flags );

          dest->Flip( dest, NULL, DSFLIP_NONE );

          font->Release( font );

          usleep( 500000 );
     }


out:
     if (dest)
          dest->Release( dest );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

