/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>

#include <freetype/ftglyph.h>

#include <directfb.h>

#include <core/coredefs.h>

#include <gfx/convert.h>

#include <media/idirectfbfont.h>

#define FTLOADFLAGS     (FT_LOAD_DEFAULT /*| FT_LOAD_NO_HINTING*/)

static FT_Library library = NULL;


char *get_type()
{
     return "IDirectFBFont";
}

char *get_implementation()
{
     return "FT2";
}

DFBResult Probe( void *data )
{
     return DFB_OK;
}

/*
 * bad code for TrueType font loading, rewrite with one loop only
 * and usage of the various API shortcuts of FreeType 2
 */
DFBResult Construct( IDirectFBFont *thiz,
                     const char *filename,
                     DFBFontDescription *desc )
{
     __u8 *dst;
     int i;
     int surface_width = 0;
     FT_Error err;
     FT_Face  face;
     CoreSurface *surface;
     CoreFontData *font;
     int start = 0;
     int pitch;

     if (!library) {
          err = FT_Init_FreeType( &library );
          if (err) {
               ERRORMSG( "DirectFB/FontFT2: "
                         "Initializing the FreeType Library failed!\n" );
               return DFB_FAILURE;
          }
     }

     err = FT_New_Face( library, filename, 0, &face );
     if (err) {
          ERRORMSG( "DirectFB/FontFT2: "
                    "Failed loading font file `%s'!\n", filename );
          return DFB_FAILURE;
     }

     if (desc->flags & DFDESC_HEIGHT) {
          err = FT_Set_Pixel_Sizes( face, 0, desc->height );
          if (err) {
               ERRORMSG( "DirectB/FontFT2: "
                         "Could not set pixel size to %d!\n", desc->height );
               FT_Done_Face( face );
               return DFB_FAILURE;
          }
     }

     font = (CoreFontData*)malloc( sizeof(CoreFontData) );
     memset( font, 0, sizeof(CoreFontData) );

     for (i=32; i<128; i++) {
          int index = FT_Get_Char_Index(face, i);

          err = FT_Load_Glyph( face, index, FTLOADFLAGS );
          if (err) {
               ERRORMSG( "DirectB/FontFT2: "
                         "Could not load glyph for character #%02x!\n", i );
               free( font );
               FT_Done_Face( face );
               return DFB_FAILURE;
          }

          if (face->glyph->format != ft_glyph_format_bitmap) {
               FT_Render_Glyph( face->glyph, ft_render_mode_normal );
               if (err) {
                    ERRORMSG( "DirectB/FontFT2: Could not "
                              "render glyph for character #%02x!\n", i );
                    free( font );
                    FT_Done_Face( face );
                    return DFB_FAILURE;
               }
          }

          HEAVYDEBUGMSG( "Rendered character #%02x: bitmap size %3dx%3d\n", i,
                         face->glyph->bitmap.width, face->glyph->bitmap.rows );

          font->glyphs[i].width   = face->glyph->bitmap.width;
          font->glyphs[i].height  = face->glyph->bitmap.rows;
          font->glyphs[i].advance = face->glyph->advance.x >> 6;

          surface_width += font->glyphs[i].width;

          if (font->glyphs[i].width > font->maxadvance)
               font->maxadvance = font->glyphs[i].width;
     }

     font->ascender  = face->size->metrics.ascender  >> 6;
     font->descender = face->size->metrics.descender >> 6;
     font->height    = (face->size->metrics.ascender -
                        face->size->metrics.descender) >> 6;

     HEAVYDEBUGMSG( "DirectFB/FontFT2: surface_width = %d\n", surface_width );
     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->height = %d\n", font->height );
     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->ascender = %d\n", font->ascender);
     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->descender = %d\n",font->descender);

     if (config->argb_font) {
          err = surface_create( surface_width, font->height, DSPF_ARGB,
                                CSP_VIDEOHIGH, DSCAPS_ALPHA, &surface );
     }
     else {
          err = surface_create( surface_width, font->height, DSPF_A8,
                                CSP_VIDEOHIGH, DSCAPS_ALPHA, &surface );
     }
     if (err) {
          ERRORMSG( "DirectB/FontFT2: "
                    "Unable to create font map surface with size %dx%d!\n",
                    surface_width, font->height );
          free( font );
          FT_Done_Face( face );
          return err;
     }


     err = surface_soft_lock( surface, DSLF_WRITE, (void**)&dst, &pitch, 0 );
     if (err) {
          ERRORMSG( "DirectB/FontFT2: Unable to lock surface!\n" );
          free( font );
          surface_destroy( surface );
          FT_Done_Face( face );
          return err;
     }
     memset( dst, 0, pitch * surface->height );

     for (i=32; i<128; i++) {
          int index = FT_Get_Char_Index(face, i);
          int y, c;
          __u8 *d = dst;

          err = FT_Load_Glyph( face, index, FTLOADFLAGS );
          if (err) {
               ERRORMSG( "DirectB/FontFT2: "
                         "Could not load glyph for character #%02x!\n", i );
               free( font );
               surface_unlock( surface, 0 );
               surface_destroy( surface );
               FT_Done_Face( face );
               return DFB_FAILURE;
          }

          if (face->glyph->format != ft_glyph_format_bitmap) {
               err = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
               if (err) {
                    ERRORMSG( "DirectFB/FontFT2: Could not "
                              "render glyph for character #%02x!\n", i );
                    free( font );
                    surface_unlock( surface, 0 );
                    surface_destroy( surface );
                    FT_Done_Face( face );
                    return DFB_FAILURE;
               }
          }

          HEAVYDEBUGMSG( "loaded %d\n", i );

          font->glyphs[i].start = start;

          HEAVYDEBUGMSG( "top: +++ %d +++\n", face->glyph->bitmap_top );
          HEAVYDEBUGMSG( "bitmap.rows: %d +++ metrics.height: %d +++\n",
                         face->glyph->bitmap.rows,
                         (int)(face->glyph->metrics.height >> 6) );


          d += start * (config->argb_font ? 4 : 1);
          //          d += pitch * (font->ascender - face->glyph->bitmap_top);

          for (y=0; y<face->glyph->bitmap.rows; y++) {
               if (config->argb_font) {
                    span_a8_to_argb(
                         &face->glyph->bitmap.buffer[face->glyph->bitmap.pitch*y],
                         (__u32*)d,
                         face->glyph->bitmap.width
                    );
               }
               else {
                    memcpy( d, &face->glyph->bitmap.buffer[face->glyph->bitmap.pitch*y],
                            face->glyph->bitmap.width );
               }
               d += pitch;
          }

          font->glyphs[i].left = face->glyph->bitmap_left;
          font->glyphs[i].top  = font->ascender - face->glyph->bitmap_top;

          start += font->glyphs[i].width;

          for (c=32; c<128; c++) {
               FT_Vector vector;

               FT_Get_Kerning( face, FT_Get_Char_Index(face, c), index,
                               ft_kerning_default, &vector );
#ifdef __i386__
           if (vector.x)
         {
           if (! font->kerning_table)
             font->kerning_table = malloc (256 * 256);

           font->kerning_table[i * 256 + c] = vector.x >> 6;
         }
#endif
          }
     }

     FT_Done_Face( face );

     surface_unlock( surface, 0 );
     font->surface = surface;

     return IDirectFBFont_Construct( thiz, font );
}

