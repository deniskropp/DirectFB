/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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
#include <string.h>

#include <malloc.h>

#include "directfb.h"

#include "core/coretypes.h"

#include "core/fonts.h"

#include "idirectfbfont.h"

#include "misc/utf8.h"
#include "misc/util.h"

/*
 * increments reference count of font
 */
DFBResult IDirectFBFont_AddRef( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

/*
 * decrements reference count, destructs interface data if reference count is 0
 */
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

/*
 * Get the distance from the baseline to the top.
 */
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

/*
 * Get the distance from the baseline to the bottom.
 * This is a negative value!
 */
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

/*
 * Get the height of this font.
 */
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

/*
 * Get the maximum character width.
 */
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

/*
 * Get the logical and ink extensions of the specified string as if it were 
 * drawn with this font.
 */
DFBResult IDirectFBFont_GetStringExtents( IDirectFBFont *thiz,
                                          const char *text, int bytes,
                                          DFBRectangle *logical_rect,
                                          DFBRectangle *ink_rect )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;
     CoreFont *font;
     CoreGlyphData *glyph;
     unichar  prev = 0;
     unichar  current;
     int      width = 0;
     int      offset;
     int      kerning;

     if (!data)
          return DFB_DEAD;

     if (!text)
          return DFB_INVARG;

     font = data->font;

     if (ink_rect) {
          memset (ink_rect, 0, sizeof (DFBRectangle));
     }

     if (bytes < 0)
          bytes = strlen (text);

     for (offset = 0; 
          offset < bytes; 
          offset += utf8_skip[(__u8)text[offset]]) {

          current = utf8_get_char (&text[offset]);
               
          if (fonts_get_glyph_data (font, current, &glyph) == DFB_OK) {
            
               if (prev && font->GetKerning && 
                   (* font->GetKerning) (font, prev, current, &kerning) == DFB_OK) {
                    width += kerning;
               }
               if (ink_rect) {
                    DFBRectangle glyph_rect = { width + glyph->left, glyph->top,
                                                glyph->width, glyph->height }; 
                    rectangle_union (ink_rect, &glyph_rect);
               } 

               width += glyph->advance;
          }

          prev = current;
     }

     if (logical_rect) {
          logical_rect->x = 0;
          logical_rect->y = - font->ascender;
          logical_rect->w = width;
          logical_rect->h = font->height;
     }

     if (ink_rect) {
          if (ink_rect->w < 0) {
               ink_rect->x += ink_rect->w;
               ink_rect->w = -ink_rect->w;
          }
          ink_rect->y -= font->ascender;
     }

     return DFB_OK;
}

/*
 * Get the logical width of the specified string as if it were drawn 
 * with this font. The drawn string may extend this value.
 */
DFBResult IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                                        const char *text, int bytes,
                                        int *width )
{
     DFBRectangle rect;
     DFBResult    result;

     if (!width)
          return DFB_INVARG;

     result = IDirectFBFont_GetStringExtents (thiz, text, bytes, &rect, NULL);

     if (result == DFB_OK)
          *width = rect.w;

     return result;
}

DFBResult IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFont *font )
{
     IDirectFBFont_data *data;

     data = (IDirectFBFont_data*) calloc( 1, sizeof(IDirectFBFont_data) );

     thiz->priv = data;

     data->ref = 1;
     data->font = font;

     thiz->AddRef = IDirectFBFont_AddRef;
     thiz->Release = IDirectFBFont_Release;
     thiz->GetAscender = IDirectFBFont_GetAscender;
     thiz->GetDescender = IDirectFBFont_GetDescender;
     thiz->GetHeight = IDirectFBFont_GetHeight;
     thiz->GetMaxAdvance = IDirectFBFont_GetMaxAdvance;
     thiz->GetStringWidth = IDirectFBFont_GetStringWidth;
     thiz->GetStringExtents = IDirectFBFont_GetStringExtents;

     return DFB_OK;
}

void IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     fonts_destruct (data->font);
     free (data->font);

     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}
