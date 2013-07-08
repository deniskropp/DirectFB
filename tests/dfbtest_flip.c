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

#include <direct/messages.h>

#include <direct/String.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>

static const DirectFBPixelFormatNames( format_names );

/**********************************************************************************************************************/

static DFBBoolean
parse_format( const char *arg, DFBSurfacePixelFormat *_f )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!direct_strcasecmp( arg, format_names[i].name )) {
               *_f = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static int
print_usage( const char *prg )
{
     int i = 0;

     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Flip Test (version %s) ==\n", DIRECTFB_VERSION);
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
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -d, --dest      <pixelformat>     Destination pixel format\n");
     fprintf (stderr, "  -t, --triple                      Use triple buffer\n");
     fprintf (stderr, "  -f, --frames                      Use frame base (instead of time base)\n");
     fprintf (stderr, "  -T, --frame-time                  Use frame time base (IDirectFBSurface::GetFrameTime)\n");
     fprintf (stderr, "  -n, --num                         Exit after num frames\n");

     return -1;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     int                     i;
     DFBSurfaceDescription   desc;
     IDirectFB              *dfb;
     IDirectFBSurface       *dest           = NULL;
     IDirectFBFont          *font           = NULL;
     DFBSurfacePixelFormat   dest_format    = DSPF_UNKNOWN;
     bool                    triple         = false;
     bool                    use_frame_time = false;
     bool                    frames         = false;
     long long               t0, count      = 0;
     long long               num            = 0;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Flip: DirectFBInit() failed!\n" );
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
          else if (strcmp (arg, "-d") == 0 || strcmp (arg, "--dest") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[i], &dest_format ))
                    return false;
          }
          else if (strcmp (arg, "-t") == 0 || strcmp (arg, "--triple") == 0) {
               triple = true;
          }
          else if (strcmp (arg, "-T") == 0 || strcmp (arg, "--frame-time") == 0) {
               use_frame_time = true;
          }
          else if (strcmp (arg, "-f") == 0 || strcmp (arg, "--frames") == 0) {
               frames = true;
          }
          else if (strcmp (arg, "-n") == 0 || strcmp (arg, "--num") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (sscanf( argv[i], "%lld", &num ) != 1) {
                    fprintf (stderr, "\nInvalid number specified!\n\n" );
                    return false;
               }
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Flip: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | (triple ? DSCAPS_TRIPLE : DSCAPS_DOUBLE);

     if (dest_format != DSPF_UNKNOWN) {
          desc.flags       |= DSDESC_PIXELFORMAT;
          desc.pixelformat  = dest_format;
     }

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &dest );
     if (ret) {
          D_DERROR( ret, "DFBTest/Flip: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     dest->GetSize( dest, &desc.width, &desc.height );
     dest->GetPixelFormat( dest, &desc.pixelformat );

     D_INFO( "DFBTest/Flip: Destination is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );

     /* Load the font. */
     DFBFontDescription fdesc = { .flags = DFDESC_HEIGHT, .height = 36 };
     ret = dfb->CreateFont( dfb, DATADIR "/decker.dgiff", &fdesc, &font );
     if (ret) {
          D_DERROR( ret, "DFBTest/Flip: IDirectFB::CreateFont( " DATADIR "/decker.dgiff ) failed!\n" );

          ret = dfb->CreateFont( dfb, DATADIR "/decker.ttf", &fdesc, &font );
          if (ret) {
               D_DERROR( ret, "DFBTest/Flip: IDirectFB::CreateFont( " DATADIR "/decker.ttf ) failed!\n" );
               goto out;
          }
     }
     dest->SetFont( dest, font );

     t0 = direct_clock_get_abs_millis();

     long long prev = 0;

     while (num <= 0 || count < num) {
//          long long t1, t2;
          long long base, frame_time = 0;
          long long now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          if (use_frame_time) {
               if (dest->GetFrameTime( dest, &frame_time ))
                    break;

               D_INFO( "Got frame time %lld (now %lld) with advance %lld (us in future)\n", frame_time, now, frame_time - now );

               base = frame_time * 5 / 17000;
          }
          else if (frames) {
               base = count * 5;
          }
          else {
               base = (direct_clock_get_abs_millis() - t0) * 5 / 17;
          }

          dest->Clear( dest, 0x55, 0x55, 0x55, 0xde );

          dest->SetColor( dest, 0xff, 0xff, 0xff, 0xff );
          dest->FillRectangle( dest, base % (desc.width - 100), 100, 100, 100 );

          dest->DrawString( dest, D_String_PrintTLS( "Frame Time: %lld (%lld from now)",
                                                     frame_time, frame_time - now ),
                            -1, 10, 10, DSTF_TOPLEFT );

          dest->DrawString( dest, D_String_PrintTLS( "Frame Diff: %lld (count %lld)",
                                                     frame_time - prev, count ),
                            -1, 10, 40, DSTF_TOPLEFT );

          prev = frame_time;

          //t1 = direct_clock_get_abs_millis();
          dest->Flip( dest, NULL, triple ? DSFLIP_ONSYNC : DSFLIP_WAITFORSYNC );
          //t2 = direct_clock_get_abs_millis();

          count++;

//          D_INFO( "Took %lld ms\n", t2 - t1 );
     }

out:
     if (font)
          font->Release( font );

     if (dest)
          dest->Release( dest );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

