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

#ifndef __DIRECTFBFONT_H__
#define __DIRECTFBFONT_H__

#include <core/fonts.h>

/*
 * private data struct of IDirectFBFont
 */
typedef struct {
     int           ref;   /* reference counter */
     CoreFontData *font;  /* pointer to core data */
} IDirectFBFont_data;

/*
 * common code to construct the interface (internal usage only)
 */
DFBResult IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFontData *font );

/*
 * deinitialize font and its surface
 */
void IDirectFBFont_Destruct( IDirectFBFont *thiz );

#endif
