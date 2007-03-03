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

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <png.h>

#include <directfb.h>
#include <directfb_strings.h>

#include <direct/debug.h>

#include <gfx/convert.h>

#include <dfiff.h>

static DirectFBPixelFormatNames( format_names );

static const char            *filename;
static DFBSurfacePixelFormat  format    = DSPF_UNKNOWN;
static DFBSurfacePixelFormat  rgbformat = DSPF_UNKNOWN;

/**********************************************************************************************************************/

static DFBResult
load_image (const char            *filename,
            DFBSurfaceDescription *desc)
{
     DFBSurfacePixelFormat dest_format;
     DFBSurfacePixelFormat src_format;
     FILE          *fp;
     png_structp    png_ptr  = NULL;
     png_infop      info_ptr = NULL;
     png_uint_32    width, height;
     unsigned char *data     = NULL;
     int            type;
     char           header[8];
     int            bytes, pitch;

     dest_format =
       (desc->flags & DSDESC_PIXELFORMAT) ? desc->pixelformat : DSPF_UNKNOWN;

     desc->flags = 0;
     desc->preallocated[0].data = NULL;

     if (!(fp = fopen (filename, "rb"))) {
          fprintf (stderr, "Failed to open file '%s': %s.\n",
                   filename, strerror (errno));
          goto cleanup;
     }

     bytes = fread (header, 1, sizeof(header), fp);
     if (png_sig_cmp ((unsigned char*) header, 0, bytes)) {
          fprintf (stderr, "File '%s' doesn't seem to be a PNG image file.\n",
                   filename);
          goto cleanup;
     }

     png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL);
     if (!png_ptr)
          goto cleanup;

     if (setjmp (png_ptr->jmpbuf)) {
          if (desc->preallocated[0].data) {
               free (desc->preallocated[0].data);
               desc->preallocated[0].data = NULL;
          }

          /* data might have been clobbered,
             set it to NULL and leak instead of crashing */
          data = NULL;

          goto cleanup;
     }

     info_ptr = png_create_info_struct (png_ptr);
     if (!info_ptr)
          goto cleanup;

     png_init_io (png_ptr, fp);
     png_set_sig_bytes (png_ptr, bytes);

     png_read_info (png_ptr, info_ptr);

     png_get_IHDR (png_ptr, info_ptr,
                   &width, &height, &bytes, &type, NULL, NULL, NULL);

     if (bytes == 16)
          png_set_strip_16 (png_ptr);

#ifdef WORDS_BIGENDIAN
     png_set_swap_alpha (png_ptr);
#else
     png_set_bgr (png_ptr);
#endif

     src_format = (type & PNG_COLOR_MASK_ALPHA) ? DSPF_ARGB : DSPF_RGB32;
     switch (type) {
          case PNG_COLOR_TYPE_GRAY:
               if (dest_format == DSPF_A8) {
                    src_format = DSPF_A8;
                    break;
               }
               /* fallthru */
          case PNG_COLOR_TYPE_GRAY_ALPHA:
               png_set_gray_to_rgb (png_ptr);
               if (rgbformat)
                    dest_format = rgbformat;
               break;

          case PNG_COLOR_TYPE_PALETTE:
               png_set_palette_to_rgb (png_ptr);
               /* fallthru */
          case PNG_COLOR_TYPE_RGB:
               if (rgbformat)
                    dest_format = rgbformat;
          case PNG_COLOR_TYPE_RGB_ALPHA:
               if (dest_format == DSPF_RGB24) {
                    png_set_strip_alpha (png_ptr);
                    src_format = DSPF_RGB24;
               }
               break;
       }

     switch (src_format) {
          case DSPF_RGB32:
                png_set_filler (png_ptr, 0xFF,
#ifdef WORDS_BIGENDIAN
                                PNG_FILLER_BEFORE
#else
                                PNG_FILLER_AFTER
#endif
                                );
                break;
          case DSPF_ARGB:
          case DSPF_A8:
               if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
                    png_set_tRNS_to_alpha (png_ptr);
               break;
          default:
               break;
     }

     pitch = (DFB_BYTES_PER_LINE( src_format, width ) + 7) & ~7;

     data  = malloc (height * pitch);
     if (!data) {
          fprintf (stderr, "Failed to allocate %ld bytes.\n", height * pitch);
          goto cleanup;
     }

     {
          unsigned int i;
          png_bytep bptrs[height];

          for (i = 0; i < height; i++)
               bptrs[i] = data + i * pitch;

          png_read_image (png_ptr, bptrs);
     }

     if (!dest_format)
          dest_format = src_format;

     if (DFB_BYTES_PER_PIXEL(src_format) != DFB_BYTES_PER_PIXEL(dest_format)) {
          unsigned char *s, *d, *dest;
          int            d_pitch, h;

          D_ASSERT( DFB_BYTES_PER_PIXEL(src_format) == 4 );

          d_pitch = (DFB_BYTES_PER_LINE(dest_format, width) + 7) & ~7;

          dest = malloc (height * d_pitch);
          if (!dest) {
               fprintf (stderr, "Failed to allocate %ld bytes.\n",
                        height * d_pitch);
               goto cleanup;
          }

          h = height;
          switch (dest_format) {
               case DSPF_RGB16:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_rgb16 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB1555:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb1555 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB2554:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb2554 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB4444:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb4444 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_RGB332:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_rgb332 ((u32 *) s, (u8 *) d, width);
                    break;
               case DSPF_A8:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_a8 ((u32 *) s, (u8 *) d, width);
                    break;
               default:
                    fprintf (stderr,
                             "Sorry, unsupported format conversion.\n");
                    goto cleanup;
          }

          free (data);
          data = dest;
          pitch = d_pitch;
     }

     desc->flags = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |
                    DSDESC_PREALLOCATED);
     desc->width       = width;
     desc->height      = height;
     desc->pixelformat = dest_format;
     desc->preallocated[0].pitch = pitch;
     desc->preallocated[0].data  = data;

     data = NULL;

 cleanup:
     if (fp)
          fclose (fp);

     if (png_ptr)
          png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

     if (data)
          free (data);

     return ((desc->flags) ? DFB_OK : DFB_FAILURE);
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     int i = 0;

     fprintf (stderr, "\nDirectFB Fast Image File Format Tool (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -f, --format    <pixelformat>   Choose the pixel format (in all cases)\n");
     fprintf (stderr, "   -r, --rgbformat <pixelformat>   Choose the pixel format (in case of RGB)\n");
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
parse_format( const char *arg, DFBSurfacePixelFormat *ret_format )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               *ret_format = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "mkdfiff version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_format( argv[n], &format ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-r") == 0 || strcmp (arg, "--rgbformat") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_format( argv[n], &rgbformat ))
                    return DFB_FALSE;

               continue;
          }

          if (filename || access( arg, R_OK )) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          filename = arg;
     }

     if (!filename) {
          print_usage (argv[0]);
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

/**********************************************************************************************************************/

static DFIFFHeader header = {
     magic: { 'D', 'F', 'I', 'F', 'F' },
     major: 0,
     minor: 0,
     flags: DFIFF_FLAG_LITTLE_ENDIAN
};

int
main( int argc, char *argv[] )
{
     int                   i;
     DFBSurfaceDescription desc = { flags: DSDESC_NONE };

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -1;

     if (format != DSPF_UNKNOWN) {
          desc.flags       |= DSDESC_PIXELFORMAT;
          desc.pixelformat  = format;
     }

     if (load_image( filename, &desc ))
          return -2;

     for (i=0; i<D_ARRAY_SIZE(format_names); i++) {
          if (format_names[i].format == desc.pixelformat) {
               fprintf( stderr, "Writing %dx%d %s image...\n", desc.width, desc.height,
                        format_names[i].name );
               break;
          }
     }

     header.width  = desc.width;
     header.height = desc.height;
     header.format = desc.pixelformat;
     header.pitch  = desc.preallocated[0].pitch;

     fwrite( &header, sizeof(header), 1, stdout );

     fwrite( desc.preallocated[0].data, header.pitch, header.height, stdout );

     return 0;
}
