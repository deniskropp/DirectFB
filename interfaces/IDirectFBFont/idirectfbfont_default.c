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

#include <malloc.h>

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

static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBFont      *thiz,
           const char         *filename,
           DFBFontDescription *desc );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBFont, Default )


#define FONTFILE DATADIR"/font.data"


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
          DFB_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     font = dfb_font_create();

     DFB_ASSERT( font->pixel_format == DSPF_ARGB || 
                 font->pixel_format == DSPF_A8 );

     font->height    = 24;
     font->ascender  = 16;
     font->descender = -4;

     dfb_surface_create( 1024, 20, font->pixel_format,
                         CSP_VIDEOHIGH, DSCAPS_NONE, NULL, &surface );

     font->rows = 1;
     font->row_width = 1024;
     font->surfaces = DFBMALLOC(sizeof (void *));
     font->surfaces[0] = surface;

     {
          CoreGlyphData *data;
          int use_unicode;
          int start = 0;
	  int index = 0;
          int key;
          unsigned char points[1024];
          unsigned char *glyphs =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "01234567890!\"$\%&/()=?^<>"
                                   "|,;.:-_{[]}\\`+*~#'";

	  if ((desc->flags & DFDESC_ATTRIBUTES) &&
	      (desc->attributes & DFFA_NOCHARMAP))
               use_unicode = 0;
	  else
	       use_unicode = 1;
	  
          fread( points, 1024, 1, f );

          for (i=0; i<1024; i++) {
               if (points[i] == 0xFF) {
                    data = DFBMALLOC(sizeof (CoreGlyphData));
                    data->surface = surface;
                    data->start   = start;
                    data->width   = i - start + 1;
                    data->height  = 20;
                    data->left    = 0;
                    data->top     = 0;
                    data->advance = data->width + 1;
                    HEAVYDEBUGMSG( "DirectFB/core/fonts: "
				   "glyph '%c' at %d, width %d\n",
                                   glyphs[index], start, i-start );

                    if (font->maxadvance < data->advance)
                         font->maxadvance = data->advance;

		    if (use_unicode)
                         key = dfb_utf8_get_char (glyphs+index);
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
               key = dfb_utf8_get_char (" ");
          else
               key = index;
	    
          dfb_tree_insert (font->glyph_infos, (void*) key, data);
     }

     dfb_surface_soft_lock( surface, DSLF_WRITE, (void **) &dst, &pitch, 0 );

     for (i = 0; i < 20; i++) {
          switch (surface->format) {
               case DSPF_ARGB:
                    {
                         char buf[1024];

                         fread( buf, 1024, 1, f);
                         span_a8_to_argb(buf, (__u32*)dst, 1024);
                    }
                    break;
               case DSPF_A8:
                    fread( dst, 1024, 1, f);
                    break;
               default:
                    break;
          }
          dst += pitch;
     }

     dfb_surface_unlock( surface, 0 );

     fclose( f );

     return IDirectFBFont_Construct (thiz, font);
}
