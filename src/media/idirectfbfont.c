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

#include <directfb.h>

#include <core/core.h>

#include "idirectfbfont.h"

/*
 * increments reference count of input buffer
 */
DFBResult IDirectFBFont_AddRef( IDirectFBFont *thiz );

/*
 * decrements reference count, destructs interface data if reference count is 0
 */
DFBResult IDirectFBFont_Release( IDirectFBFont *thiz );

/*
 * Get the distance from the baseline to the top.
 */
DFBResult IDirectFBFont_GetAscender( IDirectFBFont *thiz, int *ascender );

/*
 * Get the distance from the baseline to the bottom.
 * This is a negative value!
 */
DFBResult IDirectFBFont_GetDescender( IDirectFBFont *thiz, int *descender );

/*
 * Get the height of this font.
 */
DFBResult IDirectFBFont_GetHeight( IDirectFBFont *thiz, int *height );

/*
 * Get the maximum character width.
 */
DFBResult IDirectFBFont_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance );

/*
 * Get the pixel width of the specified string
 * as if it were drawn with this font.
 */
DFBResult IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                                        const char *string, int *width );


DFBResult IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFontData *font )
{
     IDirectFBFont_data *data;

     data = (IDirectFBFont_data*)malloc( sizeof(IDirectFBFont_data) );
     memset( data, 0, sizeof(IDirectFBFont_data) );
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

     return DFB_OK;
}

void IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (data->font) {
          surface_destroy (data->font->surface);
      if (data->font->kerning_table)
           free (data->font->kerning_table);
      free (data->font);
     }

     free( thiz->priv );
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
     unsigned char prev = 0;
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!width)
          return DFB_INVARG;

     if (!string)
          return DFB_INVARG;

     *width = 0;

     while (*string) {
          if (data->font->kerning_table && prev)
        *width += data->font->kerning_table[(unsigned char)*string * 256 + prev];
          *width += data->font->glyphs[(unsigned char)*string].advance;

          prev = *string++;
     }

     return DFB_OK;
}
