/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __IDIRECTFBSURFACE_DISPATCHER_H__
#define __IDIRECTFBSURFACE_DISPATCHER_H__

#define IDIRECTFBSURFACE_METHOD_ID_AddRef                     1
#define IDIRECTFBSURFACE_METHOD_ID_Release                    2
#define IDIRECTFBSURFACE_METHOD_ID_GetCapabilities            3
#define IDIRECTFBSURFACE_METHOD_ID_GetSize                    4
#define IDIRECTFBSURFACE_METHOD_ID_GetVisibleRectangle        5
#define IDIRECTFBSURFACE_METHOD_ID_GetPixelFormat             6
#define IDIRECTFBSURFACE_METHOD_ID_GetAccelerationMask        7
#define IDIRECTFBSURFACE_METHOD_ID_GetPalette                 8
#define IDIRECTFBSURFACE_METHOD_ID_SetPalette                 9
#define IDIRECTFBSURFACE_METHOD_ID_Lock                      10
#define IDIRECTFBSURFACE_METHOD_ID_Unlock                    11
#define IDIRECTFBSURFACE_METHOD_ID_Flip                      12
#define IDIRECTFBSURFACE_METHOD_ID_SetField                  13
#define IDIRECTFBSURFACE_METHOD_ID_Clear                     14
#define IDIRECTFBSURFACE_METHOD_ID_SetClip                   15
#define IDIRECTFBSURFACE_METHOD_ID_SetColor                  16
#define IDIRECTFBSURFACE_METHOD_ID_SetColorIndex             17
#define IDIRECTFBSURFACE_METHOD_ID_SetSrcBlendFunction       18
#define IDIRECTFBSURFACE_METHOD_ID_SetDstBlendFunction       19
#define IDIRECTFBSURFACE_METHOD_ID_SetPorterDuff             20
#define IDIRECTFBSURFACE_METHOD_ID_SetSrcColorKey            21
#define IDIRECTFBSURFACE_METHOD_ID_SetSrcColorKeyIndex       22
#define IDIRECTFBSURFACE_METHOD_ID_SetDstColorKey            23
#define IDIRECTFBSURFACE_METHOD_ID_SetDstColorKeyIndex       24
#define IDIRECTFBSURFACE_METHOD_ID_SetBlittingFlags          25
#define IDIRECTFBSURFACE_METHOD_ID_Blit                      26
#define IDIRECTFBSURFACE_METHOD_ID_TileBlit                  27
#define IDIRECTFBSURFACE_METHOD_ID_BatchBlit                 28
#define IDIRECTFBSURFACE_METHOD_ID_StretchBlit               29
#define IDIRECTFBSURFACE_METHOD_ID_TextureTriangles          30
#define IDIRECTFBSURFACE_METHOD_ID_SetDrawingFlags           31
#define IDIRECTFBSURFACE_METHOD_ID_FillRectangle             32
#define IDIRECTFBSURFACE_METHOD_ID_DrawLine                  33
#define IDIRECTFBSURFACE_METHOD_ID_DrawLines                 34
#define IDIRECTFBSURFACE_METHOD_ID_DrawRectangle             35
#define IDIRECTFBSURFACE_METHOD_ID_FillTriangle              36
#define IDIRECTFBSURFACE_METHOD_ID_SetFont                   37
#define IDIRECTFBSURFACE_METHOD_ID_GetFont                   38
#define IDIRECTFBSURFACE_METHOD_ID_DrawString                39
#define IDIRECTFBSURFACE_METHOD_ID_DrawGlyph                 40
#define IDIRECTFBSURFACE_METHOD_ID_GetSubSurface             41
#define IDIRECTFBSURFACE_METHOD_ID_GetGL                     42
#define IDIRECTFBSURFACE_METHOD_ID_Dump                      43

#endif
