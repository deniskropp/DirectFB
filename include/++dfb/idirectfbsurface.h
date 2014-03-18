/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


#ifndef IDIRECTFBSURFACE_H
#define IDIRECTFBSURFACE_H

#ifndef DFBPP_H
#error Please include ++dfb.h only.
#endif

class IDirectFBSurface : public IPPAny<IDirectFBSurface, IDirectFBSurface_C> {
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
     PPDFB_API IDirectFBSurface(IDirectFBSurface_C* myptr=NULL):IPPAny<IDirectFBSurface, IDirectFBSurface_C>(myptr){}

     DFBSurfaceCapabilities PPDFB_API GetCapabilities     ();
     void                   PPDFB_API GetPosition         (int                      *x,
                                                           int                      *y);
     void                   PPDFB_API GetSize             (int                      *width,
                                                           int                      *height);
     void                   PPDFB_API GetVisibleRectangle (DFBRectangle             *rect);
     DFBSurfacePixelFormat  PPDFB_API GetPixelFormat      ();
     DFBAccelerationMask    PPDFB_API GetAccelerationMask (IDirectFBSurface         *source = NULL);

     IDirectFBPalette       PPDFB_API GetPalette          ();
     void                   PPDFB_API SetPalette          (IDirectFBPalette         *palette);
     void                   PPDFB_API SetAlphaRamp        (u8                      a0,
                                                           u8                      a1,
                                                           u8                      a2,
                                                           u8                      a3);

     void                   PPDFB_API Lock                (DFBSurfaceLockFlags       flags,
                                                           void                    **ptr,
                                                           int                      *pitch);
     void                   PPDFB_API Unlock              ();
     void                   PPDFB_API Flip                (DFBRegion                *region = NULL,
                                                           DFBSurfaceFlipFlags       flags = static_cast<DFBSurfaceFlipFlags>(0));
     void                   PPDFB_API SetField            (int                       field);
     void                   PPDFB_API Clear               (u8                      r = 0x00,
                                                           u8                      g = 0x00,
                                                           u8                      b = 0x00,
                                                           u8                      a = 0x00);
     void                   PPDFB_API Clear               (DFBColor               &color);

     void                   PPDFB_API SetClip             (const DFBRegion          *clip = 0);
     void                   PPDFB_API SetClip             (const DFBRectangle       *clip);
     void                   PPDFB_API SetColor            (u8                      r,
                                                           u8                      g,
                                                           u8                      b,
                                                           u8                      a = 0xFF);
     void                   PPDFB_API SetColor            (DFBColor               &color);
     void                   PPDFB_API SetColorIndex       (unsigned int              index);
     void                   PPDFB_API SetSrcBlendFunction (DFBSurfaceBlendFunction   function);
     void                   PPDFB_API SetDstBlendFunction (DFBSurfaceBlendFunction   function);
     void                   PPDFB_API SetPorterDuff       (DFBSurfacePorterDuffRule  rule);
     void                   PPDFB_API SetSrcColorKey      (u8                      r,
                                                           u8                      g,
                                                           u8                      b);
     void                   PPDFB_API SetSrcColorKeyIndex (unsigned int              index);
     void                   PPDFB_API SetDstColorKey      (u8                      r,
                                                           u8                      g,
                                                           u8                      b);
     void                   PPDFB_API SetDstColorKeyIndex (unsigned int              index);

     void                   PPDFB_API SetBlittingFlags    (DFBSurfaceBlittingFlags   flags);
     void                   PPDFB_API Blit                (IDirectFBSurface         *source,
                                                           const DFBRectangle       *source_rect = NULL,
                                                           int                       x = 0,
                                                           int                       y = 0);
     void                   PPDFB_API TileBlit            (IDirectFBSurface         *source,
                                                           const DFBRectangle       *source_rect = NULL,
                                                           int                       x = 0,
                                                           int                       y = 0);
     void                   PPDFB_API BatchBlit           (IDirectFBSurface         *source,
                                                           const DFBRectangle       *source_rects,
                                                           const DFBPoint           *dest_points,
                                                           int                       num);
     void                   PPDFB_API StretchBlit         (IDirectFBSurface         *source,
                                                           const DFBRectangle       *source_rect = NULL,
                                                           const DFBRectangle       *destination_rect = NULL);

     void                   PPDFB_API TextureTriangles    (IDirectFBSurface         *source,
                                                           const DFBVertex          *vertices,
                                                           const int                *indices,
                                                           int                       num,
                                                           DFBTriangleFormation      formation);

     void                   PPDFB_API SetDrawingFlags     (DFBSurfaceDrawingFlags    flags);
     void                   PPDFB_API FillRectangle       (int                       x,
                                                           int                       y,
                                                           int                       width,
                                                           int                       height);
     void                   PPDFB_API FillRectangle       (DFBRectangle             &rect);
     void                   PPDFB_API FillRectangle       (DFBRegion                &rect);
     void                   PPDFB_API FillRectangles      (const DFBRectangle       *rects,
                                                           unsigned int              num_rects);
     void                   PPDFB_API DrawRectangle       (int                       x,
                                                           int                       y,
                                                           int                       width,
                                                           int                       height);
     void                   PPDFB_API DrawLine            (int                       x1,
                                                           int                       y1,
                                                           int                       x2,
                                                           int                       y2); 
     void                   PPDFB_API DrawLines           (const DFBRegion          *lines,
                                                           unsigned int              num_lines);
     void                   PPDFB_API FillTriangle        (int                       x1,
                                                           int                       y1,
                                                           int                       x2,
                                                           int                       y2,
                                                           int                       x3,
                                                           int                       y3);
     void                   PPDFB_API FillSpans           (int                       y,
                                                           const DFBSpan            *spans,
                                                           unsigned int              num);

     void                   PPDFB_API SetFont             (const IDirectFBFont &font) const;
     IDirectFBFont          PPDFB_API GetFont             () const;
     void                   PPDFB_API DrawString          (const char               *text,
                                                           int                       bytes,
                                                           int                       x,
                                                           int                       y,
                                                           DFBSurfaceTextFlags       flags);
     void                   PPDFB_API DrawGlyph           (unsigned int              index,
                                                           int                       x,
                                                           int                       y,
                                                           DFBSurfaceTextFlags       flags);
     void                   PPDFB_API SetEncoding         (DFBTextEncodingID         encoding);

     IDirectFBSurface       PPDFB_API GetSubSurface       (DFBRectangle             *rect);

     void                   PPDFB_API Dump                (const char               *directory,
                                                           const char               *prefix);

     void                   PPDFB_API DisableAcceleration (DFBAccelerationMask       mask);

     IDirectFBGL           PPDFB_API *GetGL               ();


     DFBSurfaceID           PPDFB_API GetID               ();
     void                   PPDFB_API AllowAccess         (const char               *executable);



     /* Additional methods added for enhanced usability */

     int                    PPDFB_API GetWidth            ();
     int                    PPDFB_API GetHeight           ();

     void                   PPDFB_API SetColor            (const DFBColor           &color);
     void                   PPDFB_API SetColor            (const DFBColor           *color);

     void                   PPDFB_API FillRectangle       (const DFBRectangle       &rect);
     void                   PPDFB_API DrawRectangle       (const DFBRectangle       &rect);
     void                   PPDFB_API DrawLine            (const DFBRegion          &line);

     IDirectFBSurface       PPDFB_API GetSubSurface       (int                       x,
                                                           int                       y,
                                                           int                       width,
                                                           int                       height);

     void                   PPDFB_API GetClip             (DFBRegion                *clip);

     int                    PPDFB_API GetFramebufferOffset();

     void                   PPDFB_API ReleaseSource       ();
     void                   PPDFB_API SetIndexTranslation (const int                *indices,
                                                           int                       num_indices);

     void                   PPDFB_API Read                (void                     *ptr,
                                                           int                       pitch,
                                                           const DFBRectangle       *rect = NULL);

     void                   PPDFB_API Write               (const void               *ptr,
                                                           int                       pitch,
                                                           const DFBRectangle       *rect = NULL);

     void                   PPDFB_API SetRenderOptions    (const DFBSurfaceRenderOptions &options);

     IDirectFBEventBuffer   PPDFB_API CreateEventBuffer   ();
     void                   PPDFB_API AttachEventBuffer   (IDirectFBEventBuffer     *buffer);
     void                   PPDFB_API DetachEventBuffer   (IDirectFBEventBuffer     *buffer);

     DFBDimension           PPDFB_API GetSize             ();

     inline IDirectFBSurface PPDFB_API & operator = (const IDirectFBSurface& other){
          return IPPAny<IDirectFBSurface, IDirectFBSurface_C>::operator =(other);
     }
     inline IDirectFBSurface PPDFB_API & operator = (IDirectFBSurface_C* other){
          return IPPAny<IDirectFBSurface, IDirectFBSurface_C>::operator =(other);
     }
};

#endif
