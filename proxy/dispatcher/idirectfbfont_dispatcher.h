/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#ifndef __IDIRECTFBFONT_DISPATCHER_H__
#define __IDIRECTFBFONT_DISPATCHER_H__

#define IDIRECTFBFONT_METHOD_ID_AddRef                     1
#define IDIRECTFBFONT_METHOD_ID_Release                    2
#define IDIRECTFBFONT_METHOD_ID_GetAscender                3
#define IDIRECTFBFONT_METHOD_ID_GetDescender               4
#define IDIRECTFBFONT_METHOD_ID_GetHeight                  5
#define IDIRECTFBFONT_METHOD_ID_GetMaxAdvance              6
#define IDIRECTFBFONT_METHOD_ID_GetKerning                 7
#define IDIRECTFBFONT_METHOD_ID_GetStringWidth             8
#define IDIRECTFBFONT_METHOD_ID_GetStringExtents           9
#define IDIRECTFBFONT_METHOD_ID_GetGlyphExtents           10

#endif
