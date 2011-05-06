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

#ifndef __CORE__GRAPHICS_STATE_H__
#define __CORE__GRAPHICS_STATE_H__


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
     CORE_GRAPHICS_STATE_SET_CLIP            = 2,
     CORE_GRAPHICS_STATE_SET_COLOR           = 3,

     CORE_GRAPHICS_STATE_FILL_RECTANGLE      = 4, // TEST
     CORE_GRAPHICS_STATE_FILL_RECTANGLES     = 5, // TEST
} CoreGraphicsStateCall;

/*
 * CORE_GRAPHICS_STATE_SET_DESTINATION
 */
typedef struct {
     u32                 object_id;
} CoreGraphicsStateSetDestination;

/*
 * CORE_GRAPHICS_STATE_SET_CLIP
 */
typedef struct {
     DFBRegion           clip;
} CoreGraphicsStateSetClip;

/*
 * CORE_GRAPHICS_STATE_SET_COLOR
 */
typedef struct {
     DFBColor            color;
} CoreGraphicsStateSetColor;

/*
 * CORE_GRAPHICS_STATE_FILL_RECTANGLE
 */
typedef struct {
     DFBRectangle        rect;
} CoreGraphicsStateFillRectangle;

/*
 * CORE_GRAPHICS_STATE_FILL_RECTANGLES
 */
typedef struct {
     unsigned int        num;

     /* rectangles follow */
} CoreGraphicsStateFillRectangles;




typedef struct {
     int            magic;

     CoreDFB       *core;

     FusionCall     call;
} CoreGraphicsStateClient;


DFBResult CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                                        CoreDFB                 *core );

DFBResult CoreGraphicsStateClient_SetState( CoreGraphicsStateClient *client,
                                            CardState               *state,
                                            StateModificationFlags   flags );

// TEST
DFBResult CoreGraphicsStateClient_FillRectangle( CoreGraphicsStateClient *client,
                                                 const DFBRectangle      *rect );

// TEST
DFBResult CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                                  const DFBRectangle      *rects,
                                                  unsigned int             num );

#endif

