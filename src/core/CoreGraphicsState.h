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

#ifndef ___CoreGraphicsState__H___
#define ___CoreGraphicsState__H___

#include "CoreGraphicsState_includes.h"

/**********************************************************************************************************************
 * CoreGraphicsState
 */

#ifdef __cplusplus
#include <core/Interface.h>

extern "C" {
#endif


DFBResult CoreGraphicsState_SetDrawingFlags(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceDrawingFlags                     flags);

DFBResult CoreGraphicsState_SetBlittingFlags(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlittingFlags                    flags);

DFBResult CoreGraphicsState_SetClip(
                    CoreGraphicsState                         *obj,
                    const DFBRegion                           *region);

DFBResult CoreGraphicsState_SetColor(
                    CoreGraphicsState                         *obj,
                    const DFBColor                            *color);

DFBResult CoreGraphicsState_SetSrcBlend(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlendFunction                    function);

DFBResult CoreGraphicsState_SetDstBlend(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlendFunction                    function);

DFBResult CoreGraphicsState_SetSrcColorKey(
                    CoreGraphicsState                         *obj,
                    u32                                        key);

DFBResult CoreGraphicsState_SetDstColorKey(
                    CoreGraphicsState                         *obj,
                    u32                                        key);

DFBResult CoreGraphicsState_SetDestination(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface);

DFBResult CoreGraphicsState_SetSource(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface);

DFBResult CoreGraphicsState_SetSourceMask(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface);

DFBResult CoreGraphicsState_SetSourceMaskVals(
                    CoreGraphicsState                         *obj,
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags);

DFBResult CoreGraphicsState_SetIndexTranslation(
                    CoreGraphicsState                         *obj,
                    const u32                                 *indices,
                    u32                                        num);

DFBResult CoreGraphicsState_SetColorKey(
                    CoreGraphicsState                         *obj,
                    const DFBColorKey                         *key);

DFBResult CoreGraphicsState_SetRenderOptions(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceRenderOptions                    options);

DFBResult CoreGraphicsState_SetMatrix(
                    CoreGraphicsState                         *obj,
                    const u32                                 *values);

DFBResult CoreGraphicsState_SetSource2(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface);

DFBResult CoreGraphicsState_FillRectangles(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    u32                                        num);

DFBResult CoreGraphicsState_Blit(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num);

DFBResult CoreGraphicsState_StretchBlit(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num);


void *CoreGraphicsState_Init_Dispatch(
                    CoreDFB              *core,
                    CoreGraphicsState    *obj,
                    FusionCall           *call
);

void  CoreGraphicsState_Deinit_Dispatch(
                    void                 *dispatch
);


#ifdef __cplusplus
}


namespace DirectFB {


/*
 * CoreGraphicsState Calls
 */
typedef enum {
    CoreGraphicsState_SetDrawingFlags = 1,
    CoreGraphicsState_SetBlittingFlags = 2,
    CoreGraphicsState_SetClip = 3,
    CoreGraphicsState_SetColor = 4,
    CoreGraphicsState_SetSrcBlend = 5,
    CoreGraphicsState_SetDstBlend = 6,
    CoreGraphicsState_SetSrcColorKey = 7,
    CoreGraphicsState_SetDstColorKey = 8,
    CoreGraphicsState_SetDestination = 9,
    CoreGraphicsState_SetSource = 10,
    CoreGraphicsState_SetSourceMask = 11,
    CoreGraphicsState_SetSourceMaskVals = 12,
    CoreGraphicsState_SetIndexTranslation = 13,
    CoreGraphicsState_SetColorKey = 14,
    CoreGraphicsState_SetRenderOptions = 15,
    CoreGraphicsState_SetMatrix = 16,
    CoreGraphicsState_SetSource2 = 17,
    CoreGraphicsState_FillRectangles = 18,
    CoreGraphicsState_Blit = 19,
    CoreGraphicsState_StretchBlit = 20,
} CoreGraphicsStateCall;

/*
 * CoreGraphicsState_SetDrawingFlags
 */
typedef struct {
    DFBSurfaceDrawingFlags                     flags;
} CoreGraphicsStateSetDrawingFlags;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetDrawingFlagsReturn;


/*
 * CoreGraphicsState_SetBlittingFlags
 */
typedef struct {
    DFBSurfaceBlittingFlags                    flags;
} CoreGraphicsStateSetBlittingFlags;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetBlittingFlagsReturn;


/*
 * CoreGraphicsState_SetClip
 */
typedef struct {
    DFBRegion                                  region;
} CoreGraphicsStateSetClip;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetClipReturn;


/*
 * CoreGraphicsState_SetColor
 */
typedef struct {
    DFBColor                                   color;
} CoreGraphicsStateSetColor;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetColorReturn;


/*
 * CoreGraphicsState_SetSrcBlend
 */
typedef struct {
    DFBSurfaceBlendFunction                    function;
} CoreGraphicsStateSetSrcBlend;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSrcBlendReturn;


/*
 * CoreGraphicsState_SetDstBlend
 */
typedef struct {
    DFBSurfaceBlendFunction                    function;
} CoreGraphicsStateSetDstBlend;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetDstBlendReturn;


/*
 * CoreGraphicsState_SetSrcColorKey
 */
typedef struct {
    u32                                        key;
} CoreGraphicsStateSetSrcColorKey;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSrcColorKeyReturn;


/*
 * CoreGraphicsState_SetDstColorKey
 */
typedef struct {
    u32                                        key;
} CoreGraphicsStateSetDstColorKey;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetDstColorKeyReturn;


/*
 * CoreGraphicsState_SetDestination
 */
typedef struct {
    u32                                        surface_id;
} CoreGraphicsStateSetDestination;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetDestinationReturn;


/*
 * CoreGraphicsState_SetSource
 */
typedef struct {
    u32                                        surface_id;
} CoreGraphicsStateSetSource;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSourceReturn;


/*
 * CoreGraphicsState_SetSourceMask
 */
typedef struct {
    u32                                        surface_id;
} CoreGraphicsStateSetSourceMask;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSourceMaskReturn;


/*
 * CoreGraphicsState_SetSourceMaskVals
 */
typedef struct {
    DFBPoint                                   offset;
    DFBSurfaceMaskFlags                        flags;
} CoreGraphicsStateSetSourceMaskVals;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSourceMaskValsReturn;


/*
 * CoreGraphicsState_SetIndexTranslation
 */
typedef struct {
    u32                                        num;
    /* 'num' u32 follow (indices) */
} CoreGraphicsStateSetIndexTranslation;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetIndexTranslationReturn;


/*
 * CoreGraphicsState_SetColorKey
 */
typedef struct {
    DFBColorKey                                key;
} CoreGraphicsStateSetColorKey;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetColorKeyReturn;


/*
 * CoreGraphicsState_SetRenderOptions
 */
typedef struct {
    DFBSurfaceRenderOptions                    options;
} CoreGraphicsStateSetRenderOptions;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetRenderOptionsReturn;


/*
 * CoreGraphicsState_SetMatrix
 */
typedef struct {
    /* '9' u32 follow (values) */
} CoreGraphicsStateSetMatrix;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetMatrixReturn;


/*
 * CoreGraphicsState_SetSource2
 */
typedef struct {
    u32                                        surface_id;
} CoreGraphicsStateSetSource2;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateSetSource2Return;


/*
 * CoreGraphicsState_FillRectangles
 */
typedef struct {
    u32                                        num;
    /* 'num' DFBRectangle follow (rects) */
} CoreGraphicsStateFillRectangles;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateFillRectanglesReturn;


/*
 * CoreGraphicsState_Blit
 */
typedef struct {
    u32                                        num;
    /* 'num' DFBRectangle follow (rects) */
    /* 'num' DFBPoint follow (points) */
} CoreGraphicsStateBlit;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateBlitReturn;


/*
 * CoreGraphicsState_StretchBlit
 */
typedef struct {
    u32                                        num;
    /* 'num' DFBRectangle follow (srects) */
    /* 'num' DFBRectangle follow (drects) */
} CoreGraphicsStateStretchBlit;

typedef struct {
    DFBResult                                  result;
} CoreGraphicsStateStretchBlitReturn;





class IGraphicsState : public Interface
{
public:
    IGraphicsState( CoreDFB *core )
        :
        Interface( core )
    {
    }

public:
    virtual DFBResult SetDrawingFlags(
                    DFBSurfaceDrawingFlags                     flags
    ) = 0;

    virtual DFBResult SetBlittingFlags(
                    DFBSurfaceBlittingFlags                    flags
    ) = 0;

    virtual DFBResult SetClip(
                    const DFBRegion                           *region
    ) = 0;

    virtual DFBResult SetColor(
                    const DFBColor                            *color
    ) = 0;

    virtual DFBResult SetSrcBlend(
                    DFBSurfaceBlendFunction                    function
    ) = 0;

    virtual DFBResult SetDstBlend(
                    DFBSurfaceBlendFunction                    function
    ) = 0;

    virtual DFBResult SetSrcColorKey(
                    u32                                        key
    ) = 0;

    virtual DFBResult SetDstColorKey(
                    u32                                        key
    ) = 0;

    virtual DFBResult SetDestination(
                    CoreSurface                               *surface
    ) = 0;

    virtual DFBResult SetSource(
                    CoreSurface                               *surface
    ) = 0;

    virtual DFBResult SetSourceMask(
                    CoreSurface                               *surface
    ) = 0;

    virtual DFBResult SetSourceMaskVals(
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags
    ) = 0;

    virtual DFBResult SetIndexTranslation(
                    const u32                                 *indices,
                    u32                                        num
    ) = 0;

    virtual DFBResult SetColorKey(
                    const DFBColorKey                         *key
    ) = 0;

    virtual DFBResult SetRenderOptions(
                    DFBSurfaceRenderOptions                    options
    ) = 0;

    virtual DFBResult SetMatrix(
                    const u32                                 *values
    ) = 0;

    virtual DFBResult SetSource2(
                    CoreSurface                               *surface
    ) = 0;

    virtual DFBResult FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
    ) = 0;

    virtual DFBResult Blit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
    ) = 0;

    virtual DFBResult StretchBlit(
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
    ) = 0;

};



class IGraphicsState_Real : public IGraphicsState
{
private:
    CoreGraphicsState *obj;

public:
    IGraphicsState_Real( CoreDFB *core, CoreGraphicsState *obj )
        :
        IGraphicsState( core ),
        obj( obj )
    {
    }

public:
    virtual DFBResult SetDrawingFlags(
                    DFBSurfaceDrawingFlags                     flags
    );

    virtual DFBResult SetBlittingFlags(
                    DFBSurfaceBlittingFlags                    flags
    );

    virtual DFBResult SetClip(
                    const DFBRegion                           *region
    );

    virtual DFBResult SetColor(
                    const DFBColor                            *color
    );

    virtual DFBResult SetSrcBlend(
                    DFBSurfaceBlendFunction                    function
    );

    virtual DFBResult SetDstBlend(
                    DFBSurfaceBlendFunction                    function
    );

    virtual DFBResult SetSrcColorKey(
                    u32                                        key
    );

    virtual DFBResult SetDstColorKey(
                    u32                                        key
    );

    virtual DFBResult SetDestination(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSource(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSourceMask(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSourceMaskVals(
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags
    );

    virtual DFBResult SetIndexTranslation(
                    const u32                                 *indices,
                    u32                                        num
    );

    virtual DFBResult SetColorKey(
                    const DFBColorKey                         *key
    );

    virtual DFBResult SetRenderOptions(
                    DFBSurfaceRenderOptions                    options
    );

    virtual DFBResult SetMatrix(
                    const u32                                 *values
    );

    virtual DFBResult SetSource2(
                    CoreSurface                               *surface
    );

    virtual DFBResult FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
    );

    virtual DFBResult Blit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
    );

    virtual DFBResult StretchBlit(
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
    );

};



class IGraphicsState_Requestor : public IGraphicsState
{
private:
    CoreGraphicsState *obj;

public:
    IGraphicsState_Requestor( CoreDFB *core, CoreGraphicsState *obj )
        :
        IGraphicsState( core ),
        obj( obj )
    {
    }

public:
    virtual DFBResult SetDrawingFlags(
                    DFBSurfaceDrawingFlags                     flags
    );

    virtual DFBResult SetBlittingFlags(
                    DFBSurfaceBlittingFlags                    flags
    );

    virtual DFBResult SetClip(
                    const DFBRegion                           *region
    );

    virtual DFBResult SetColor(
                    const DFBColor                            *color
    );

    virtual DFBResult SetSrcBlend(
                    DFBSurfaceBlendFunction                    function
    );

    virtual DFBResult SetDstBlend(
                    DFBSurfaceBlendFunction                    function
    );

    virtual DFBResult SetSrcColorKey(
                    u32                                        key
    );

    virtual DFBResult SetDstColorKey(
                    u32                                        key
    );

    virtual DFBResult SetDestination(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSource(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSourceMask(
                    CoreSurface                               *surface
    );

    virtual DFBResult SetSourceMaskVals(
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags
    );

    virtual DFBResult SetIndexTranslation(
                    const u32                                 *indices,
                    u32                                        num
    );

    virtual DFBResult SetColorKey(
                    const DFBColorKey                         *key
    );

    virtual DFBResult SetRenderOptions(
                    DFBSurfaceRenderOptions                    options
    );

    virtual DFBResult SetMatrix(
                    const u32                                 *values
    );

    virtual DFBResult SetSource2(
                    CoreSurface                               *surface
    );

    virtual DFBResult FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
    );

    virtual DFBResult Blit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
    );

    virtual DFBResult StretchBlit(
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
    );

};



class CoreGraphicsStateDispatch
{

public:
    CoreGraphicsStateDispatch( CoreDFB *core, IGraphicsState *real )
        :
        core( core ),
        real( real )
    {
    }


    virtual DFBResult Dispatch( int           method,
                                void         *ptr,
                                unsigned int  length,
                                void         *ret_ptr,
                                unsigned int  ret_size,
                                unsigned int *ret_length );

private:
    CoreDFB              *core;
    IGraphicsState       *real;
};


}

#endif

#endif
