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

#include <directfb.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <gfx/convert.h>

#include <media/idirectfbfont.h>

#include <misc/tree.h>
#include <misc/utf8.h>
#include <misc/mem.h>
#include <misc/memcpy.h>

#include "default_font.h"

#define DEFAULT_FONT_HEIGHT     24
#define DEFAULT_FONT_ASCENDER   16
#define DEFAULT_FONT_DESCENDER  -4


static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBFont      *thiz,
           const char         *filename,
           DFBFontDescription *desc );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBFont, Default )


static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx )
{
     /* default font is created with a NULL filename */
     if (ctx->filename)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
Construct( IDirectFBFont      *thiz,
           const char         *filename,
           DFBFontDescription *desc )
{
     CoreFont    *font;
     CoreSurface *surface;
     void        *dst;
     __u8        *pixels;
     int          pitch;
     int          i;

     HEAVYDEBUGMSG( "DirectFB/FontDefault: Construct default font");

     font = dfb_font_create();

     DFB_ASSERT( font->pixel_format == DSPF_ARGB || 
                 font->pixel_format == DSPF_A8 );

     font->height    = DEFAULT_FONT_HEIGHT;
     font->ascender  = DEFAULT_FONT_ASCENDER;
     font->descender = DEFAULT_FONT_DESCENDER;

     dfb_surface_create( font_desc.width, font_desc.height, font->pixel_format,
                         CSP_VIDEOHIGH, DSCAPS_NONE, NULL, &surface );

     font->rows = 1;
     font->row_width = font_desc.width;
     font->surfaces = DFBMALLOC(sizeof (void *));
     font->surfaces[0] = surface;

     pixels = font_data;

     {
          CoreGlyphData *data;
          int use_unicode;
          int start = 0;
          int index = 0;
          int key;
          const char *glyphs =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyz"
          "01234567890!\"$%&/()=?^<>"
          "|,;.:-_{[]}\\`+*~#'";

          if (desc && (desc->flags & DFDESC_ATTRIBUTES) &&
              (desc->attributes & DFFA_NOCHARMAP))
               use_unicode = 0;
          else
               use_unicode = 1;

          for (i = 0; i < font_desc.width; i++) {
               if (pixels[i] == 0xFF) {
                    data = DFBMALLOC(sizeof (CoreGlyphData));
                    data->surface = surface;
                    data->start   = start;
                    data->width   = i - start + 1;
                    data->height  = font_desc.height - 1;
                    data->left    = 0;
                    data->top     = 0;
                    data->advance = ((desc && (desc->flags &
                                               DFDESC_FIXEDADVANCE)) ?
                                     desc->fixed_advance :
                                     data->width + 1);

                    HEAVYDEBUGMSG( "DirectFB/core/fonts: "
                                   "glyph '%c' at %d, width %d\n",
                                   glyphs[index], start, i-start );

                    if (font->maxadvance < data->advance)
                         font->maxadvance = data->advance;

                    if (use_unicode)
                         key = glyphs[index];
                    else
                         key = index;

                    dfb_tree_insert (font->glyph_infos, (void*) key, data);

                    start = i + 1;
                    index++;
               }
               if (glyphs[index] == '\0')
                    break;
          }

          /*  space  */
          data = DFBCALLOC(1, sizeof (CoreGlyphData));
          data->advance = 5;

          if (use_unicode)
               key = 32;
          else
               key = index;

          dfb_tree_insert (font->glyph_infos, (void*) key, data);
     }

     dfb_surface_soft_lock( surface, DSLF_WRITE, &dst, &pitch, 0 );

     for (i = 1; i < font_desc.height; i++) {
          int    n;
          __u32 *dst32 = dst;

          pixels += font_desc.preallocated[0].pitch;
          switch (surface->format) {
               case DSPF_ARGB:
                    for (n=0; n<font_desc.width; n++)
                         dst32[n] = (pixels[n] << 24) | 0xFFFFFF;
                    break;
               case DSPF_A8:
                    dfb_memcpy(dst, pixels, font_desc.width);
                    break;
               default:
                    break;
          }

          dst += pitch;
     }

     dfb_surface_unlock( surface, 0 );

     return IDirectFBFont_Construct (thiz, font);
}
