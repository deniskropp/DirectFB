/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
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

#ifndef IDIRECTFBSURFACE_H
#define IDIRECTFBSURFACE_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBSurface {
friend
     class IDirectFB;
friend
     class IDirectFBDisplayLayer;
friend
     class IDirectFBImageProvider;
friend
     class IDirectFBVideoProvider;
friend
     class IDirectFBWindow;

public:
     DFBSurfaceCapabilities GetCapabilities     ();
     void                   GetSize             (int                      *width,
                                                 int                      *height);
     void                   GetVisibleRectangle (DFBRectangle             *rect);
     DFBSurfacePixelFormat  GetPixelFormat      ();
     DFBAccelerationMask    GetAccelerationMask (IDirectFBSurface         *source = NULL);

     IDirectFBPalette       GetPalette          ();
     void                   SetPalette          (IDirectFBPalette         *palette);
     void                   SetAlphaRamp        (__u8                      a0,
                                                 __u8                      a1,
                                                 __u8                      a2,
                                                 __u8                      a3);

     void                   Lock                (DFBSurfaceLockFlags       flags,
                                                 void                    **ptr,
                                                 int                      *pitch);
     void                   Unlock              ();
     void                   Flip                (DFBRegion                *region = NULL,
                                                 DFBSurfaceFlipFlags       flags = (DFBSurfaceFlipFlags)0);
     void                   SetField            (int                       field);
     void                   Clear               (__u8                      r = 0x00,
                                                 __u8                      g = 0x00,
                                                 __u8                      b = 0x00,
                                                 __u8                      a = 0x00);

     void                   SetClip             (DFBRegion                *clip);
     void                   SetColor            (__u8                      r,
                                                 __u8                      g,
                                                 __u8                      b,
                                                 __u8                      a = 0xFF);
     void                   SetColorIndex       (unsigned int              index);
     void                   SetSrcBlendFunction (DFBSurfaceBlendFunction   function);
     void                   SetDstBlendFunction (DFBSurfaceBlendFunction   function);
     void                   SetPorterDuff       (DFBSurfacePorterDuffRule  rule);
     void                   SetSrcColorKey      (__u8                      r,
                                                 __u8                      g,
                                                 __u8                      b);
     void                   SetSrcColorKeyIndex (unsigned int              index);
     void                   SetDstColorKey      (__u8                      r,
                                                 __u8                      g,
                                                 __u8                      b);
     void                   SetDstColorKeyIndex (unsigned int              index);

     void                   SetBlittingFlags    (DFBSurfaceBlittingFlags   flags);
     void                   Blit                (IDirectFBSurface         *source,
                                                 DFBRectangle             *source_rect = NULL,
                                                 int                       x = 0,
                                                 int                       y = 0);
     void                   TileBlit            (IDirectFBSurface         *source,
                                                 DFBRectangle             *source_rect = NULL,
                                                 int                       x = 0,
                                                 int                       y = 0);
     void                   StretchBlit         (IDirectFBSurface         *source,
                                                 DFBRectangle             *source_rect = NULL,
                                                 DFBRectangle             *destination_rect = NULL);

     void                   TextureTriangles    (IDirectFBSurface         *source,
                                                 const DFBVertex          *vertices,
                                                 const int                *indices,
                                                 int                       num,
                                                 DFBTriangleFormation      formation);

     void                   SetDrawingFlags     (DFBSurfaceDrawingFlags    flags);
     void                   FillRectangle       (int                       x,
                                                 int                       y,
                                                 int                       width,
                                                 int                       height);
     void                   DrawRectangle       (int                       x,
                                                 int                       y,
                                                 int                       width,
                                                 int                       height);
     void                   DrawLine            (int                       x1,
                                                 int                       y1,
                                                 int                       x2,
                                                 int                       y2);
     void                   DrawLines           (DFBRegion                *lines,
                                                 unsigned int              num_lines);
     void                   FillTriangle        (int                       x1,
                                                 int                       y1,
                                                 int                       x2,
                                                 int                       y2,
                                                 int                       x3,
                                                 int                       y3);

     void                   SetFont             (IDirectFBFont            *font);
     IDirectFBFont          GetFont             ();
     void                   DrawString          (const char               *text,
                                                 int                       bytes,
                                                 int                       x,
                                                 int                       y,
                                                 DFBSurfaceTextFlags       flags);
     void                   DrawGlyph           (unsigned int              index,
                                                 int                       x,
                                                 int                       y,
                                                 DFBSurfaceTextFlags       flags);

     IDirectFBSurface       GetSubSurface       (DFBRectangle             *rect);

     void                   Dump                (const char *directory, const char *prefix);

     IDirectFBGL           *GetGL               ();

     /* Additional methods added for enhanced usability */

     int                    GetWidth            ();
     int                    GetHeight           ();

     void                   SetColor            (DFBColor                 &color);

     void                   FillRectangle       (const DFBRectangle       &rect);
     void                   DrawRectangle       (const DFBRectangle       &rect);
     void                   DrawLine            (const DFBRegion          &line);

     IDirectFBSurface       GetSubSurface       (int                       x,
                                                 int                       y,
                                                 int                       width,
                                                 int                       height);


     __DFB_PLUS_PLUS__INTERFACE_CLASS( IDirectFBSurface );
};

#endif
