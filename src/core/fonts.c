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

#include <directfb.h>

#include "fonts.h"
#include "core.h"
#include "coredefs.h"


static CoreFontData default_font;

#define FONTFILE DATADIR"/fonts/font.data"


void fonts_deinit()
{
     surface_destroy (default_font.surface);
     default_font.surface = NULL;
}

DFBResult fonts_load_default()
{
     FILE *f;
     __u8 *dst;
     int   pitch;
     int   i;

     f = fopen( FONTFILE, "r" );
     if (!f) {
          PERRORMSG( "Could not load default font '" FONTFILE "'!\n" );
          return DFB_INIT;
     }

     memset( &default_font, 0, sizeof(CoreFontData) );
     default_font.height = 20;
     default_font.ascender = 16;
     default_font.descender = 4;

     surface_create( 1024, default_font.height, DSPF_A8, CSP_VIDEOHIGH,
                     DSCAPS_ALPHA, &default_font.surface );

     {
          int start = 0;
          unsigned char points[1024];
          unsigned char *glyphs =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "01234567890!\"$\%&/()=?^<>"
                                   "|,;.:-_{[]}\\`+*~#'";

          fread( points, 1024, 1, f );

          for (i=0; i<1024; i++) {
               if (points[i] == 0xFF) {
                    default_font.glyphs[*glyphs].start   = start;
                    default_font.glyphs[*glyphs].width   = i - start;
                    default_font.glyphs[*glyphs].height  = default_font.height;
                    default_font.glyphs[*glyphs].left    = 0;
                    default_font.glyphs[*glyphs].top     = 0;
                    default_font.glyphs[*glyphs].advance = i - start + 1;
                    HEAVYDEBUGMSG( "DirectFB/core/fonts: glyph '%c' at %d, width %d\n", *glyphs, start, i-start );
                    if (default_font.maxadvance
                        < default_font.glyphs[*glyphs].width)
                    {
                         default_font.maxadvance =
                              default_font.glyphs[*glyphs].width;
                    }
                    start = i+1;
                    glyphs++;
               }
               if (*glyphs == 0)
                    break;
          }
     }

     surface_soft_lock( default_font.surface, 
                        DSLF_WRITE, (void **) &dst, &pitch, 0 );
     
     for (i = 0; i < default_font.height; i++) {
         fread( dst, 1024, 1, f);
         dst += pitch;
     }

     surface_unlock( default_font.surface, 0 );

     fclose( f );

     /* set space width */
     default_font.glyphs[32].advance = 5;

     core_cleanup_push( fonts_deinit );

     return DFB_OK;
}

CoreFontData* fonts_get_default()
{
     return &default_font;
}
