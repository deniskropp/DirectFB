/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <directfb.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>

#include <media/idirectfbfont.h>

#include <direct/hash.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/utf8.h>

#include <dgiff.h>

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBFont      *thiz,
           CoreDFB            *core,
           const char         *filename,
           DFBFontDescription *desc );

/**********************************************************************************************************************/

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFont, DGIFF )

/**********************************************************************************************************************/

typedef struct {
     void *map;     /* Memory map of the file. */
     int   size;    /* Size of the memory map. */
} DGIFFImplData;

/**********************************************************************************************************************/

static void
IDirectFBFont_DGIFF_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = thiz->priv;
     CoreFont           *font = data->font;
     DGIFFImplData      *impl = font->impl_data;

     munmap( impl->map, impl->size );

     D_FREE( impl );

     IDirectFBFont_Destruct( thiz );
}


static DFBResult
IDirectFBFont_DGIFF_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     if (--data->ref == 0) {
          IDirectFBFont_DGIFF_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx )
{
     DFBResult   ret = DFB_OK;
     int         fd;
     DGIFFHeader header;

     if (!ctx->filename)
          return DFB_UNSUPPORTED;

     /* Open the file. */
     fd = open( ctx->filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during open() of '%s'!\n", ctx->filename );
          goto out;
     }

     /* Read the header. */
     if (read( fd, &header, sizeof(header) ) != sizeof(header)) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure reading %zu bytes from '%s'!\n", sizeof(header), ctx->filename );
          goto out;
     }

     /* Check the magic. */
     if (strncmp( (char*) header.magic, "DGIFF", 5 ))
          ret = DFB_UNSUPPORTED;

out:
     if (fd >= 0)
          close( fd );

     return ret;
}

static DFBResult
Construct( IDirectFBFont      *thiz,
           CoreDFB            *core,
           const char         *filename,
           DFBFontDescription *desc )
{
     DFBResult        ret;
     int              i;
     int              fd;
     struct stat      stat;
     void            *ptr  = MAP_FAILED;
     CoreFont        *font = NULL;
     DGIFFHeader     *header;
     DGIFFFaceHeader *face;
     DGIFFGlyphInfo  *glyphs;
     DGIFFGlyphRow   *row;
     DGIFFImplData   *data;

//     if (desc->flags & (DFDESC_WIDTH | DFDESC_ATTRIBUTES | DFDESC_FIXEDADVANCE))
  //        return DFB_UNSUPPORTED;

     /* Open the file. */
     fd = open( filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during open() of '%s'!\n", filename );
          return ret;
     }

     /* Query file size etc. */
     if (fstat( fd, &stat ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during fstat() of '%s'!\n", filename );
          goto error;
     }

     /* Memory map the file. */
     ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (ptr == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during mmap() of '%s'!\n", filename );
          goto error;
     }

     /* Keep entry pointers for main header and face. */
     header = ptr;
     face   = ptr + sizeof(DGIFFHeader);

     /* Lookup requested face, otherwise use first if nothing requested or show error if not found. */
     if (desc->flags & DFDESC_HEIGHT) {
          for (i=0; i<header->num_faces; i++) {
               if (face->size == desc->height)
                    break;

               face = ((void*) face) + face->next_face;
          }

          if (i == header->num_faces) {
               ret = DFB_UNSUPPORTED;
               D_ERROR( "Font/DGIFF: Requested size %d not found in '%s'!\n", desc->height, filename );
               goto error;
          }
     }

     glyphs = (void*)(face + 1);
     row    = (void*)(glyphs + face->num_glyphs);

     /* Create the core object. */
     ret = dfb_font_create( core, &font );
     if (ret)
          goto error;

     /* Fill font information. */
     font->ascender     = face->ascender;
     font->descender    = face->descender;
     font->height       = face->height;

     font->maxadvance   = face->max_advance;
     font->pixel_format = face->pixelformat;

     font->num_rows     = face->num_rows;

     /* Allocate array for glyph cache rows. */
     font->rows = D_CALLOC( face->num_rows, sizeof(void*) );
     if (!font->rows) {
          ret = D_OOM();
          goto error;
     }

     /* Build glyph cache rows. */
     for (i=0; i<face->num_rows; i++) {
          font->rows[i] = D_CALLOC( 1, sizeof(CoreFontCacheRow) );
          if (!font->rows[i]) {
               ret = D_OOM();
               goto error;
          }

          ret = dfb_surface_create_preallocated( core, row->width, row->height, face->pixelformat,
                                                 CSP_VIDEOHIGH, DSCAPS_NONE, NULL, (void*)(row+1),
                                                 NULL, row->pitch, 0, &font->rows[i]->surface );
          if (ret) {
               D_DERROR( ret, "DGIFF/Font: Could not create preallocated %s %dx%d glyph row surface!\n",
                         dfb_pixelformat_name(face->pixelformat), row->width, row->height );
               goto error;
          }

          D_MAGIC_SET( font->rows[i], CoreFontCacheRow );

          /* Jump to next row. */
          row = (void*)(row + 1) + row->pitch * row->height;
     }

     /* Build glyph infos. */
     for (i=0; i<face->num_glyphs; i++) {
          CoreGlyphData  *data;
          DGIFFGlyphInfo *glyph = &glyphs[i];

          data = D_CALLOC( 1, sizeof(CoreGlyphData) );
          if (!data) {
               ret = D_OOM();
               goto error;
          }

          data->surface = font->rows[glyph->row]->surface;

          data->start   = glyph->offset;
          data->width   = glyph->width;
          data->height  = glyph->height;
          data->left    = glyph->left;
          data->top     = glyph->top;
          data->advance = glyph->advance;

          D_MAGIC_SET( data, CoreGlyphData );

          direct_hash_insert( font->glyph_hash, glyph->unicode, data );

          if (glyph->unicode < 128)
               font->glyph_data[glyph->unicode] = data;
     }


     data = D_CALLOC( 1, sizeof(DGIFFImplData) );
     if (!data) {
          ret = D_OOM();
          goto error;
     }

     data->map  = ptr;
     data->size = stat.st_size;

     font->impl_data = data;

     /* Already close, we still have the map. */
     close( fd );

     ret = IDirectFBFont_Construct( thiz, font );
     D_ASSERT( ret == DFB_OK );

     thiz->Release = IDirectFBFont_DGIFF_Release;

     return DFB_OK;


error:
     if (font) {
          if (font->rows) {
               for (i=0; i<font->num_rows; i++) {
                    if (font->rows[i]) {
                         if (font->rows[i]->surface)
                              dfb_surface_unref( font->rows[i]->surface );

                         D_FREE( font->rows[i] );
                    }
               }

               D_FREE( font->rows );

               font->rows = NULL;
          }

          dfb_font_destroy( font );
     }

     if (ptr != MAP_FAILED)
          munmap( ptr, stat.st_size );

     close( fd );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

