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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <gfx/convert.h>

#include <media/idirectfbfont.h>

#include <misc/tree.h>
#include <misc/utf8.h>
#include <misc/mem.h>

static DFBResult
Probe( void *data );

static DFBResult
Construct( IDirectFBFont      *thiz,
           const char         *filename,
           DFBFontDescription *desc );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBFont, Default )


#define FONTFILE DATADIR"/font.data"


static DFBResult
Probe( void *data )
{
     return DFB_OK;
}

static DFBResult
Construct( IDirectFBFont      *thiz,
           const char         *filename,
           DFBFontDescription *desc )
{
     CoreFont *font;
     CoreSurface *surface;
     FILE *f;
     __u8 *dst;
     int pitch;
     int i;

     HEAVYDEBUGMSG( "DirectFB/FontDefault: Construct default font");

     f = fopen( FONTFILE, "r" );
     if (!f) {
          PERRORMSG( "Could not load default font '" FONTFILE "'!\n" );
          return DFB_FAILURE;
     }

     font = dfb_font_create();

     font->height    = 20;
     font->ascender  = 16;
     font->descender = 4;

     dfb_surface_create( 1024, font->height,
                         dfb_config->argb_font ? DSPF_ARGB : DSPF_A8,
                         CSP_VIDEOHIGH, DSCAPS_NONE, &surface );

     font->rows = 1;
     font->row_width = 1024;
     font->surfaces = DFBMALLOC(sizeof (void *));
     font->surfaces[0] = surface;

     {
          CoreGlyphData *data;
          int start = 0;
          unsigned char points[1024];
          unsigned char *glyphs =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "01234567890!\"$\%&/()=?^<>"
                                   "|,;.:-_{[]}\\`+*~#'";

          fread( points, 1024, 1, f );

          for (i=0; i<1024; i++) {
               if (points[i] == 0xFF) {
                 data = DFBMALLOC(sizeof (CoreGlyphData));
                 data->surface = surface;
                 data->start   = start;
                 data->width   = i - start;
                 data->height  = font->height;
                 data->left    = 0;
                 data->top     = 0;
                 data->advance = data->width + 1;
                 HEAVYDEBUGMSG( "DirectFB/core/fonts: glyph '%c' at %d, width %d\n",
                                *glyphs, start, i-start );

                 if (font->maxadvance < data->width)
                      font->maxadvance = data->width;

                 dfb_tree_insert (font->glyph_infos,
                              (void *) dfb_utf8_get_char (glyphs), data);

                 start = i+1;
                 glyphs++;
               }
               if (*glyphs == 0)
                    break;
          }

          /*  space  */
          data = DFBCALLOC(1, sizeof (CoreGlyphData));
          data->advance = 5;
          dfb_tree_insert (font->glyph_infos,
                       (void *) dfb_utf8_get_char (" "), data);
     }

     dfb_surface_soft_lock( surface, DSLF_WRITE, (void **) &dst, &pitch, 0 );

     for (i = 0; i < font->height; i++) {
          if (dfb_config->argb_font) {
               char buf[1024];

               fread( buf, 1024, 1, f);
               span_a8_to_argb(buf, (__u32*)dst, 1024);
          }
          else {
               fread( dst, 1024, 1, f);
          }
         dst += pitch;
     }

     dfb_surface_unlock( surface, 0 );

     fclose( f );

     IDirectFBFont_Construct (thiz, font);

     return DFB_OK;
}


