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

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "directfb.h"

#include "coredefs.h"
#include "coretypes.h"

#include "fonts.h"
#include "surfaces.h"

#include "misc/mem.h"
#include "misc/tree.h"
#include "misc/util.h"


CoreFont *font_create()
{
     CoreFont *font;

     font = (CoreFont *) DFBCALLOC( 1, sizeof(CoreFont) );

     pthread_mutex_init( &font->lock, NULL );

     font->glyph_infos = tree_new ();

     return font;
}

void font_destroy( CoreFont *font )
{
     int i;

     font_lock( font );

     tree_destroy (font->glyph_infos);

     for (i = 0; i < font->rows; i++) {
          surface_destroy (font->surfaces[i]);
     }

     if (font->surfaces)
          DFBFREE( font->surfaces );

     font_unlock( font );

     pthread_mutex_destroy( &font->lock );

     DFBFREE( font );
}

DFBResult font_get_glyph_data( CoreFont        *font,
                               unichar          glyph,
                               CoreGlyphData  **glyph_data )
{
     CoreGlyphData *data;

     if ((data = tree_lookup (font->glyph_infos, (void *)glyph)) != NULL) {
          *glyph_data = data;
          return DFB_OK;
     }

     if (!data) {
          data = (CoreGlyphData *) DFBCALLOC(1, sizeof (CoreGlyphData));
          if (!data) {
               return DFB_NOSYSTEMMEMORY;
          }

          if (font->GetGlyphInfo &&
              (* font->GetGlyphInfo) (font, glyph, data) == DFB_OK &&
              data->width && data->height)
          {

               if (font->next_x + data->width > font->row_width) {
                    if (font->row_width == 0)
                        font->row_width = ((font->maxadvance * 32) > 2048 ?
                                           2048 : font->maxadvance * 32);

                    font->next_x = 0;
                    font->rows++;

                    font->surfaces = DFBREALLOC(font->surfaces,
                                              sizeof (void *) * font->rows);

                    surface_create( font->row_width, font->height,
                                    dfb_config->argb_font ? DSPF_ARGB : DSPF_A8,
                                    CSP_VIDEOHIGH, DSCAPS_NONE,
                                    &font->surfaces[font->rows - 1] );
               }

               if ((* font->RenderGlyph)
                   (font, glyph, data, font->surfaces[font->rows - 1]) == DFB_OK)
               {
                    data->surface = font->surfaces[font->rows - 1];
                    data->start   = font->next_x;
                    font->next_x += data->width;
               }
               else {
                    data->start = data->width = data->height = 0;
               }
          }

          tree_insert (font->glyph_infos, (void *) glyph, data);
     }

     *glyph_data = data;

     return DFB_OK;
}
