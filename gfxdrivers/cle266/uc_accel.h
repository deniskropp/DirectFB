/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UC_ACCEL_H__
#define __UC_ACCEL_H__

#include "unichrome.h"


// 2D accelerator capabilites

#define UC_DRAWING_FLAGS_2D        (DSDRAW_XOR)

#define UC_BLITTING_FLAGS_2D       (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY)

#define UC_DRAWING_FUNCTIONS_2D    (DFXL_DRAWLINE      | \
                                    DFXL_DRAWRECTANGLE | \
                                    DFXL_FILLRECTANGLE)

#define UC_BLITTING_FUNCTIONS_2D   (DFXL_BLIT)


// 3D accelerator capabilites

#ifdef UC_ENABLE_3D

#define UC_DRAWING_FLAGS_3D        (DSDRAW_BLEND | DSDRAW_XOR)

#define UC_BLITTING_FLAGS_3D       (DSBLIT_BLEND_ALPHACHANNEL | \
                                    DSBLIT_BLEND_COLORALPHA   | \
                                    DSBLIT_COLORIZE           | \
                                    DSBLIT_DEINTERLACE)

#define UC_DRAWING_FUNCTIONS_3D    (DFXL_DRAWLINE      | \
                                    DFXL_DRAWRECTANGLE | \
                                    DFXL_FILLRECTANGLE | \
                                    DFXL_FILLTRIANGLE)

#define UC_BLITTING_FUNCTIONS_3D   (DFXL_BLIT        | \
                                    DFXL_STRETCHBLIT | \
                                    DFXL_TEXTRIANGLES)

#else

#define UC_DRAWING_FLAGS_3D        0
#define UC_BLITTING_FLAGS_3D       0
#define UC_DRAWING_FUNCTIONS_3D    0
#define UC_BLITTING_FUNCTIONS_3D   0

#endif // UC_ENABLE_3D


// Functions

void uc_emit_commands      ( void         *drv,
                             void         *dev );

void uc_flush_texture_cache( void         *drv,
                             void         *dev );

bool uc_fill_rectangle     ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect );

bool uc_draw_rectangle     ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect );

bool uc_draw_line          ( void         *drv,
                             void         *dev,
                             DFBRegion    *line );

bool uc_blit               ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect,
                             int           dx,
                             int           dy );

bool uc_fill_rectangle_3d  ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect );

bool uc_draw_rectangle_3d  ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect );

bool uc_draw_line_3d       ( void         *drv,
                             void         *dev,
                             DFBRegion    *line );

bool uc_fill_triangle      ( void         *drv,
                             void         *dev,
                             DFBTriangle  *tri );

bool uc_blit_3d            ( void         *drv,
                             void         *dev,
                             DFBRectangle *rect,
                             int           dx,
                             int           dy );

bool uc_stretch_blit       ( void         *drv,
                             void         *dev,
                             DFBRectangle *srect,
                             DFBRectangle *drect );

bool uc_texture_triangles  ( void         *drv,
                             void         *dev,
                             DFBVertex    *vertices,
                             int           num,
                             DFBTriangleFormation formation );

#endif // __UC_ACCEL_H__

