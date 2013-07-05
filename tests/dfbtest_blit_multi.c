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
     fprintf (stderr, "== DirectFB Blitting Test (version %s) ==\n", DIRECTFB_VERSION);
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
     fprintf (stderr, "  -s, --source    <pixelformat>     Source pixel format\n");
     fprintf (stderr, "  -d, --dest      <pixelformat>     Destination pixel format\n");
     fprintf (stderr, "  -r, --resize                      Set destination from source size\n");
     fprintf (stderr, "  -b, --benchmark                   Enable benchmarking mode\n");
     fprintf (stderr, "  -R, --rerender                    Rerender before every blit (benchmark)\n");

     return -1;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     int                      i;
     DFBResult                ret;
     DFBSurfaceDescription    desc;
     IDirectFB               *dfb;
     IDirectFBImageProvider  *provider      = NULL;
     IDirectFBSurface       **sources       = NULL;
     IDirectFBSurface        *dest          = NULL;
     const char              *url           = NULL;
     DFBSurfacePixelFormat    source_format = DSPF_UNKNOWN;
     DFBSurfacePixelFormat    dest_format   = DSPF_UNKNOWN;
     bool                     dest_resize   = false;
     bool                     benchmark     = false;
     bool                     rerender      = false;
     int                      num_sources   = 100;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: DirectFBInit() failed!\n" );
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
          else if (strcmp (arg, "-s") == 0 || strcmp (arg, "--source") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_format( argv[i], &source_format ))
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
          else if (strcmp (arg, "-r") == 0 || strcmp (arg, "--resize") == 0)
               dest_resize = true;
          else if (strcmp (arg, "-b") == 0 || strcmp (arg, "--benchmark") == 0)
               benchmark = true;
          else if (strcmp (arg, "-R") == 0 || strcmp (arg, "--rerender") == 0)
               rerender = true;
          else if (!url)
               url = arg;
          else
               return print_usage( argv[0] );
     }

     /* Check if we got an URL. */
     if (!url)
          return print_usage( argv[0] );
          
     sources = malloc( sizeof(IDirectFBSurface*) * num_sources );

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Create an image provider for the image to be loaded. */
     ret = dfb->CreateImageProvider( dfb, url, &provider );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: IDirectFB::CreateImageProvider( '%s' ) failed!\n", url );
          goto out;
     }

     /* Get the surface description. */
     ret = provider->GetSurfaceDescription( provider, &desc );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: IDirectFBImageProvider::GetSurfaceDescription() failed!\n" );
          goto out;
     }

     if (source_format != DSPF_UNKNOWN)
          desc.pixelformat = source_format;
     
     D_INFO( "DFBTest/Blit: Source is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );

     desc.width  = 10;
     desc.height = 10;

     for (i=0; i<num_sources; i++) {
          /* Create a surface for the image. */
          ret = dfb->CreateSurface( dfb, &desc, &sources[i] );
          if (ret) {
               D_DERROR( ret, "DFBTest/Blit: IDirectFB::CreateSurface() failed!\n" );
               goto out;
          }
          
          ret = provider->RenderTo( provider, sources[i], NULL );
          if (ret) {
               D_DERROR( ret, "DFBTest/Blit: IDirectFBImageProvider::RenderTo() failed!\n" );
               goto out;
          }
     }

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     if (dest_format != DSPF_UNKNOWN) {
          desc.flags       |= DSDESC_PIXELFORMAT;
          desc.pixelformat  = dest_format;
     }

     if (dest_resize)
          desc.flags |= DSDESC_WIDTH | DSDESC_HEIGHT;

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &dest );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: IDirectFB::CreateSurface() failed!\n" );
          goto out;
     }

     dest->GetSize( dest, &desc.width, &desc.height );
     dest->GetPixelFormat( dest, &desc.pixelformat );

     D_INFO( "DFBTest/Blit: Destination is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );

     for (i=0; i<num_sources; i++)
          dest->StretchBlit( dest, sources[i], NULL, NULL );
     dest->Flip( dest, NULL, DSFLIP_NONE );

     while (benchmark) {
          int       num = 0;
          long long start, diff = 0, speed;

//          direct_sync();

//          direct_thread_sleep( 1000000 );

          for (i=0; i<num_sources; i++)
               dest->StretchBlit( dest, sources[i], NULL, NULL );

          D_INFO( "DFBTest/Blit: Benchmarking...\n" );

          dfb->WaitIdle( dfb );

          start = direct_clock_get_millis();

          do {
               if (rerender) {
                    for (i=0; i<num_sources; i++) {
                         ret = provider->RenderTo( provider, sources[i], NULL );
                         if (ret) {
                              D_DERROR( ret, "DFBTest/Blit: IDirectFBImageProvider::RenderTo() failed!\n" );
                              goto out;
                         }
                    }
               }

               for (i=0; i<num_sources; i++)
                    dest->StretchBlit( dest, sources[i], NULL, NULL );

               if ((num & 7) == 7)
                    diff = direct_clock_get_millis() - start;

               num++;
          } while (diff < 2300);

          num *= num_sources;

          dfb->WaitIdle( dfb );

          diff = direct_clock_get_millis() - start;

          speed = (long long) num * desc.width * desc.height / diff;

          D_INFO( "DFBTest/Blit: Speed is %lld.%03lld MPixel/sec (%dx%d x %d in %lld.%03lld sec)\n",
                  speed / 1000LL, speed % 1000LL, desc.width, desc.height, num, diff / 1000LL, diff % 1000LL );
     }
//     else
//          direct_thread_sleep( 2000000 );


out:
     if (dest)
          dest->Release( dest );

     for (i=0; i<num_sources; i++)
          if (sources[i])
               sources[i]->Release( sources[i] );

     if (provider)
          provider->Release( provider );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

