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
 * CoreGraphicsState
 */

/*
 * CoreGraphicsState Calls
 */
typedef enum {
     CORE_GRAPHICS_STATE_SET_DESTINATION     = 1,
     CORE_GRAPHICS_STATE_SET_SOURCE          = 2,
     CORE_GRAPHICS_STATE_SET_CLIP            = 3,
     CORE_GRAPHICS_STATE_SET_COLOR           = 4,
     CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS    = 5,
     CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS   = 6,
     CORE_GRAPHICS_STATE_SET_SRC_BLEND       = 7,
     CORE_GRAPHICS_STATE_SET_DST_BLEND       = 8,
     CORE_GRAPHICS_STATE_SET_SRC_COLORKEY    = 9,
     CORE_GRAPHICS_STATE_SET_DST_COLORKEY    = 10,

     CORE_GRAPHICS_STATE_FILL_RECTANGLE      = 11, // TEST
     CORE_GRAPHICS_STATE_FILL_RECTANGLES     = 12, // TEST
     CORE_GRAPHICS_STATE_BLIT                = 13, // TEST
     CORE_GRAPHICS_STATE_STRETCH_BLIT        = 14, // TEST
} CoreGraphicsStateCall;

/*
 * CORE_GRAPHICS_STATE_SET_DESTINATION
 */
typedef struct {
     u32                      object_id;
} CoreGraphicsStateSetDestination;

/*
 * CORE_GRAPHICS_STATE_SET_SOURCE
 */
typedef struct {
     u32                      object_id;
} CoreGraphicsStateSetSource;

/*
 * CORE_GRAPHICS_STATE_SET_CLIP
 */
typedef struct {
     DFBRegion                clip;
} CoreGraphicsStateSetClip;

/*
 * CORE_GRAPHICS_STATE_SET_COLOR
 */
typedef struct {
     DFBColor                 color;
} CoreGraphicsStateSetColor;

/*
 * CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS
 */
typedef struct {
     DFBSurfaceDrawingFlags   flags;
} CoreGraphicsStateSetDrawingFlags;

/*
 * CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS
 */
typedef struct {
     DFBSurfaceBlittingFlags  flags;
} CoreGraphicsStateSetBlittingFlags;

/*
 * CORE_GRAPHICS_STATE_SET_SRC_BLEND
 */
typedef struct {
     DFBSurfaceBlendFunction  function;
} CoreGraphicsStateSetSrcBlend;

/*
 * CORE_GRAPHICS_STATE_SET_DST_BLEND
 */
typedef struct {
     DFBSurfaceBlendFunction  function;
} CoreGraphicsStateSetDstBlend;

/*
 * CORE_GRAPHICS_STATE_SET_SRC_COLORKEY
 */
typedef struct {
     u32                      key;
} CoreGraphicsStateSetSrcColorkey;

/*
 * CORE_GRAPHICS_STATE_SET_DST_COLORKEY
 */
typedef struct {
     u32                      key;
} CoreGraphicsStateSetDstColorkey;


/*
 * CORE_GRAPHICS_STATE_FILL_RECTANGLE
 */
typedef struct {
     DFBRectangle             rect;
} CoreGraphicsStateFillRectangle;

/*
 * CORE_GRAPHICS_STATE_FILL_RECTANGLES
 */
typedef struct {
     unsigned int             num;

     /* rectangles follow */
} CoreGraphicsStateFillRectangles;

/*
 * CORE_GRAPHICS_STATE_BLIT
 */
typedef struct {
     unsigned int             num;

     /* rectangles, points follow */
} CoreGraphicsStateBlit;

/*
 * CORE_GRAPHICS_STATE_STRETCH_BLIT
 */
typedef struct {
     unsigned int             num;

     /* rectangles for source, destination follow */
} CoreGraphicsStateStretchBlit;




struct __DFB_CoreGraphicsStateClient {
     int            magic;

     CoreDFB       *core;
     CardState     *state;

     CoreGraphicsState *gfx_state;
};


DFBResult CoreGraphicsStateClient_Init          ( CoreGraphicsStateClient *client,
                                                  CardState               *state );

DFBResult CoreGraphicsStateClient_SetState      ( CoreGraphicsStateClient *client,
                                                  CardState               *state,
                                                  StateModificationFlags   flags );
DFBResult CoreGraphicsStateClient_Update        ( CoreGraphicsStateClient *client,
                                                  DFBAccelerationMask      accel,
                                                  CardState               *state );

// TEST
DFBResult CoreGraphicsStateClient_FillRectangle ( CoreGraphicsStateClient *client,
                                                  const DFBRectangle      *rect );

// TEST
DFBResult CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                                  const DFBRectangle      *rects,
                                                  unsigned int             num );

// TEST
DFBResult CoreGraphicsStateClient_Blit          ( CoreGraphicsStateClient *client,
                                                  const DFBRectangle      *rects,
                                                  const DFBPoint          *points,
                                                  unsigned int             num );

// TEST
DFBResult CoreGraphicsStateClient_StretchBlit   ( CoreGraphicsStateClient *client,
                                                  const DFBRectangle      *srects,
                                                  const DFBRectangle      *drects,
                                                  unsigned int             num );

#endif

