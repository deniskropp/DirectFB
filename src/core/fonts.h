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

#ifndef __FONTS_H__
#define __FONTS_H__

#include <directfb.h>

#include "surfaces.h"

/*
 * font struct, includes kerning table for each character
 */
typedef struct {
     CoreSurface    *surface;           /* contains bitmaps of the glyphs  */

     int            height;             /* font height                     */

     int            ascender;           /* a positive value, the distance
                                           from the baseline to the top    */
     int            descender;          /* a negative value, the distance
                                           from the baseline to the bottom */
     int            maxadvance;         /* width of largest character      */

     struct {
          int       start;              /* x offset of glyph in surface    */
          int       width;              /* width of the glyphs bitmap      */
          int       height;             /* height of the glyphs bitmap     */
          int       left;               /* x offset of the glyph           */
          int       top;                /* y offset of the glyph           */
          int       advance;            /* placement of next glyph         */
     } glyphs[256];

     char          *kerning_table;
} CoreFontData;

/*
 * loads default DirectFB bitmap font
 */
DFBResult fonts_load_default();

void fonts_deinit();

CoreFontData* fonts_get_default();

#endif
