/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   directfb-csource is based on gdk-pixbuf-csource, a GdkPixbuf
   based image CSource generator  Copyright (C) 1999, 2001 Tim Janik

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

#include "config.h"

#include <endian.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>

#include <png.h>

#include "directfb.h"

#include "gfx/convert.h"
#include "misc/util.h"


static struct {
     DFBSurfacePixelFormat  format;
     const char             *name;
} pixelformats[] = {
     { DSPF_ARGB,   "ARGB"   },
     { DSPF_RGB32,  "RGB32"  },
     { DSPF_RGB24,  "RGB24"  },
     { DSPF_RGB16,  "RGB16"  },
     { DSPF_RGB15,  "RGB15"  },
     { DSPF_RGB332, "RGB332" },
     { DSPF_A8,     "A8"     },
     { DSPF_LUT8,   "LUT8"   }
};
static int n_pixelformats = sizeof (pixelformats) / sizeof (pixelformats[0]);


static DFBResult  load_image  (const char            *filename,
                               DFBSurfaceDescription *desc,
                               DFBColor              *palette,
                               int                   *palette_size);
static DFBResult  dump_image  (const char            *name,
                               DFBSurfaceDescription *desc,
                               DFBColor              *palette,
                               int                    palette_size);
static void       print_usage (const char            *prg_name);


int main (int         argc,
          const char *argv[])
{
     DFBSurfaceDescription  desc    = { 0 };
     DFBSurfacePixelFormat  format  = DSPF_UNKNOWN;
     DFBColor    palette[256];
     int         palette_size = 0;
     const char *name         = NULL;
     const char *filename     = NULL;
     char *vname;
     int   i, j, n;

     /* parse command line */
     for (n = 1; n < argc; n++) {
          if (strncmp (argv[n], "--", 2) == 0) {
               if (strcmp (argv[n] + 2, "help") == 0) {
                    print_usage (argv[0]);
                    return EXIT_SUCCESS;
               }
               if (strcmp (argv[n] + 2, "version") == 0) {
                    fprintf (stderr, "directfb-csource version %s\n",
                             DIRECTFB_VERSION);
                    return EXIT_SUCCESS;
               }
               if (strncmp (argv[n] + 2, "format=", 7) == 0 && !format) {
                    for (j = 0; j < n_pixelformats && !format; j++)
                         if (!strcasecmp (pixelformats[j].name, argv[n] + 9))
                              format = pixelformats[j].format;
                    if (format)
                         continue;
               }
               if (strncmp (argv[n] + 2, "name=", 5) == 0 && !name) {
                    name = argv[n] + 7;
                    if (*name)
                         continue;
               }
               filename = ""; /* bail out below */
          }
          if (filename) {
               print_usage (argv[0]);
               return EXIT_FAILURE;
          }
          filename = argv[n];
     }

     if (!filename) {
          print_usage (argv[0]);
          return EXIT_FAILURE;
     }

     if (name)
          vname = strdup (name);
     else
          vname = basename (strdup (filename));

     for (i = 0; vname[i]; i++) {
          switch (vname[i]) {
               case 'a'...'z':
               case 'A'...'Z':
               case '0'...'9':
               case '_':
                    break;
               default:
                    vname[i] = '_';
          }
     }

     if (format) {
          desc.flags = DSDESC_PIXELFORMAT;
          desc.pixelformat = format;
     }

     if (load_image (filename, &desc, palette, &palette_size) != DFB_OK)
          return EXIT_FAILURE;

     return dump_image (vname, &desc, palette, palette_size);
}

static void print_usage (const char *prg_name)
{
     fprintf (stderr, "directfb-csource version %s\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options] <imagefile>\n", prg_name);
     fprintf (stderr, "   --name=<identifer>     specifies variable name\n");
     fprintf (stderr, "   --format=<identifer>   specifies surface format\n");
     fprintf (stderr, "   --help                 show this help message\n");
     fprintf (stderr, "   --version              print version information\n");
     fprintf (stderr, "\n");
}

static DFBResult load_image (const char            *filename,
                             DFBSurfaceDescription *desc,
                             DFBColor              *palette,
                             int                   *palette_size)
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
     if (png_sig_cmp (header, 0, bytes)) {
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

#if __BYTE_ORDER == __BIG_ENDIAN
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
               break;

          case PNG_COLOR_TYPE_PALETTE:
               if (dest_format == DSPF_LUT8) {
                    src_format = DSPF_LUT8;
                    break;
               }
               png_set_palette_to_rgb (png_ptr);
               /* fallthru */
          case PNG_COLOR_TYPE_RGB:
          case PNG_COLOR_TYPE_RGB_ALPHA:
               if (dest_format == DSPF_RGB24) {
                    png_set_strip_alpha (png_ptr);
                    src_format = DSPF_RGB24;
               }
               break;
       }

     switch (src_format) {
          case DSPF_LUT8:
               if (info_ptr->num_palette) {
                    png_byte *alpha;
                    int       i, num;

                    *palette_size = MIN (info_ptr->num_palette, 256);
                    for (i = 0; i < *palette_size; i++) {
                         palette[i].a = 0xFF;
                         palette[i].r = info_ptr->palette[i].red;
                         palette[i].g = info_ptr->palette[i].green;
                         palette[i].b = info_ptr->palette[i].blue;
                    }
                    if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS)) {
                         png_get_tRNS (png_ptr, info_ptr, &alpha, &num, NULL);
                         for (i = 0; i < MIN (num, *palette_size); i++)
                              palette[i].a = alpha[i];
                    }
               }
               break;
          case DSPF_RGB32:
                png_set_filler (png_ptr, 0xFF,
#if __BYTE_ORDER == __BIG_ENDIAN
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

     pitch = width * DFB_BYTES_PER_PIXEL (src_format);
     if (pitch & 3)
          pitch += 4 - (pitch & 3);

     data  = malloc (height * pitch);
     if (!data) {
          fprintf (stderr, "Failed to allocate %ld bytes.\n",
                   height * pitch);
          goto cleanup;
     }

     {
          int i;
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

          assert (DFB_BYTES_PER_PIXEL (src_format) == 4);
          
          d_pitch = width * DFB_BYTES_PER_PIXEL (dest_format);
          if (d_pitch & 3)
               d_pitch += 4 - (d_pitch & 3);

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
                         span_rgb32_to_rgb16 ((__u32 *) s, (__u16 *) d, width);
                    break;
               case DSPF_RGB15:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         span_rgb32_to_rgb15 ((__u32 *) s, (__u16 *) d, width);
                    break;
               case DSPF_RGB332:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         span_rgb32_to_rgb332 ((__u32 *) s, (__u8 *) d, width);
                    break;
               case DSPF_A8:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         span_argb_to_a8 ((__u32 *) s, (__u8 *) d, width);
                    break;
               default:
                    fprintf (stderr, "Sorry, unsupported format conversion.\n");
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


typedef struct {
     FILE  *fp;
     int     pos;
     bool    pad;
} CSourceData;


static inline void
save_uchar (CSourceData   *csource,
	    unsigned char  d)
{
     if (csource->pos > 70) {
          fprintf (csource->fp, "\"\n  \"");

          csource->pos = 3;
          csource->pad = false;
     }
     if (d < 33 || d > 126) {
          fprintf (csource->fp, "\\%o", d);
          csource->pos += 1 + 1 + (d > 7) + (d > 63);
          csource->pad = d < 64;
          return;
     }
     if (d == '\\') {
          fprintf (csource->fp, "\\\\");
          csource->pos += 2;
     }
     else if (d == '"') {
          fprintf (csource->fp, "\\\"");
          csource->pos += 2;
     }
     else if (csource->pad && d >= '0' && d <= '9') {
          fprintf (csource->fp, "\"\"%c", d);
          csource->pos += 3;
     }
     else {
          fputc (d, csource->fp);
          csource->pos += 1;
     }
     csource->pad = false;

     return;
}

static DFBResult dump_image (const char            *name,
                             DFBSurfaceDescription *desc,
                             DFBColor              *palette,
                             int                    palette_size)
{
     CSourceData    csource = { stdout, 0, 0 };
     const char    *format  = NULL;
     unsigned char *data;
     unsigned long  len;
     unsigned int   i;

     if (desc->flags != (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |
                         DSDESC_PREALLOCATED))
          return DFB_INVARG;

     for (i = 0; i < n_pixelformats && !format; i++)
          if (pixelformats[i].format == desc->pixelformat)
               format = pixelformats[i].name;

     if (!format)
          return DFB_INVARG;

     data = (unsigned char *) desc->preallocated[0].data;
     len = desc->height * desc->preallocated[0].pitch;

     if (!len)
          return DFB_INVARG;

     /* dump comment */
     fprintf (csource.fp,
              "/* DirectFB surface dump created by directfb-csource %s */\n\n",
              DIRECTFB_VERSION);

     /* dump data */
     fprintf (csource.fp,
              "static unsigned char %s_data[] = \n", name);
     fprintf (csource.fp, "  \"");

     csource.pos = 3;
     do
          save_uchar (&csource, *data++);
     while (--len);

     fprintf (csource.fp, "\";\n\n");

     /* dump palette */
     if (palette_size > 0) {
          fprintf (csource.fp,
                   "static DFBColor %s_palette[%d] = {\n", name, palette_size);
          for (i = 0; i < palette_size; i++)
               fprintf (csource.fp,
                        "  { 0x%02x, 0x%02x, 0x%02x, 0x%02x }%c\n",
                        palette[i].a, palette[i].r, palette[i].g, palette[i].b,
                        i+1 < palette_size ? ',' : ' ');
          fprintf (csource.fp, "};\n\n");
     }

     /* dump description */
     fprintf (csource.fp,
              "static DFBSurfaceDescription %s_desc = {\n", name);
     fprintf (csource.fp,
              "  flags                   : DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |\n"
              "                            DSDESC_PREALLOCATED");
     if (palette_size > 0)
          fprintf (csource.fp, " | DSDESC_PALETTE");
     fprintf (csource.fp, ",\n");     
     fprintf (csource.fp,
              "  width                   : %d,\n", desc->width);
     fprintf (csource.fp,
              "  height                  : %d,\n", desc->height);
     fprintf (csource.fp,
              "  pixelformat             : DSPF_%s,\n", format);
     fprintf (csource.fp,
              "  preallocated : {{  data : (void *) %s_data,\n", name);
     fprintf (csource.fp,
              "                    pitch : %d  }}", desc->preallocated[0].pitch);
     if (palette_size > 0) {
          fprintf (csource.fp, ",\n");
          fprintf (csource.fp,
                   "  palette :    {  entries : %s_palette,\n", name);
          fprintf (csource.fp,
                   "                     size : %d  }", palette_size);
     }
     fprintf (csource.fp, "\n};\n\n");

     return DFB_OK;
}
