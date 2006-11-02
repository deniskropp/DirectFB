/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/utf8.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/util.h>


D_DEBUG_DOMAIN( Core_Font,         "Core/Font",      "DirectFB Core Font" );
D_DEBUG_DOMAIN( Core_FontSurfaces, "Core/Font/Surf", "DirectFB Core Font Surfaces" );

/**********************************************************************************************************************/

static bool free_glyphs( DirectHash *hash,
                         u32         key,
                         void       *value,
                         void       *ctx );

/**********************************************************************************************************************/

DFBResult
dfb_font_create( CoreDFB *core, CoreFont **ret_font )
{
     DFBResult  ret;
     CoreFont  *font;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_ASSERT( ret_font != NULL );

     font = D_CALLOC( 1, sizeof(CoreFont) );
     if (!font)
          return D_OOM();

     ret = direct_hash_create( 163, &font->glyph_hash );
     if (ret) {
          D_FREE( font );
          return ret;
     }

     font->core     = core;
     font->max_rows = 5;

     direct_util_recursive_pthread_mutex_init( &font->lock );

     /* the proposed pixel_format, may be changed by the font provider */
     font->pixel_format = dfb_config->font_format ? : DSPF_A8;

     if (font->pixel_format == DSPF_ARGB)
          font->surface_caps = DSCAPS_PREMULTIPLIED;

     /* the state used to blit the glyphs, may be changed by the font provider */
     dfb_state_init( &font->state, core );
     font->state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;

     D_MAGIC_SET( font, CoreFont );

     *ret_font = font;

     return DFB_OK;
}

void
dfb_font_destroy( CoreFont *font )
{
     int i;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );

     D_MAGIC_CLEAR( font );

     pthread_mutex_lock( &font->lock );

     dfb_state_set_destination( &font->state, NULL );
     dfb_state_set_source( &font->state, NULL );

     dfb_state_destroy( &font->state );

     direct_hash_iterate( font->glyph_hash, free_glyphs, NULL );

     direct_hash_destroy( font->glyph_hash );

     if (font->rows) {
          for (i = 0; i < font->num_rows; i++) {
               CoreFontCacheRow *row = font->rows[i];

               D_MAGIC_ASSERT( row, CoreFontCacheRow );

               dfb_surface_unref( row->surface );

               D_MAGIC_CLEAR( row );

               D_FREE( row );
          }

          D_FREE( font->rows );
     }

     D_ASSERT( font->encodings != NULL || !font->last_encoding );

     for (i=DTEID_OTHER; i<=font->last_encoding; i++) {
          CoreFontEncoding *encoding = font->encodings[i];

          D_ASSERT( encoding != NULL );
          D_ASSERT( encoding->name != NULL );

          D_MAGIC_CLEAR( encoding );

          D_FREE( encoding->name );
          D_FREE( encoding );
     }

     if (font->encodings)
          D_FREE( font->encodings );

     pthread_mutex_unlock( &font->lock );
     pthread_mutex_destroy( &font->lock );

     D_FREE( font );
}

void
dfb_font_drop_destination( CoreFont    *font,
                           CoreSurface *surface )
{
     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );

     D_ASSERT( surface != NULL );

     pthread_mutex_lock( &font->lock );

     if (font->state.destination == surface)
          dfb_state_set_destination( &font->state, NULL );

     pthread_mutex_unlock( &font->lock );
}

/**********************************************************************************************************************/

DFBResult
dfb_font_get_glyph_data( CoreFont        *font,
                         unsigned int     index,
                         CoreGlyphData  **ret_data )
{
     DFBResult         ret;
     CoreGlyphData    *data;
     int               i;
     int               align;
     CoreFontCacheRow *row = NULL;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( ret_data != NULL );

     D_ASSERT( font->num_rows >= 0 );

     if (font->num_rows) {
          D_ASSERT( font->num_rows <= font->max_rows || font->max_rows < 0 );
          D_ASSERT( font->active_row >= 0 );
          D_ASSERT( font->active_row < font->num_rows );
     }

     if (index < 128 && font->glyph_data[index]) {
          *ret_data = font->glyph_data[index];
          return DFB_OK;
     }

     data = direct_hash_lookup( font->glyph_hash, index );
     if (data) {
          D_MAGIC_ASSERT( data, CoreGlyphData );
          D_ASSERT( data->row >= 0 );
          D_ASSERT( data->row < font->num_rows );

          row = font->rows[data->row];

          D_MAGIC_ASSERT( row, CoreFontCacheRow );

          row->stamp = font->row_stamp++;

          *ret_data = data;
          return DFB_OK;
     }

     if (!font->GetGlyphData)
          return DFB_UNSUPPORTED;

     data = D_CALLOC( 1, sizeof(CoreGlyphData) );
     if (!data)
          return D_OOM();

     D_MAGIC_SET( data, CoreGlyphData );

     align = DFB_PIXELFORMAT_ALIGNMENT( font->pixel_format );

     ret = font->GetGlyphData( font, index, data );
     if (ret) {
          D_DERROR( ret, "Core/Font: Could not get glyph info for index %d!\n", index );
          data->start = data->width = data->height = 0;
          goto out;
     }

     if (data->width < 1 || data->height < 1) {
          data->start = data->width = data->height = 0;
          goto out;
     }

     if (font->rows) {
          D_ASSERT( font->active_row >= 0 );
          D_ASSERT( font->active_row < font->num_rows );

          row = font->rows[font->active_row];

          D_MAGIC_ASSERT( row, CoreFontCacheRow );
     }
     else {
          /* Calculate row width? */
          if (font->row_width == 0) {
               int width = 2048 * font->height / 64;

               if (width > 2048)
                    width = 2048;

               if (width < font->maxadvance)
                    width = font->maxadvance;

               font->row_width = (width + 7) & ~7;
          }
     }

     /* Need another font surface? */
     if (!row || (row->next_x + data->width > font->row_width)) {
          D_ASSERT( font->max_rows != 0 );

          /* Maximum number of rows reached? */
          if (font->num_rows == font->max_rows) {
               int          best_row = -1;
               unsigned int best_val = 0;

               /* Check for trailing space first. */
               for (i=0; i<font->num_rows; i++) {
                    row = font->rows[i];

                    D_MAGIC_ASSERT( row, CoreFontCacheRow );

                    if (row->next_x + data->width <= font->row_width) {
                         if (best_row == -1 || best_val < row->next_x) {
                              best_row = i;
                              best_val = row->next_x;
                         }
                    }
               }

               /* Found a row with enough trailing space? */
               if (best_row != -1) {
                    font->active_row = best_row;
                    row = font->rows[best_row];

                    D_MAGIC_ASSERT( row, CoreFontCacheRow );

                    D_DEBUG_AT( Core_FontSurfaces, "  -> using trailing space of row %d - %dx%d %s\n",
                                font->active_row, row->surface->width, row->surface->height,
                                dfb_pixelformat_name(row->surface->format) );
               }
               else {
                    CoreGlyphData *d, *n;

                    D_ASSERT( best_row == -1 );
                    D_ASSERT( best_val == 0 );

                    /* Reuse the least recently used row. */
                    for (i=0; i<font->num_rows; i++) {
                         row = font->rows[i];

                         D_MAGIC_ASSERT( row, CoreFontCacheRow );

                         if (best_row == -1 || best_val > row->stamp) {
                              best_row = i;
                              best_val = row->stamp;
                         }
                    }

                    D_ASSERT( best_row != -1 );

                    font->active_row = best_row;
                    row = font->rows[best_row];

                    D_MAGIC_ASSERT( row, CoreFontCacheRow );

                    D_DEBUG_AT( Core_FontSurfaces, "  -> reusing row %d - %dx%d %s\n",
                                font->active_row, row->surface->width, row->surface->height,
                                dfb_pixelformat_name(row->surface->format) );

                    /* Kick out all glyphs. */
                    direct_list_foreach_safe (d, n, row->glyphs) {
                         D_MAGIC_ASSERT( d, CoreGlyphData );

                         /*ret =*/ direct_hash_remove( font->glyph_hash, d->index );
                         //FIXME: use D_ASSERT( ret == DFB_OK );

                         if (d->index < 128)
                              font->glyph_data[d->index] = NULL;

                         D_MAGIC_CLEAR( d );
                         D_FREE( d );
                    }

                    /* Reset row. */
                    row->glyphs = NULL;
                    row->next_x = 0;
               }
          }
          else {
               /* Allocate new font cache row structure. */
               row = D_CALLOC( 1, sizeof(CoreFontCacheRow) );
               if (!row) {
                    ret = D_OOM();
                    goto error;
               }

               /* Create a new font surface. */
               ret = dfb_surface_create( font->core,
                                         font->row_width,
                                         MAX( font->height + 1, 8 ),
                                         font->pixel_format,
                                         CSP_VIDEOLOW, font->surface_caps, NULL, &row->surface );
               if (ret) {
                    D_DERROR( ret, "Core/Font: Could not create font surface!\n" );
                    D_FREE( row );
                    goto error;
               }

               D_DEBUG_AT( Core_FontSurfaces, "  -> new row %d - %dx%d %s\n", font->num_rows,
                           row->surface->width, row->surface->height,
                           dfb_pixelformat_name(row->surface->format) );

               D_MAGIC_SET( row, CoreFontCacheRow );


               /* Append to array. FIXME: Use vector to avoid realloc each time! */
               font->rows = D_REALLOC( font->rows, sizeof(void*) * (font->num_rows + 1) );
               D_ASSERT( font->rows != NULL );

               font->rows[font->num_rows] = row;

               /* Set new row to use. */
               font->active_row = font->num_rows++;
          }
     }

     D_MAGIC_ASSERT( row, CoreFontCacheRow );
     D_ASSERT( font->num_rows > 0 );
     D_ASSERT( font->num_rows <= font->max_rows || font->max_rows < 0 );
     D_ASSERT( font->active_row >= 0 );
     D_ASSERT( font->active_row < font->num_rows );
     D_ASSERT( row == font->rows[font->active_row] );

     D_DEBUG_AT( Core_FontSurfaces, "  -> render %2d - %2dx%2d at %d:%03d font <%p>\n",
                 index, data->width, data->height, font->active_row, row->next_x, font );

     data->index   = index;
     data->row     = font->active_row;
     data->start   = row->next_x;
     data->surface = row->surface;

     row->next_x  += (data->width + align) & ~align;

     row->stamp = font->row_stamp++;

     /* Render the glyph data into the surface. */
     ret = font->RenderGlyph( font, index, data );
     if (ret) {
          data->start = data->width = data->height = 0;
          goto out;
     }

     dfb_gfxcard_flush_texture_cache();


out:
     if (row)
          direct_list_append( &row->glyphs, &data->link );

     direct_hash_insert( font->glyph_hash, index, data );

     if (index < 128)
          font->glyph_data[index] = data;

     *ret_data = data;

     return DFB_OK;


error:
     D_MAGIC_CLEAR( data );
     D_FREE( data );

     return ret;
}

/**********************************************************************************************************************/

DFBResult
dfb_font_register_encoding( CoreFont                    *font,
                            const char                  *name,
                            const CoreFontEncodingFuncs *funcs,
                            DFBTextEncodingID            encoding_id )
{
     CoreFontEncoding  *encoding;
     CoreFontEncoding **encodings;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( encoding_id == DTEID_UTF8 || name != NULL );
     D_ASSERT( funcs != NULL );

     if (!funcs->GetCharacterIndex)
          return DFB_INVARG;

     /* Special case for default, native format. */
     if (encoding_id == DTEID_UTF8) {
          font->utf8 = funcs;

          return DFB_OK;
     }

     if (!funcs->DecodeText)
          return DFB_INVARG;

     /* Setup new encoding information. */
     encoding = D_CALLOC( 1, sizeof(CoreFontEncoding) );
     if (!encoding)
          return D_OOM();

     encoding->encoding = font->last_encoding + 1;
     encoding->funcs    = funcs;
     encoding->name     = D_STRDUP( name );

     if (!encoding->name) {
          D_FREE( encoding );
          return D_OOM();
     }

     /* Add to array. */
     encodings = D_REALLOC( font->encodings,
                            (encoding->encoding + 1) * sizeof(CoreFontEncoding*) );
     if (!encodings) {
          D_FREE( encoding->name );
          D_FREE( encoding );
          return D_OOM();
     }

     font->encodings = encodings;

     font->last_encoding++;

     D_ASSERT( font->last_encoding == encoding->encoding );

     encodings[encoding->encoding] = encoding;

     D_MAGIC_SET( encoding, CoreFontEncoding );

     return DFB_OK;
}

DFBResult
dfb_font_decode_text( CoreFont          *font,
                      DFBTextEncodingID  encoding,
                      const void        *text,
                      int                length,
                      unsigned int      *ret_indices,
                      int               *ret_num )
{
     int pos = 0, num = 0;
     const u8 *bytes = text;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( text != NULL );
     D_ASSERT( length >= 0 );  /* TODO: handle -1 here? */
     D_ASSERT( ret_indices != NULL );
     D_ASSERT( ret_num != NULL );

     if (encoding != DTEID_UTF8) {
          if (encoding > font->last_encoding)
               return DFB_IDNOTFOUND;

          D_ASSERT( font->encodings[encoding] != NULL );
          D_ASSERT( font->encodings[encoding]->funcs != NULL );
          D_ASSERT( font->encodings[encoding]->funcs->DecodeText != NULL );

          return font->encodings[encoding]->funcs->DecodeText( font, text, length,
                                                               ret_indices, ret_num );
     }
     else if (font->utf8) {
          const CoreFontEncodingFuncs *funcs = font->utf8;

          if (funcs->DecodeText)
               return funcs->DecodeText( font, text, length, ret_indices, ret_num );

          D_ASSERT( funcs->GetCharacterIndex != NULL );

          while (pos < length) {
               unsigned int c;

               if (bytes[pos] < 128)
                    c = bytes[pos++];
               else {
                    c = DIRECT_UTF8_GET_CHAR( &bytes[pos] );
                    pos += DIRECT_UTF8_SKIP(bytes[pos]);
               }

               if (funcs->GetCharacterIndex( font, c, &ret_indices[num] ) == DFB_OK)
                    num++;
          }

     }
     else {
          while (pos < length) {
               if (bytes[pos] < 128)
                    ret_indices[num++] = bytes[pos++];
               else {
                    ret_indices[num++] = DIRECT_UTF8_GET_CHAR( &bytes[pos] );
                    pos += DIRECT_UTF8_SKIP(bytes[pos]);
               }
          }
     }

     *ret_num = num;

     return DFB_OK;
}

DFBResult
dfb_font_decode_character( CoreFont          *font,
                           DFBTextEncodingID  encoding,
                           u32                character,
                           unsigned int      *ret_index )
{
     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( ret_index != NULL );

     if (encoding > font->last_encoding)
          return DFB_IDNOTFOUND;

     if (encoding != DTEID_UTF8) {
          D_ASSERT( font->encodings[encoding] != NULL );
          D_ASSERT( font->encodings[encoding]->funcs != NULL );
          D_ASSERT( font->encodings[encoding]->funcs->GetCharacterIndex != NULL );

          return font->encodings[encoding]->funcs->GetCharacterIndex( font, character, ret_index );
     }
     else if (font->utf8) {
          const CoreFontEncodingFuncs *funcs = font->utf8;

          D_ASSERT( funcs->GetCharacterIndex != NULL );

          return funcs->GetCharacterIndex( font, character, ret_index );
     }
     else
          *ret_index = character;

     return DFB_OK;
}

/**********************************************************************************************************************/

static bool
free_glyphs( DirectHash *hash,
             u32         key,
             void       *value,
             void       *ctx )
{
     CoreGlyphData *data = value;

     D_MAGIC_ASSERT( data, CoreGlyphData );

     D_MAGIC_CLEAR( data );
     D_FREE( data );

     return true;
}

