/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__CORE_GRAPHICS_STATE_H__
#define __CORE__CORE_GRAPHICS_STATE_H__


#include <directfb.h>

#include <core/state.h>


/**********************************************************************************************************************
 * CoreGraphicsStateClient
 */

struct __DFB_CoreGraphicsStateClient {
     int                magic;

     CoreDFB           *core;
     CardState         *state;          /* Local state structure */

     CoreGraphicsState *gfx_state;      /* Remote object for rendering, syncing values from local state as needed */
};


DFBResult CoreGraphicsStateClient_Init            ( CoreGraphicsStateClient *client,
                                                    CardState               *state );

void      CoreGraphicsStateClient_Deinit          ( CoreGraphicsStateClient *client );

void      CoreGraphicsStateClient_Flush           ( CoreGraphicsStateClient *client );
void      CoreGraphicsStateClient_FlushAll        ( void );
void      CoreGraphicsStateClient_FlushAllDst     ( CoreSurface             *surface );

DFBResult CoreGraphicsStateClient_ReleaseSource   ( CoreGraphicsStateClient *client );

DFBResult CoreGraphicsStateClient_SetColorAndIndex( CoreGraphicsStateClient *client,
                                                    const DFBColor          *color,
                                                    u32                      index );

DFBResult CoreGraphicsStateClient_SetState        ( CoreGraphicsStateClient *client,
                                                    CardState               *state,
                                                    StateModificationFlags   flags );
DFBResult CoreGraphicsStateClient_Update          ( CoreGraphicsStateClient *client,
                                                    DFBAccelerationMask      accel,
                                                    CardState               *state );

DFBResult CoreGraphicsStateClient_DrawRectangles  ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *rects,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_DrawLines       ( CoreGraphicsStateClient *client,
                                                    const DFBRegion         *lines,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_FillRectangles  ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *rects,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_FillTriangles   ( CoreGraphicsStateClient *client,
                                                    const DFBTriangle       *triangles,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_FillTrapezoids  ( CoreGraphicsStateClient *client,
                                                    const DFBTrapezoid      *trapezoids,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_FillSpans       ( CoreGraphicsStateClient *client,
                                                    int                      y,
                                                    const DFBSpan           *spans,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_Blit            ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *rects,
                                                    const DFBPoint          *points,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_Blit2           ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *rects,
                                                    const DFBPoint          *points1,
                                                    const DFBPoint          *points2,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_StretchBlit     ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *srects,
                                                    const DFBRectangle      *drects,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_TileBlit        ( CoreGraphicsStateClient *client,
                                                    const DFBRectangle      *rects,
                                                    const DFBPoint          *points1,
                                                    const DFBPoint          *points2,
                                                    unsigned int             num );

DFBResult CoreGraphicsStateClient_TextureTriangles( CoreGraphicsStateClient *client,
                                                    const DFBVertex         *vertices,
                                                    int                      num,
                                                    DFBTriangleFormation     formation );

#endif

