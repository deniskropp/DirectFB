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

#include "++dfb.h"
#include "++dfb_internal.h"

DFBSurfaceCapabilities IDirectFBSurface::GetCapabilities()
{
     DFBSurfaceCapabilities caps;

     DFBCHECK( iface->GetCapabilities (iface, &caps) );

     return caps;
}

void IDirectFBSurface::GetSize (int *width,
                                int *height)
{
     DFBCHECK( iface->GetSize (iface, width, height) );
}

void IDirectFBSurface::GetVisibleRectangle (DFBRectangle *rect)
{
     DFBCHECK( iface->GetVisibleRectangle (iface, rect) );
}

DFBSurfacePixelFormat IDirectFBSurface::GetPixelFormat()
{
     DFBSurfacePixelFormat format;

     DFBCHECK( iface->GetPixelFormat (iface, &format) );

     return format;
}

DFBAccelerationMask IDirectFBSurface::GetAccelerationMask (IDirectFBSurface *source)
{
     DFBAccelerationMask mask;

     DFBCHECK( iface->GetAccelerationMask (iface, source->get_iface(), &mask) );

     return mask;
}

IDirectFBPalette IDirectFBSurface::GetPalette()
{
     IDirectFBPalette_C *idirectfbpalette;

     DFBCHECK( iface->GetPalette (iface, &idirectfbpalette) );

     return IDirectFBPalette (idirectfbpalette);
}

void IDirectFBSurface::SetPalette (IDirectFBPalette *palette)
{
     DFBCHECK( iface->SetPalette (iface, palette->get_iface()) );
}

void IDirectFBSurface::SetAlphaRamp (__u8 a0, __u8 a1, __u8 a2, __u8 a3)
{
     DFBCHECK( iface->SetAlphaRamp (iface, a0, a1, a2, a3) );
}

void IDirectFBSurface::Lock (DFBSurfaceLockFlags   flags,
                             void                **ptr,
                             int                  *pitch)
{
     DFBCHECK( iface->Lock (iface, flags, ptr, pitch) );
}

void IDirectFBSurface::Unlock()
{
     DFBCHECK( iface->Unlock (iface) );
}

void IDirectFBSurface::Flip (DFBRegion           *region,
                             DFBSurfaceFlipFlags  flags)
{
     DFBCHECK( iface->Flip (iface, region, flags) );
}

void IDirectFBSurface::SetField (int field)
{
     DFBCHECK( iface->SetField (iface, field) );
}

void IDirectFBSurface::Clear (__u8 r, __u8 g, __u8 b, __u8 a)
{
     DFBCHECK( iface->Clear (iface, r, g, b, a) );
}

void IDirectFBSurface::SetClip (DFBRegion *clip)
{
     DFBCHECK( iface->SetClip (iface, clip) );
}

void IDirectFBSurface::SetColor (__u8 r, __u8 g, __u8 b, __u8 a)
{
     DFBCHECK( iface->SetColor (iface, r, g, b, a) );
}

void IDirectFBSurface::SetColorIndex (unsigned int index)
{
     DFBCHECK( iface->SetColorIndex (iface, index) );
}

void IDirectFBSurface::SetSrcBlendFunction (DFBSurfaceBlendFunction function)
{
     DFBCHECK( iface->SetSrcBlendFunction (iface, function) );
}

void IDirectFBSurface::SetDstBlendFunction (DFBSurfaceBlendFunction function)
{
     DFBCHECK( iface->SetDstBlendFunction (iface, function) );
}

void IDirectFBSurface::SetPorterDuff (DFBSurfacePorterDuffRule rule)
{
     DFBCHECK( iface->SetPorterDuff (iface, rule) );
}

void IDirectFBSurface::SetSrcColorKey (__u8 r, __u8 g, __u8 b)
{
     DFBCHECK( iface->SetSrcColorKey (iface, r, g, b) );
}

void IDirectFBSurface::SetSrcColorKeyIndex (unsigned int index)
{
     DFBCHECK( iface->SetSrcColorKeyIndex (iface, index) );
}

void IDirectFBSurface::SetDstColorKey (__u8 r, __u8 g, __u8 b)
{
     DFBCHECK( iface->SetDstColorKey (iface, r, g, b) );
}

void IDirectFBSurface::SetDstColorKeyIndex (unsigned int index)
{
     DFBCHECK( iface->SetDstColorKeyIndex (iface, index) );
}

void IDirectFBSurface::SetBlittingFlags (DFBSurfaceBlittingFlags flags)
{
     DFBCHECK( iface->SetBlittingFlags (iface, flags) );
}

void IDirectFBSurface::Blit (IDirectFBSurface *source,
                             DFBRectangle     *source_rect,
                             int               x,
                             int               y)
{
     DFBCHECK( iface->Blit (iface, source->get_iface(), source_rect, x, y) );
}

void IDirectFBSurface::TileBlit (IDirectFBSurface *source,
                                 DFBRectangle     *source_rect,
                                 int               x,
                                 int               y)
{
     DFBCHECK( iface->TileBlit (iface, source->get_iface(), source_rect, x, y) );
}

void IDirectFBSurface::StretchBlit (IDirectFBSurface *source,
                                    DFBRectangle     *source_rect,
                                    DFBRectangle     *destination_rect)
{
     DFBCHECK( iface->StretchBlit (iface, source->get_iface(),
                                   source_rect, destination_rect) );
}

void IDirectFBSurface::TextureTriangles (IDirectFBSurface     *source,
                                         const DFBVertex      *vertices,
                                         const int            *indices,
                                         int                   num,
                                         DFBTriangleFormation  formation)
{
     DFBCHECK( iface->TextureTriangles (iface, source->get_iface(),
                                        vertices, indices, num, formation) );
}

void IDirectFBSurface::SetDrawingFlags (DFBSurfaceDrawingFlags flags)
{
     DFBCHECK( iface->SetDrawingFlags (iface, flags) );
}

void IDirectFBSurface::FillRectangle (int x, int y, int width, int height)
{
     DFBCHECK( iface->FillRectangle (iface, x, y, width, height) );
}

void IDirectFBSurface::DrawRectangle (int x, int y, int width, int height)
{
     DFBCHECK( iface->DrawRectangle (iface, x, y, width, height) );
}

void IDirectFBSurface::DrawLine (int x1, int y1, int x2, int y2)
{
     DFBCHECK( iface->DrawLine (iface, x1, y1, x2, y2) );
}

void IDirectFBSurface::DrawLines (const DFBRegion *lines, unsigned int num_lines)
{
     DFBCHECK( iface->DrawLines (iface, lines, num_lines) );
}

void IDirectFBSurface::FillRectangles (const DFBRectangle *rects, unsigned int num_rects)
{
     DFBCHECK( iface->FillRectangles (iface, rects, num_rects) );
}

void IDirectFBSurface::FillTriangle (int x1, int y1,
                                     int x2, int y2,
                                     int x3, int y3)
{
     DFBCHECK( iface->FillTriangle (iface, x1, y1, x2, y2, x3, y3) );
}

void IDirectFBSurface::SetFont (IDirectFBFont *font)
{
     DFBCHECK( iface->SetFont (iface, font->get_iface()) );
}

IDirectFBFont IDirectFBSurface::GetFont()
{
     IDirectFBFont_C *idirectfbfont;

     DFBCHECK( iface->GetFont (iface, &idirectfbfont) );

     return IDirectFBFont (idirectfbfont);
}

void IDirectFBSurface::DrawString (const char          *text,
                                   int                  bytes,
                                   int                  x,
                                   int                  y,
                                   DFBSurfaceTextFlags  flags)
{
     DFBCHECK( iface->DrawString (iface, text, bytes, x, y, flags) );
}

void IDirectFBSurface::DrawGlyph (unsigned int        index,
                                  int                 x,
                                  int                 y,
                                  DFBSurfaceTextFlags flags)
{
     DFBCHECK( iface->DrawGlyph (iface, index, x, y, flags) );
}

IDirectFBSurface IDirectFBSurface::GetSubSurface (DFBRectangle *rect)
{
     IDirectFBSurface_C *idirectfbsurface;

     DFBCHECK( iface->GetSubSurface (iface, rect, &idirectfbsurface) );

     return IDirectFBSurface (idirectfbsurface);
}

int IDirectFBSurface::GetWidth()
{
     int width;

     GetSize (&width, NULL);

     return width;
}

int IDirectFBSurface::GetHeight()
{
     int height;

     GetSize (NULL, &height);

     return height;
}

void IDirectFBSurface::SetColor (DFBColor &color)
{
     DFBCHECK( iface->SetColor (iface, color.r, color.g, color.b, color.a) );
}

void IDirectFBSurface::FillRectangle (const DFBRectangle &rect)
{
     DFBCHECK( iface->FillRectangle (iface, rect.x, rect.y, rect.w, rect.h) );
}

void IDirectFBSurface::DrawRectangle (const DFBRectangle &rect)
{
     DFBCHECK( iface->DrawRectangle (iface, rect.x, rect.y, rect.w, rect.h) );
}

void IDirectFBSurface::DrawLine (const DFBRegion &line)
{
     DFBCHECK( iface->DrawLine (iface, line.x1, line.y1, line.x2, line.y2) );
}

IDirectFBSurface IDirectFBSurface::GetSubSurface (int x, int y,
                                                   int width, int height)
{
     DFBRectangle rect = { x, y, width, height };

     IDirectFBSurface_C *idirectfbsurface;

     DFBCHECK( iface->GetSubSurface (iface, &rect, &idirectfbsurface) );

     return IDirectFBSurface (idirectfbsurface);
}

void IDirectFBSurface::Dump (const char *directory,
                              const char *prefix)
{
     DFBCHECK( iface->Dump (iface, directory, prefix) );
}

void IDirectFBSurface::DisableAcceleration (DFBAccelerationMask mask)
{
     DFBCHECK( iface->DisableAcceleration (iface, mask) );
}

IDirectFBGL *IDirectFBSurface::GetGL()
{
     IDirectFBGL *idirectfbgl;

     DFBCHECK( iface->GetGL (iface, &idirectfbgl) );

     return idirectfbgl;
}

