/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
#include "directfb_internals.h"

#include "core/coretypes.h"

#include "core/fonts.h"

#include "idirectfbfont.h"

#include "misc/utf8.h"
#include "misc/mem.h"
#include "misc/util.h"

void
IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     dfb_font_destroy (data->font);

     DFB_DEALLOCATE_INTERFACE( thiz );
}

/*
 * increments reference count of font
 */
static DFBResult
IDirectFBFont_AddRef( IDirectFBFont *thiz )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     data->ref++;

     return DFB_OK;
}

/*
 * decrements reference count, destructs interface data if reference count is 0
 */
static DFBResult
IDirectFBFont_Release( IDirectFBFont *thiz )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     if (--data->ref == 0)
          IDirectFBFont_Destruct( thiz );

     return DFB_OK;
}

/*
 * Get the distance from the baseline to the top.
 */
static DFBResult
IDirectFBFont_GetAscender( IDirectFBFont *thiz, int *ascender )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     if (!ascender)
          return DFB_INVARG;

     *ascender = data->font->ascender;

     return DFB_OK;
}

/*
 * Get the distance from the baseline to the bottom.
 * This is a negative value!
 */
static DFBResult
IDirectFBFont_GetDescender( IDirectFBFont *thiz, int *descender )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     if (!descender)
          return DFB_INVARG;

     *descender = data->font->descender;

     return DFB_OK;
}

/*
 * Get the height of this font.
 */
static DFBResult
IDirectFBFont_GetHeight( IDirectFBFont *thiz, int *height )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     if (!height)
          return DFB_INVARG;

     *height = data->font->height;

     return DFB_OK;
}

/*
 * Get the maximum character width.
 */
static DFBResult
IDirectFBFont_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance )
{
     INTERFACE_GET_DATA(IDirectFBFont)

     if (!maxadvance)
          return DFB_INVARG;

     *maxadvance = data->font->maxadvance;

     return DFB_OK;
}

/*
 * Get the kerning to apply between two glyphs.
 */
static DFBResult
IDirectFBFont_GetKerning( IDirectFBFont *thiz,
                          unsigned int prev_index, unsigned int current_index,
                          int *kern_x, int *kern_y)
{
     CoreFont *font;
     int x, y;

     INTERFACE_GET_DATA(IDirectFBFont)

     if (!kern_x && !kern_y)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     if (! font->GetKerning ||
         font->GetKerning (font, prev_index, current_index, &x, &y) != DFB_OK)
          x = y = 0;

     if (kern_x)
          *kern_x = x;
     if (kern_y)
          *kern_y = y;

     dfb_font_unlock( font );

     return DFB_OK;
}

/*
 * Get the logical and ink extents of the specified string.
 */
static DFBResult
IDirectFBFont_GetStringExtents( IDirectFBFont *thiz,
                                const char *text, int bytes,
                                DFBRectangle *logical_rect,
                                DFBRectangle *ink_rect )
{
     CoreFont      *font;
     CoreGlyphData *glyph;
     unichar        prev = 0;
     unichar        current;
     int            width = 0;
     int            offset;
     int            kern_x;
     int            kern_y;

     INTERFACE_GET_DATA(IDirectFBFont)


     if (!text)
          return DFB_INVARG;

     if (!logical_rect && !ink_rect)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     if (ink_rect)
          memset (ink_rect, 0, sizeof (DFBRectangle));

     if (bytes < 0)
          bytes = strlen (text);

     for (offset = 0;
          offset < bytes;
          offset += dfb_utf8_skip[(__u8)text[offset]]) {

          current = dfb_utf8_get_char (&text[offset]);

          if (dfb_font_get_glyph_data (font, current, &glyph) == DFB_OK) {

               kern_y = 0;
               if (prev && font->GetKerning &&
                   (* font->GetKerning) (font, 
                                         prev, current, 
                                         &kern_x, &kern_y) == DFB_OK) {
                    width += kern_x;
               }
               if (ink_rect) {
                    DFBRectangle glyph_rect = { width + glyph->left, 
                                                kern_y + glyph->top,
                                                glyph->width, glyph->height };
                    dfb_rectangle_union (ink_rect, &glyph_rect);
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

     dfb_font_unlock( font );

     return DFB_OK;
}

/*
 * Get the logical width of the specified string.
 */
static DFBResult
IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                              const char *text, int bytes,
                              int *width )
{
     DFBRectangle rect;
     DFBResult    result;

     if (!width)
          return DFB_INVARG;

     result = thiz->GetStringExtents (thiz, text, bytes, &rect, NULL);

     if (result == DFB_OK)
          *width = rect.w;

     return result;
}

/*
 * Get the extents of the specified glyph.
 */
static DFBResult
IDirectFBFont_GetGlyphExtents( IDirectFBFont *thiz,
                               unsigned int   index,
                               DFBRectangle  *rect,
                               int           *advance )
{
     CoreFont      *font;
     CoreGlyphData *glyph;

     INTERFACE_GET_DATA(IDirectFBFont)

     if (!rect && !advance)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     if (dfb_font_get_glyph_data (font, index, &glyph) != DFB_OK) {

          if (rect) {
               rect->x = rect->y = rect->w = rect->h = 0;
          }
          if (advance) {
               *advance = 0;
          }
     }
     else {
          if (rect) {
               rect->x = glyph->left;
               rect->y = glyph->top - font->ascender;
               rect->w = glyph->width;
               rect->h = glyph->height;
          }
          if (advance) {
               *advance = glyph->advance;
          }
     }

     dfb_font_unlock( font );
     
     return DFB_OK;
}

DFBResult
IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFont *font )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFont)

     data->ref = 1;
     data->font = font;

     thiz->AddRef = IDirectFBFont_AddRef;
     thiz->Release = IDirectFBFont_Release;
     thiz->GetAscender = IDirectFBFont_GetAscender;
     thiz->GetDescender = IDirectFBFont_GetDescender;
     thiz->GetHeight = IDirectFBFont_GetHeight;
     thiz->GetMaxAdvance = IDirectFBFont_GetMaxAdvance;
     thiz->GetKerning = IDirectFBFont_GetKerning;
     thiz->GetStringWidth = IDirectFBFont_GetStringWidth;
     thiz->GetStringExtents = IDirectFBFont_GetStringExtents;
     thiz->GetGlyphExtents = IDirectFBFont_GetGlyphExtents;

     return DFB_OK;
}

