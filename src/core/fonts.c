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

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/tree.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

CoreFont *
dfb_font_create( CoreDFB *core )
{
     CoreFont *font;

     font = D_CALLOC( 1, sizeof(CoreFont) );
     if (!font) {
          D_OOM();
          return NULL;
     }

     font->core = core;

     direct_util_recursive_pthread_mutex_init( &font->lock );

     /* the proposed pixel_format, may be changed by the font provider */
     font->pixel_format = dfb_config->font_format ? : DSPF_A8;

     if (font->pixel_format == DSPF_ARGB)
          font->surface_caps = DSCAPS_PREMULTIPLIED;

     /* the state used to blit the glyphs, may be changed by the font provider */
     dfb_state_init( &font->state );
     font->state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;

     font->glyph_infos = direct_tree_new();

     D_MAGIC_SET( font, CoreFont );

     return font;
}

void
dfb_font_destroy( CoreFont *font )
{
     int i;

     D_MAGIC_ASSERT( font, CoreFont );

     D_MAGIC_CLEAR( font );

     pthread_mutex_lock( &font->lock );

     dfb_state_set_destination( &font->state, NULL );
     dfb_state_set_source( &font->state, NULL );

     dfb_state_destroy( &font->state );

     direct_tree_destroy( font->glyph_infos );

     if (font->surfaces) {
          for (i = 0; i < font->rows; i++)
               dfb_surface_unref( font->surfaces[i] );

          D_FREE( font->surfaces );
     }

     pthread_mutex_unlock( &font->lock );
     pthread_mutex_destroy( &font->lock );

     D_FREE( font );
}

void
dfb_font_drop_destination( CoreFont    *font,
                           CoreSurface *surface )
{
     D_MAGIC_ASSERT( font, CoreFont );

     D_ASSERT( surface != NULL );

     pthread_mutex_lock( &font->lock );

     if (font->state.destination == surface)
          dfb_state_set_destination( &font->state, NULL );

     pthread_mutex_unlock( &font->lock );
}

DFBResult
dfb_font_get_glyph_data( CoreFont        *font,
                         unichar          glyph,
                         CoreGlyphData  **ret_data )
{
     DFBResult      ret;
     CoreGlyphData *data;

     D_MAGIC_ASSERT( font, CoreFont );

     D_ASSERT( ret_data != NULL );

     if ((data = direct_tree_lookup (font->glyph_infos, (void *)glyph)) != NULL) {
          *ret_data = data;
          return DFB_OK;
     }

     data = (CoreGlyphData *) D_CALLOC(1, sizeof (CoreGlyphData));
     if (!data)
          return DFB_NOSYSTEMMEMORY;

     if (font->GetGlyphInfo &&
         font->GetGlyphInfo (font, glyph, data) == DFB_OK &&
         data->width > 0 && data->height > 0)
     {
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
                              "Could not create glyph surface! (%s)\n",
                              DirectFBErrorString( ret ) );

                    D_FREE( data );
                    return ret;
               }

               font->next_x = 0;
               font->rows++;

               font->surfaces = D_REALLOC( font->surfaces, sizeof(void *) * font->rows );

               font->surfaces[font->rows - 1] = surface;
          }

          if (font->RenderGlyph(font, glyph, data, font->surfaces[font->rows - 1]) == DFB_OK) {
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

     direct_tree_insert (font->glyph_infos, (void *) glyph, data);

     *ret_data = data;

     return DFB_OK;
}

