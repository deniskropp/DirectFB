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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <png.h>

#include <directfb.h>

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

static struct {
     DFBSurfacePixelFormat  format;
     const char             *name;
} pixelformat_names[] = {
     { DSPF_RGB32, "DSPF_RGB32" },
     { DSPF_ARGB,  "DSPF_ARGB"  }
};
static int n_pixelformats = (sizeof (pixelformat_names) /
                             sizeof (pixelformat_names[0]));


static DFBResult  load_image  (DFBSurfaceDescription *desc,
                               const char            *filename);
static DFBResult  dump_image  (DFBSurfaceDescription *desc,
                               const char            *name);
static void       print_usage (const char            *prg_name);


int main (int         argc,
          const char *argv[])
{
     DFBSurfaceDescription  desc;
     const char *name     = NULL;
     const char *filename = NULL;
     char *vname;
     int   i, n;

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

     if (load_image (&desc, filename) != DFB_OK)
          return EXIT_FAILURE;

     return dump_image (&desc, vname);
}

static void print_usage (const char *prg_name)
{
     fprintf (stderr, "directfb-csource version %s\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options] <imagefile>\n", prg_name);
     fprintf (stderr, "   --name=<identifer>  specifies variable name\n");
     fprintf (stderr, "   --help              show this help message\n");
     fprintf (stderr, "   --version           print version information\n");
     fprintf (stderr, "\n");
}

static DFBResult load_image (DFBSurfaceDescription *desc,
                             const char            *filename)
{
     FILE        *fp;
     png_structp  png_ptr  = NULL;
     png_infop    info_ptr = NULL;
     png_uint_32  width, height;
     int          type;
     char         header[8];
     int          bytes;

     desc->flags = 0;
     desc->preallocated[0].data = NULL;

     if (!(fp = fopen (filename, "rb"))) {
          fprintf (stderr, "Couldn't open file '%s': %s.\n",
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

     /* promote everything to ARGB or RGB32 (for now) */
     switch (type)
       {
       case PNG_COLOR_TYPE_PALETTE:
         png_set_palette_to_rgb (png_ptr);
         break;
       case PNG_COLOR_TYPE_GRAY:
       case PNG_COLOR_TYPE_GRAY_ALPHA:
         png_set_gray_to_rgb (png_ptr);
         break;
       }

     if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
          png_set_tRNS_to_alpha (png_ptr);

     if (bytes == 16)
          png_set_strip_16 (png_ptr);

#if __BYTE_ORDER == __BIG_ENDIAN
     if (!(type & PNG_COLOR_MASK_ALPHA))
          png_set_filler (png_ptr, 0xFF, PNG_FILLER_BEFORE);

     png_set_swap_alpha (png_ptr);
#else
     if (!(type & PNG_COLOR_MASK_ALPHA))
          png_set_filler (png_ptr, 0xFF, PNG_FILLER_AFTER);

     png_set_bgr (png_ptr);
#endif

     desc->width       = width;
     desc->height      = height;
     desc->pixelformat = ((type & PNG_COLOR_MASK_ALPHA) ?
                          DSPF_ARGB : DSPF_RGB32);

     desc->preallocated[0].pitch = (width *
                                    DFB_BYTES_PER_PIXEL (desc->pixelformat));
     desc->preallocated[0].data = malloc (height * desc->preallocated[0].pitch);

     if (desc->preallocated[0].data) {
          int i;
          png_bytep bptrs[height];
          
          for (i = 0; i < height; i++)
               bptrs[i] = ((unsigned char *) desc->preallocated[0].data
                           + desc->preallocated[0].pitch * i);

          png_read_image (png_ptr, bptrs);
     }

     desc->flags = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |
                    DSDESC_PREALLOCATED);

 cleanup:
     if (fp)
          fclose (fp);

     if (png_ptr)
          png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

     return ((desc->flags) ? DFB_OK : DFB_FAILURE);
}


typedef struct {
     FILE  *fp;
     int     pos;
     int     pad;
} CSourceData;


static inline void
save_uchar (CSourceData   *csource,
	    unsigned char  d)
{
     if (csource->pos > 70) {
          fprintf (csource->fp, "\"\n  \"");

          csource->pos = 3;
          csource->pad = FALSE;
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
     csource->pad = FALSE;

     return;
}

static DFBResult dump_image (DFBSurfaceDescription *desc,
                             const char            *name)
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
          if (pixelformat_names[i].format == desc->pixelformat)
               format = pixelformat_names[i].name;

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

     /* dump description */
     fprintf (csource.fp,
              "static DFBSurfaceDescription %s_desc = {\n", name);
     fprintf (csource.fp,
              "  flags                   : (DSDESC_WIDTH | DSDESC_HEIGHT |\n"
              "                             DSDESC_PIXELFORMAT | DSDESC_PREALLOCATED),\n");
     fprintf (csource.fp,
              "  width                   : %d,\n", desc->width);
     fprintf (csource.fp,
              "  height                  : %d,\n", desc->height);
     fprintf (csource.fp,
              "  pixelformat             : %s,\n", format);
     fprintf (csource.fp,
              "  preallocated : {{ data  : (void *) %s_data,\n", name);
     fprintf (csource.fp,
              "                    pitch : %d\n", desc->preallocated[0].pitch);
     fprintf (csource.fp, "  }}\n};\n\n");


     return EXIT_SUCCESS;
}
