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

#include <misc/conf.h>
#include <misc/util.h>


D_DEBUG_DOMAIN( Core_Font, "Core/Font", "DirectFB Core Font" );

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

     font->core = core;

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

     direct_hash_destroy( font->glyph_hash );

     if (font->surfaces) {
          for (i = 0; i < font->rows; i++)
               dfb_surface_unref( font->surfaces[i] );

          D_FREE( font->surfaces );
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

DFBResult
dfb_font_get_glyph_data( CoreFont        *font,
                         unsigned int     index,
                         CoreGlyphData  **ret_data )
{
     DFBResult      ret;
     CoreGlyphData *data;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( ret_data != NULL );

     data = direct_hash_lookup( font->glyph_hash, index );
     if (data) {
          *ret_data = data;
          return DFB_OK;
     }

     if (!font->GetGlyphInfo)
          return DFB_UNSUPPORTED;

     data = (CoreGlyphData *) D_CALLOC(1, sizeof (CoreGlyphData));
     if (!data)
          return D_OOM();

     ret = font->GetGlyphInfo (font, index, data);
     if (ret) {
          D_DERROR( ret, "Core/Font: Could not get glyph info for index %d!\n", index );
          goto error;
     }

     if (data->width > 0 && data->height > 0) {
          if (font->next_x + data->width > font->row_width) {
               CoreSurface *surface;

               if (font->row_width == 0) {
                    int width = 8192 / font->height;

                    if (width > 2048)
                         width = 2048;

                    if (width < font->maxadvance)
                         width = font->maxadvance;

                    font->row_width = (width + 7) & ~7;
               }

               ret = dfb_surface_create( font->core,
                                         font->row_width,
                                         MAX( font->height + 1, 8 ),
                                         font->pixel_format,
                                         CSP_VIDEOLOW, font->surface_caps, NULL, &surface );
               if (ret) {
                    D_ERROR( "DirectFB/core/fonts: "
                              "Could not create index surface! (%s)\n",
                              DirectFBErrorString( ret ) );

                    D_FREE( data );
                    return ret;
               }

               font->next_x = 0;
               font->rows++;

               font->surfaces = D_REALLOC( font->surfaces, sizeof(void *) * font->rows );

               D_ASSERT( font->surfaces != NULL );

               font->surfaces[font->rows - 1] = surface;
          }

          if (font->RenderGlyph(font, index, data, font->surfaces[font->rows - 1]) == DFB_OK) {
               int align = DFB_PIXELFORMAT_ALIGNMENT(font->pixel_format);

               data->surface = font->surfaces[font->rows - 1];
               data->start   = font->next_x;
               font->next_x += (data->width + align) & ~align;

               dfb_gfxcard_flush_texture_cache();
          }
          else {
               data->start = data->width = data->height = 0;
          }
     }
     else {
          data->start = data->width = data->height = 0;
     }


error:
     direct_hash_insert( font->glyph_hash, index, data );

     *ret_data = data;

     return DFB_OK;
}

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
     const __u8 *bytes = text;

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
                           __u32              character,
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

