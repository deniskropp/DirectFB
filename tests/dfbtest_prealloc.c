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

static DFBSurfaceCapabilities static_caps;

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
     fprintf (stderr, "== DirectFB Fill Rectangle Test (version %s) ==\n", DIRECTFB_VERSION);
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
     fprintf (stderr, "  -s, --static                      Use DSCAPS_STATIC_ALLOC\n");

     return -1;
}

/**********************************************************************************************************************/

static void
gen_pixels( void *ptr,
            int   pitch,
            int   height )
{
     int y;

     for (y=0; y<height; y++) {
          memset( ptr + pitch * y, y*2, pitch );
     }
}

static void
update_pixels( void *ptr,
               int   pitch,
               int   height )
{
     int y;

     for (y=0; y<height; y++) {
          int  x;
          u8  *p = ptr + pitch * y;

          if (y==0) {
               printf("%u %u %u %u\n", p[0],p[1],p[2],p[3]);
          }
          for (x=0; x<pitch; x++)
               p[x] ^= 0xff;
     }
}

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     int                     i;
     DFBSurfaceDescription   desc;
     IDirectFB              *dfb;
     IDirectFBSurface       *dest          = NULL;
     DFBSurfacePixelFormat   dest_format   = DSPF_UNKNOWN;
     char                    pixel_buffer[100*100*4];
     IDirectFBSurface       *source        = NULL;
     IDirectFBEventBuffer   *keybuffer;
     DFBInputEvent           evt;
     bool                    quit          = false;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/PreAlloc: DirectFBInit() failed!\n" );
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
          else if (strcmp (arg, "-s") == 0 || strcmp (arg, "--static") == 0) {
               static_caps = DSCAPS_STATIC_ALLOC;
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/PreAlloc: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Fill description for a primary surface. */
     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     if (dest_format != DSPF_UNKNOWN) {
          desc.flags       |= DSDESC_PIXELFORMAT;
          desc.pixelformat  = dest_format;
     }

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create an input buffer for key events */
     dfb->CreateInputEventBuffer( dfb, DICAPS_KEYS,
                                  DFB_TRUE, &keybuffer);

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &dest );
     if (ret) {
          D_DERROR( ret, "DFBTest/PreAlloc: IDirectFB::CreateSurface() for the destination failed!\n" );
          goto out;
     }

     dest->GetSize( dest, &desc.width, &desc.height );
     dest->GetPixelFormat( dest, &desc.pixelformat );

     D_INFO( "DFBTest/PreAlloc: Destination is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );

     /* Create a preallocated surface. */
     desc.flags                 = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS | DSDESC_PREALLOCATED;
     desc.width                 = 100;
     desc.height                = 100;
     desc.pixelformat           = DSPF_ARGB;
     desc.caps                  = static_caps;
     desc.preallocated[0].data  = pixel_buffer;
     desc.preallocated[0].pitch = 100 * 4;

     ret = dfb->CreateSurface( dfb, &desc, &source );
     if (ret) {
          D_DERROR( ret, "DFBTest/PreAlloc: IDirectFB::CreateSurface() for the preallocated source failed!\n" );
          goto out;
     }

     /* Before any other operation the pixel data can be written to without locking */
     gen_pixels( pixel_buffer, 100 * 4, 100 );

     while (!quit) {
          void *ptr;
          int   pitch;


          /* Lock source surface for writing before making updates to the pixel buffer */
          source->Lock( source, DSLF_WRITE, &ptr, &pitch );

          if (ptr == pixel_buffer)
               D_INFO( "DFBTest/PreAlloc: Locking preallocated source gave original preallocated pixel buffer :-)\n" );
          else {
               if (static_caps)
                    D_INFO( "DFBTest/PreAlloc: Locking preallocated source gave different pixel buffer, ERROR with static alloc!\n" );
               else
                    D_INFO( "DFBTest/PreAlloc: Locking preallocated source gave different pixel buffer, but OK (no static alloc)\n" );
          }

          update_pixels( ptr, pitch, 100 );

          /* Unlock source surface after writing, before making further Blits,
             to have the buffer be transfered to master again */
          source->Unlock( source );


          dest->Clear( dest, 0, 0, 0, 0xff );

          /* First Blit from preallocated source, data will be transfered to master */
          dest->Blit( dest, source, NULL, 50, 50 );

          /* Second Blit from preallocated source, data is already master */
          dest->Blit( dest, source, NULL, 150, 150 );

          dest->Flip( dest, NULL, DSFLIP_NONE );

          /* This will upload again the preallocated buffer to the master, where it is
             modified and outdates the preallocated buffer. Now it depends on the static
             alloc flag whether the next Lock will directly go into the shared memory
             allocation or the preallocated buffer again (with a transfer back from master
             to us). */
          source->FillRectangle( source, 0, 0, 10, 10 );

          /* Process keybuffer */
          while (keybuffer->GetEvent( keybuffer, DFB_EVENT(&evt)) == DFB_OK)
          {
              if (evt.type == DIET_KEYPRESS) {
                  switch (DFB_LOWER_CASE(evt.key_symbol)) {
                      case DIKS_ESCAPE:
                      case DIKS_SMALL_Q:
                      case DIKS_BACK:
                      case DIKS_STOP:
                      case DIKS_EXIT:
                          /* Quit main loop & test thread */
                          quit = true;
                          break;
                      default:
                          break;
                  }
              }
          }
          if (!quit)
          sleep( 5 );
     }

out:
     if (source)
          source->Release( source );

     if (dest)
          dest->Release( dest );

     keybuffer->Release( keybuffer );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

