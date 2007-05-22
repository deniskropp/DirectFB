/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __STRETCH_ALGOS_H__
#define __STRETCH_ALGOS_H__



typedef void (*StretchFunc)( void        *dst,
                             int          dpitch,
                             const void  *src,
                             int          spitch,
                             int          width,
                             int          height,
                             int          dst_width,
                             int          dst_height,
                             DFBRegion   *clip );

typedef void (*StretchFuncKeyed)( void        *dst,
                                  int          dpitch,
                                  const void  *src,
                                  int          spitch,
                                  int          width,
                                  int          height,
                                  int          dst_width,
                                  int          dst_height,
                                  DFBRegion   *clip,
                                  u16          src_key );

typedef void (*StretchIndexedFunc)( void           *dst,
                                    int             dpitch,
                                    const void     *src,
                                    int             spitch,
                                    int             width,
                                    int             height,
                                    int             dst_width,
                                    int             dst_height,
                                    const DFBColor *palette );

typedef struct {
     const char         *name;
     const char         *description;

     StretchFunc         func_rgb16;
     StretchFuncKeyed    func_rgb16_keyed;
     StretchFunc         func_argb4444;

     /* FIXME: Only RGB16 supported for conversion right now. */
     StretchIndexedFunc  func_rgb16_indexed;
     StretchFunc         func_rgb16_from32;
} StretchAlgo;


extern const StretchAlgo wm_stretch_simple;
extern const StretchAlgo wm_stretch_down;
extern const StretchAlgo wm_stretch_up;

#endif

