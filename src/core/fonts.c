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

#include <directfb.h>

#include "fonts.h"
#include "coredefs.h"


void fonts_destruct (CoreFontData *font)
{
     int i;

     if (!font)
          return;

     tree_destroy (font->glyph_infos);
     font->glyph_infos = NULL;

     for (i = 0; i < font->rows; i++) {  
          surface_destroy (font->surfaces[i]);
     }
     free (font->surfaces);
     font->surfaces = NULL;
     font->rows = 0;
}

DFBResult fonts_get_glyph_data (CoreFontData    *font,
                                unichar          glyph,
                                CoreGlyphData  **glyph_data)
{
     CoreGlyphData *data;
    
     if (!font->glyph_infos)
       font->glyph_infos = tree_new();

     data = tree_lookup (font->glyph_infos, (void *)glyph);

     if (!data) {
       data = malloc (sizeof (CoreGlyphData));
       if (!data)
         return DFB_NOSYSTEMMEMORY;
       
       memset (data, 0, sizeof (CoreGlyphData));

       if (font->GetGlyphInfo && 
            (* font->GetGlyphInfo) (font, glyph, data) == DFB_OK &&
            data->width && data->height) {
         
            if (font->next_x + data->width > font->row_width)
                 {
                     if (font->row_width == 0)
                         font->row_width = ((font->maxadvance * 32) > 2048 ? 
                                            2048 : font->maxadvance * 32);

                     font->next_x = 0;
                     font->rows++;

                     font->surfaces = 
                          realloc (font->surfaces, sizeof (void *) * font->rows);

                     surface_create( font->row_width, font->height, 
                                     config->argb_font ? DSPF_ARGB : DSPF_A8, 
                                     CSP_VIDEOHIGH, DSCAPS_NONE, 
                                     &font->surfaces[font->rows - 1] );
                 }

            if ((* font->RenderGlyph) 
                (font, glyph, data, font->surfaces[font->rows - 1]) == DFB_OK){
           
                 data->start = font->next_x + font->row_width * (font->rows - 1);
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
