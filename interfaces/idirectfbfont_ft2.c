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
#include <core/fonts.h>

#include <gfx/convert.h>

#include <idirectfb.h>

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

DFBResult render_glyph( CoreFontData  *thiz,
                        unichar        glyph, 
                        CoreGlyphData *info,
                        CoreSurface   *surface )
{
     __u8 *dst;
     FT_Error err;
     FT_Face  face;
     int y;
     int index;
     int pitch;

     face = thiz->impl_data;

     index = FT_Get_Char_Index(face, glyph);

     err = FT_Load_Glyph( face, index, FTLOADFLAGS );
     if (err) {
          ERRORMSG( "DirectB/FontFT2: "
                    "Could not load glyph for character #%d!\n", glyph );
          return DFB_FAILURE;
     }

     if (face->glyph->format != ft_glyph_format_bitmap) {
          err = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
          if (err) {
               ERRORMSG( "DirectFB/FontFT2: Could not "
                         "render glyph for character #%d!\n", glyph );
               
               return DFB_FAILURE;
          }
     }

     HEAVYDEBUGMSG( "loaded %d\n", glyph );

     HEAVYDEBUGMSG( "top: +++ %d +++\n", face->glyph->bitmap_top );
     HEAVYDEBUGMSG( "bitmap.rows: %d +++ metrics.height: %d +++\n",
                    face->glyph->bitmap.rows,
                    (int)(face->glyph->metrics.height >> 6) );

     err = surface_soft_lock( surface, DSLF_WRITE, (void**)&dst, &pitch, 0 );
     if (err) {
          ERRORMSG( "DirectB/FontFT2: Unable to lock surface!\n" );
          return err;
     }

     info->width = face->glyph->bitmap.width;
     if (info->width + thiz->next_x > surface->width)
          info->width = surface->width - thiz->next_x;

     info->height = face->glyph->bitmap.rows;
     if (info->height > surface->height)
          info->height = surface->height;

     info->left = face->glyph->bitmap_left;
     info->top  = thiz->ascender - face->glyph->bitmap_top;

     dst += thiz->next_x * BYTES_PER_PIXEL(surface->format);

     for (y=0; y < info->height; y++) {
          switch (BYTES_PER_PIXEL(surface->format)) {
             case 4:
               span_a8_to_argb( &face->glyph->bitmap.buffer[face->glyph->bitmap.pitch*y],
                                (__u32*) dst, info->width );
             break;
             case 1:
               memcpy( dst, &face->glyph->bitmap.buffer[face->glyph->bitmap.pitch*y],
                       info->width );
             break;
             default:
               break;
          }
          dst += pitch;
     }

     surface_unlock( surface, 0 );

     return DFB_OK;
}

DFBResult get_glyph_info( CoreFontData  *thiz,
                          unichar        glyph, 
                          CoreGlyphData *info )
{
     FT_Error err;
     FT_Face  face;
     int index;

     face = thiz->impl_data;

     index = FT_Get_Char_Index(face, glyph);

     err = FT_Load_Glyph( face, index, FTLOADFLAGS );
     if (err) {
          ERRORMSG( "DirectB/FontFT2: "
                    "Could not load glyph for character #%d!\n", glyph );
          return DFB_FAILURE;
     }
     
     if (face->glyph->format != ft_glyph_format_bitmap) {
          err = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
          if (err) {
               ERRORMSG( "DirectFB/FontFT2: Could not "
                         "render glyph for character #%d!\n", glyph );
               
               return DFB_FAILURE;
          }
     }

     info->width   = face->glyph->bitmap.width;
     info->height  = face->glyph->bitmap.rows;
     info->advance = face->glyph->advance.x >> 6;

     return DFB_OK;
}

DFBResult get_kerning( CoreFontData *thiz, 
                       unichar       prev, 
                       unichar       current, 
                       int          *kerning )
{
     FT_Face   face;
     FT_Vector vector;
     int prev_index;
     int current_index;

     face = thiz->impl_data;

     prev_index    = FT_Get_Char_Index(face, prev);
     current_index = FT_Get_Char_Index(face, current);

     FT_Get_Kerning( face, prev_index, current_index, ft_kerning_default, &vector );
     *kerning = vector.x >> 6;

     return DFB_OK;
}

void IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (data->font->impl_data)
          FT_Done_Face (data->font->impl_data);

     fonts_destruct (data->font);
     free( data->font );
     free( data );

     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBFont_AddRef( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBFont_Release( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBFont_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBFont_GetAscender( IDirectFBFont *thiz, int *ascender )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!ascender)
          return DFB_INVARG;

     *ascender = data->font->ascender;

     return DFB_OK;
}

DFBResult IDirectFBFont_GetDescender( IDirectFBFont *thiz, int *descender )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!descender)
          return DFB_INVARG;

     *descender = data->font->descender;

     return DFB_OK;
}

DFBResult IDirectFBFont_GetHeight( IDirectFBFont *thiz, int *height )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!height)
          return DFB_INVARG;

     *height = data->font->height;

     return DFB_OK;
}

DFBResult IDirectFBFont_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!maxadvance)
          return DFB_INVARG;

     *maxadvance = data->font->maxadvance;

     return DFB_OK;
}

DFBResult IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                                        const char *string, int *width )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;
     CoreGlyphData *glyph;
     unichar  prev = 0;
     unichar  current;
     int      kerning;


     if (!data)
          return DFB_DEAD;

     if (!width)
          return DFB_INVARG;

     if (!string)
          return DFB_INVARG;

     *width = 0;

     while (*string) {
          current = utf8_get_char (string);
               
          if (fonts_get_glyph_data (data->font, current, &glyph) == DFB_OK) {
            
               if (prev && data->font->GetKerning && 
                   (* data->font->GetKerning) (data->font, prev, current, &kerning) == DFB_OK) {
                    *width += kerning;
               }
            *width += glyph->advance;
          }

          prev = current;
          string = utf8_next_char (string);
     }

     return DFB_OK;
}

DFBResult Construct( IDirectFBFont *thiz,
                     const char *filename,
                     DFBFontDescription *desc )
{
     IDirectFBFont_data *data;
     CoreFontData       *font;
     FT_Face  face;
     FT_Error err;

     HEAVYDEBUGMSG( "DirectFB/FontFT2: Construct font '%s' height %d\n", 
                    filename, 
                    (desc->flags & DFDESC_HEIGHT) ? desc->height : -1 );

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

     font->ascender   = face->size->metrics.ascender  >> 6;
     font->descender  = face->size->metrics.descender >> 6;
     font->height     = (face->size->metrics.ascender - 
                         face->size->metrics.descender) >> 6;
     font->maxadvance = face->size->metrics.max_advance >> 6;

     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->height = %d\n", font->height );
     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->ascender = %d\n", font->ascender);
     HEAVYDEBUGMSG( "DirectFB/FontFT2: font->descender = %d\n",font->descender);

     font->impl_data = face;

     font->GetGlyphInfo = get_glyph_info;
     font->RenderGlyph  = render_glyph;
     font->GetKerning   = get_kerning;

     data = (IDirectFBFont_data *) malloc( sizeof(IDirectFBFont_data) );

     data->ref = 1;
     data->font = font;

     thiz->priv = data;

     thiz->AddRef = IDirectFBFont_AddRef;
     thiz->Release = IDirectFBFont_Release;
     thiz->GetAscender = IDirectFBFont_GetAscender;
     thiz->GetDescender = IDirectFBFont_GetDescender;
     thiz->GetHeight = IDirectFBFont_GetHeight;
     thiz->GetMaxAdvance = IDirectFBFont_GetMaxAdvance;
     thiz->GetStringWidth = IDirectFBFont_GetStringWidth;

     return DFB_OK;
}


