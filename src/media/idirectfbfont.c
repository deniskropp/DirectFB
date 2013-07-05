/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <math.h>

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/tree.h>
#include <direct/utf8.h>

#include <core/fonts.h>

#include <media/idirectfbfont.h>
#include <media/idirectfbdatabuffer.h>

#include "misc/util.h"


D_DEBUG_DOMAIN( Font, "IDirectFBFont", "DirectFB Font Interface" );

/**********************************************************************************************************************/

void
IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = (IDirectFBFont_data*)thiz->priv;

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_font_destroy (data->font);

     /* release memory, if any */
     if (data->content) {
          switch (data->content_type) {
               case IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED:
                    D_FREE( data->content );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED:
                    direct_file_unmap( NULL, data->content, data->content_size );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY:
                    break;

               default:
                    D_BUG( "unexpected content type %d", data->content_type );
          }
     }

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**********************************************************************************************************************/

/*
 * increments reference count of font
 */
static DirectResult
IDirectFBFont_AddRef( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

/*
 * decrements reference count, destructs interface data if reference count is 0
 */
static DirectResult
IDirectFBFont_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

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
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

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
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

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
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!height)
          return DFB_INVARG;

     *height = data->font->height;

     return DFB_OK;
}

/*
 * Get the line spacing vector of this font.
 */
static DFBResult
IDirectFBFont_GetLineSpacingVector( IDirectFBFont *thiz, int *xspacing, int *yspacing )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!xspacing && !yspacing)
          return DFB_INVARG;

     if (xspacing) {
          *xspacing = - data->font->height * data->font->up_unit_x;
     }

     if (yspacing) {
          *yspacing = - data->font->height * data->font->up_unit_y;
     }

     return DFB_OK;
}

/*
 * Get the maximum character width.
 */
static DFBResult
IDirectFBFont_GetMaxAdvance( IDirectFBFont *thiz, int *maxadvance )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

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
                          unsigned int prev, unsigned int current,
                          int *kern_x, int *kern_y)
{
     DFBResult     ret;
     CoreFont     *font;
     int           x = 0, y = 0;
     unsigned int  prev_index, current_index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!kern_x && !kern_y)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     if (font->GetKerning) {
          ret = dfb_font_decode_character( font, data->encoding, prev, &prev_index );
          if (ret)
               goto error;

          ret = dfb_font_decode_character( font, data->encoding, current, &current_index );
          if (ret)
               goto error;

          ret = font->GetKerning (font, prev_index, current_index, &x, &y);
          if (ret)
               goto error;
     }

     dfb_font_unlock( font );

     if (kern_x)
          *kern_x = x;
     if (kern_y)
          *kern_y = y;

     return DFB_OK;


error:
     dfb_font_unlock( font );

     return ret;
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
     DFBResult  ret;
     CoreFont  *font;
     int        xbaseline = 0;
     int        ybaseline = 0;


     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );


     if (!text)
          return DFB_INVARG;

     if (!logical_rect && !ink_rect)
          return DFB_INVARG;

     if (bytes < 0)
          bytes = strlen (text);

     if (ink_rect)
          memset (ink_rect, 0, sizeof (DFBRectangle));

     font = data->font;

     dfb_font_lock( font );

     if (bytes > 0) {
          int          i, num;
          unsigned int prev  = 0;
          unsigned int indices[bytes];

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( font );
               return ret;
          }

          for (i=0; i<num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( font, current, 0, &glyph ) == DFB_OK) {  // FIXME: support font layers
                    int kx, ky = 0;

                    if (prev && font->GetKerning &&
                        font->GetKerning( font, prev, current, &kx, &ky ) == DFB_OK) {
                         xbaseline += kx << 8;
                         ybaseline += ky << 8;
                    }

                    if (ink_rect) {
                         DFBRectangle glyph_rect = { xbaseline + (glyph->left << 8),
                              ybaseline + (glyph->top << 8),
                              glyph->width << 8, glyph->height << 8};
                         dfb_rectangle_union (ink_rect, &glyph_rect);
                    }

                    xbaseline += glyph->xadvance;
                    ybaseline += glyph->yadvance;
               }

               prev = current;
          }
     }

     if (logical_rect) {
          // We already have the text baseline vector in (xbaseline,ybaseline).
          // Now find the ascender and descender vectors:
          int xascender =  font->ascender * font->up_unit_x;
          int yascender =  font->ascender * font->up_unit_y;
          int xdescender = font->descender * font->up_unit_x;
          int ydescender = font->descender * font->up_unit_y;

          // Now find top/bottom left/right points relative to the text:
          int top_left_x     = xascender;
          int top_left_y     = yascender;
          int bottom_left_x  = xdescender;
          int bottom_left_y  = ydescender;
          int top_right_x    = top_left_x + (xbaseline >> 8);
          int top_right_y    = top_left_y + (ybaseline >> 8);
          int bottom_right_x = bottom_left_x + (xbaseline >> 8);
          int bottom_right_y = bottom_left_y + (ybaseline >> 8);

          // The logical rectangle is the bounding-box of these points:
#define MIN4(a,b,c,d) (MIN(MIN((a),(b)),MIN((c),(d))))
#define MAX4(a,b,c,d) (MAX(MAX((a),(b)),MAX((c),(d))))
          logical_rect->x = MIN4(top_left_x, bottom_left_x, top_right_x, bottom_right_x);
          logical_rect->y = MIN4(top_left_y, bottom_left_y, top_right_y, bottom_right_y);
          logical_rect->w = MAX4(top_left_x, bottom_left_x, top_right_x, bottom_right_x) - logical_rect->x;
          logical_rect->h = MAX4(top_left_y, bottom_left_y, top_right_y, bottom_right_y) - logical_rect->y;
     }

     if (ink_rect) {
          if (ink_rect->w < 0) {  /* PBE FIXME what is this doing? */
               ink_rect->x += ink_rect->w;
               ink_rect->w = -ink_rect->w;
          }
          ink_rect->x += (font->ascender * font->up_unit_x) / 256.0f;
          ink_rect->y += (font->ascender * font->up_unit_y) / 256.0f;

          ink_rect->x >>= 8;
          ink_rect->y >>= 8;
          ink_rect->w >>= 8;
          ink_rect->h >>= 8;
     }

     dfb_font_unlock( font );

     return DFB_OK;
}

/*
 * Get the logical width of the specified string.
 */
static DFBResult
IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                              const char    *text,
                              int            bytes,
                              int           *ret_width )
{
     DFBResult ret;
     int       xsize = 0;
     int       ysize = 0;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!text || !ret_width)
          return DFB_INVARG;

     if (bytes < 0)
          bytes = strlen (text);

     if (bytes > 0) {
          int           i, num, kx, ky;
          unsigned int  prev = 0;
          unsigned int  indices[bytes];
          CoreFont     *font = data->font;

          dfb_font_lock( font );

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( font );
               return ret;
          }

          /* Calculate string width. */
          for (i=0; i<num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( font, current, 0, &glyph ) == DFB_OK) {  // FIXME: support font layers
                    xsize += glyph->xadvance;
                    ysize += glyph->yadvance;

                    if (prev && font->GetKerning &&
                        font->GetKerning( font, prev, current, &kx, &ky ) == DFB_OK) {
                         xsize += kx << 8;
                         ysize += ky << 8;
                    }
               }

               prev = current;
          }

          dfb_font_unlock( font );
     }

     if (!ysize) {
          *ret_width = xsize >> 8;
     }
     else if (!xsize) {
          *ret_width = ysize >> 8;
     }
     else {
          *ret_width = sqrt(xsize*(xsize >> 8) + ysize*(ysize >> 8)) / 256.0f;
     }

     return DFB_OK;
}

/*
 * Get the extents of the specified glyph.
 */
static DFBResult
IDirectFBFont_GetGlyphExtents( IDirectFBFont *thiz,
                               unsigned int   character,
                               DFBRectangle  *rect,
                               int           *advance )
{
     DFBResult      ret;
     CoreFont      *font;
     CoreGlyphData *glyph;
     unsigned int   index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!rect && !advance)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     ret = dfb_font_decode_character( font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( font );
          return ret;
     }

     if (dfb_font_get_glyph_data (font, index, 0, &glyph) != DFB_OK) {     // FIXME: support font layers
          if (rect)
               rect->x = rect->y = rect->w = rect->h = 0;

          if (advance)
               *advance = 0;
     }
     else {
          if (rect) {
               rect->x = glyph->left;
               rect->y = glyph->top - font->ascender;
               rect->w = glyph->width;
               rect->h = glyph->height;
          }

          if (advance)
               *advance = glyph->xadvance >> 8;
     }

     dfb_font_unlock( font );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetStringBreak( IDirectFBFont *thiz,
                              const char    *text,
                              int            bytes,
                              int            max_width,
                              int           *ret_width,
                              int           *ret_str_length,
                              const char   **ret_next_line)
{
     DFBResult      ret;
     CoreFont      *font;
     const u8      *string;
     const u8      *last;
     const u8      *end;
     CoreGlyphData *glyph;
     int            kern_x;
     int            kern_y;
     int            length = 0;
     int            xsize = 0;
     int            ysize = 0;
     int            width = 0;
     unichar        current;
     unsigned int   index;
     unsigned int   prev  = 0;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     if (!text || !ret_next_line || !ret_str_length || !ret_width)
          return DFB_INVARG;

     /* FIXME: Try to change the font module API *slightly* to support this. */
     if (data->encoding != DTEID_UTF8)
          return DFB_UNSUPPORTED;

     if (bytes < 0)
          bytes = strlen (text);

     if (!bytes) {
          *ret_next_line = NULL;
          *ret_str_length = 0;
          *ret_width = 0;

          return DFB_OK;
     }

     font   = data->font;
     string = (const u8*) text;
     end    = string + bytes;
     *ret_next_line = NULL;

     dfb_font_lock( font );

     do {
          *ret_width = width >> 8;

          current = DIRECT_UTF8_GET_CHAR( string );

          last    = string;
          string += DIRECT_UTF8_SKIP( string[0] );

          if (current == ' ' || current == 0x0a) {
               *ret_next_line = (const char*) string;
               *ret_str_length = length;
               *ret_width = width >> 8;
          }

          length++;

          ret = dfb_font_decode_character( font, data->encoding, current, &index );
          if (ret)
               continue;

          ret = dfb_font_get_glyph_data( font, index, 0, &glyph );    // FIXME: support font layers
          if (ret)
               continue;

          xsize += glyph->xadvance;
          ysize += glyph->yadvance;

          if (prev && font->GetKerning && font->GetKerning( font, prev, index, &kern_x, NULL ) == DFB_OK)
               width += kern_x << 8;

          if (prev && font->GetKerning && font->GetKerning( font, prev, index, &kern_x, &kern_y) == DFB_OK) {
               xsize += kern_x << 8;
               ysize += kern_y << 8;
          }

          if (!ysize) {
               width = xsize;
          }
          else if (!xsize) {
               width = ysize;
          }
          else {
               width = sqrt(xsize * (xsize >> 8) + ysize * (ysize >> 8));
          }

          prev = index;
     } while ((width >> 8) < max_width && string < end && current != 0x0a);

     dfb_font_unlock( font );

     if ((width >> 8) < max_width && string >= end) {
          *ret_next_line = NULL;
          *ret_str_length = length;
          *ret_width = width >> 8;

          return DFB_OK;
     }

     if (*ret_next_line == NULL) {
          if (length == 1) {
               *ret_str_length = length;
               *ret_next_line = (const char*) string;
               *ret_width = width >> 8;
          } else {
               *ret_str_length = length-1;
               *ret_next_line = (const char*) last;
               /* ret_width already set in the loop */
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_SetEncoding( IDirectFBFont     *thiz,
                           DFBTextEncodingID  encoding )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p, %d )\n", __FUNCTION__, thiz, encoding );

     if (encoding > data->font->last_encoding)
          return DFB_IDNOTFOUND;

     data->encoding = encoding;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_EnumEncodings( IDirectFBFont           *thiz,
                             DFBTextEncodingCallback  callback,
                             void                    *context )
{
     int       i;
     CoreFont *font;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     if (!callback)
          return DFB_INVARG;

     D_DEBUG_AT( Font, "%s( %p, %p, %p )\n", __FUNCTION__, thiz, callback, context );

     font = data->font;

     if (callback( DTEID_UTF8, "UTF8", context ) == DFENUM_OK) {
          for (i=DTEID_OTHER; i<=font->last_encoding; i++) {
               if (callback( i, font->encodings[i]->name, context ) != DFENUM_OK)
                    break;
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_FindEncoding( IDirectFBFont     *thiz,
                            const char        *name,
                            DFBTextEncodingID *ret_id )
{
     int       i;
     CoreFont *font;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     if (!name || !ret_id)
          return DFB_INVARG;

     D_DEBUG_AT( Font, "%s( %p, '%s', %p )\n", __FUNCTION__, thiz, name, ret_id );

     if (!strcasecmp( name, "UTF8" )) {
          *ret_id = DTEID_UTF8;
          return DFB_OK;
     }

     font = data->font;

     for (i=DTEID_OTHER; i<=font->last_encoding; i++) {
          if (!strcasecmp( name, font->encodings[i]->name )) {
               *ret_id = i;
               return DFB_OK;
          }
     }

     return DFB_IDNOTFOUND;
}

static DFBResult
IDirectFBFont_Dispose( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     return dfb_font_dispose( data->font );
}

/*
 * Get the extents of the specified glyph.
 */
static DFBResult
IDirectFBFont_GetGlyphExtentsXY( IDirectFBFont *thiz,
                                 unsigned int   character,
                                 DFBRectangle  *rect,
                                 int           *xadvance,
                                 int           *yadvance )
{
     DFBResult      ret;
     CoreFont      *font;
     CoreGlyphData *glyph;
     unsigned int   index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!rect && !xadvance && !yadvance)
          return DFB_INVARG;

     font = data->font;

     dfb_font_lock( font );

     ret = dfb_font_decode_character( font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( font );
          return ret;
     }

     if (dfb_font_get_glyph_data (font, index, 0, &glyph) != DFB_OK) {     // FIXME: support font layers
          if (rect)
               rect->x = rect->y = rect->w = rect->h = 0;

          if (xadvance)
               *xadvance = 0;

          if (yadvance)
               *yadvance = 0;
     }
     else {
          if (rect) {
               rect->x = glyph->left + font->ascender * font->up_unit_x;
               rect->y = glyph->top  + font->ascender * font->up_unit_y;
               rect->w = glyph->width;
               rect->h = glyph->height;
          }

          if (xadvance)
               *xadvance = glyph->xadvance;

          if (yadvance)
               *yadvance = glyph->yadvance;
     }

     dfb_font_unlock( font );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetUnderline( IDirectFBFont *thiz,
                            int           *ret_underline_position,
                            int           *ret_underline_thickness )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (ret_underline_position)
          *ret_underline_position = data->font->underline_position;

     if (ret_underline_thickness)
          *ret_underline_thickness = data->font->underline_thickness;

     return DFB_OK;
}

/*
 * Get the description of the font.
 */
static DFBResult
IDirectFBFont_GetDescription( IDirectFBFont      *thiz,
                              DFBFontDescription *ret_description )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFont)

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_description)
          return DFB_INVARG;

     *ret_description = data->font->description;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFont *font )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFont)

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
     thiz->GetStringBreak = IDirectFBFont_GetStringBreak;
     thiz->SetEncoding = IDirectFBFont_SetEncoding;
     thiz->EnumEncodings = IDirectFBFont_EnumEncodings;
     thiz->FindEncoding = IDirectFBFont_FindEncoding;
     thiz->Dispose = IDirectFBFont_Dispose;
     thiz->GetLineSpacingVector = IDirectFBFont_GetLineSpacingVector;
     thiz->GetGlyphExtentsXY = IDirectFBFont_GetGlyphExtentsXY;
     thiz->GetUnderline = IDirectFBFont_GetUnderline;
     thiz->GetDescription = IDirectFBFont_GetDescription;

     return DFB_OK;
}

static DFBResult
try_map_file( IDirectFBDataBuffer_data   *buffer_data,
              IDirectFBFont_ProbeContext *ctx )
{
     /* try to map the "file" content first */
     if (direct_access( buffer_data->filename, R_OK ) == DR_OK) {
          DirectResult    ret;
          DirectFile      file;
          DirectFileInfo  info;
          void           *map;

          ret = direct_file_open( &file, ctx->filename, O_RDONLY, 0 );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not open '%s'\n", buffer_data->filename );
               return ret;
          }

          ret = direct_file_get_info( &file, &info );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not query info about '%s'\n", buffer_data->filename );
               direct_file_close( &file );
               return ret;
          }

          ret = direct_file_map( &file, NULL, 0, info.size, DFP_READ, &map );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not mmap '%s'\n", buffer_data->filename );
               direct_file_close( &file );
               return ret;
          }

          ctx->content      = map;
          ctx->content_size = info.size;
          ctx->content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED;

          direct_file_close( &file );

          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static void
unmap_or_free( IDirectFBFont_ProbeContext *ctx )
{
     if (ctx->content) {
          switch (ctx->content_type) {
               case IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED:
                    D_FREE( ctx->content );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED:
                    direct_file_unmap( NULL, ctx->content, ctx->content_size );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY:
                    break;

               default:
                    D_BUG( "unexpected content type %d", ctx->content_type );
          }

          ctx->content = NULL;
     }
}

DFBResult
IDirectFBFont_CreateFromBuffer( IDirectFBDataBuffer       *buffer,
                                CoreDFB                   *core,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **interface )
{
     DFBResult                   ret;
     DirectInterfaceFuncs       *funcs = NULL;
     IDirectFBDataBuffer_data   *buffer_data;
     IDirectFBFont              *ifont;
     IDirectFBFont_ProbeContext  ctx = { NULL, NULL, 0, IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN };

     /* Get the private information of the data buffer. */
     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Provide a fallback for image providers without data buffer support. */
     ctx.filename = buffer_data->filename;

     if (buffer_data->is_memory) {
          IDirectFBDataBuffer_Memory_data *mem_data = (IDirectFBDataBuffer_Memory_data*) buffer_data;

          ctx.content      = (unsigned char*) mem_data->buffer;
          ctx.content_size = mem_data->length;
          ctx.content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY;
     }
     /* try to map the "file" content first */
     else if (try_map_file( buffer_data, &ctx ) != DFB_OK) {
          /* try to load the "file" content from the buffer */

          /* we need to be able to seek (this implies non-streamed,
             so we also know the size) so we can reuse the buffer */
          if (buffer->SeekTo( buffer, 0 ) == DFB_OK) {
               unsigned int size, got;

               /* get the "file" length */
               buffer->GetLength( buffer, &size );

               ctx.content = D_MALLOC( size );
               if (!ctx.content)
                    return DR_NOLOCALMEMORY;

               ctx.content_size = 0;
               ctx.content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED;

               while (ctx.content_size < size) {
                    unsigned int get = size - ctx.content_size;

                    if (get > 8192)
                         get = 8192;

                    ret = buffer->WaitForData( buffer, get );
                    if (ret) {
                         D_DERROR( ret, "%s: WaitForData failed!\n", __FUNCTION__ );
                         break;
                    }

                    ret = buffer->GetData( buffer, get, ctx.content + ctx.content_size, &got );
                    if (ret) {
                         D_DERROR( ret, "%s: GetData failed!\n", __FUNCTION__ );
                         break;
                    }

                    if (!got)
                         break;

                    ctx.content_size += got;
               }

               if (ctx.content_size != size) {
                    D_ERROR( "%s: Got size %u differs from supposed %u!\n", __FUNCTION__, ctx.content_size, size );
                    D_FREE( ctx.content );
                    return DFB_FAILURE;
               }
          }
     }

     D_ASSERT( ctx.content_type != IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBFont", NULL, DirectProbeInterface, &ctx );
     if (ret) {
          unmap_or_free( &ctx );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( ifont, IDirectFBFont );

     /* Construct the interface. */
     ret = funcs->Construct( ifont, core, &ctx, desc );
     if (ret) {
          unmap_or_free( &ctx );
          return ret;
     }

     /* store pointer for deletion at destroy */
     {
          IDirectFBFont_data *data = (IDirectFBFont_data*)(ifont->priv);
          data->content = ctx.content;
          data->content_size = ctx.content_size;
          data->content_type = ctx.content_type;
     }

     *interface = ifont;

     return DFB_OK;
}

