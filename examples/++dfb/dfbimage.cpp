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

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include <iostream>

#include "dfbimage.h"

DFBImage::DFBImage()
{
     m_filename = "";
}

DFBImage::DFBImage( std::string filename )
{
     m_filename = "";

     LoadImage( filename );
}

DFBImage::~DFBImage()
{
     DisposeImage();
}

int
DFBImage::GetWidth()
{
     int width = 0;

     if (m_surface)
          m_surface.GetSize( &width, NULL );

     return width;
}

int
DFBImage::GetHeight()
{
     int height = 0;

     if (m_surface)
          m_surface.GetSize( NULL, &height );

     return height;
}

void
DFBImage::LoadImage( std::string  filename,
                     unsigned int width,
                     unsigned int height )
{
     IDirectFB              dfb;
     IDirectFBImageProvider provider;
     IDirectFBSurface       surface;

     DisposeImage();

     DFBSurfaceDescription desc;

     dfb = DirectFB::Create();

     provider = dfb.CreateImageProvider( filename.data() );

     provider.GetImageDescription( &m_desc );

     provider.GetSurfaceDescription( &desc );

     if (width)
          desc.width = width;

     if (height)
          desc.height = height;

     surface = dfb.CreateSurface( desc );

     provider.RenderTo( surface, NULL );

     m_flags = DSBLIT_NOFX;

     if (m_desc.caps & DICAPS_ALPHACHANNEL)
          DFB_ADD_BLITTING_FLAG( m_flags, DSBLIT_BLEND_ALPHACHANNEL );

     if (m_desc.caps & DICAPS_COLORKEY) {
          DFB_ADD_BLITTING_FLAG( m_flags, DSBLIT_SRC_COLORKEY );

          surface.SetSrcColorKey( m_desc.colorkey_r,
                                  m_desc.colorkey_g,
                                  m_desc.colorkey_b );
     }

     m_filename = filename;
     m_surface  = surface;
}

void
DFBImage::ReloadImage( int width, int height )
{
     if (m_surface) {
          int w, h;

          m_surface.GetSize( &w, &h );

          if ((!width || width == w) && (!height || height == h))
               return;
     }

     LoadImage( m_filename, width, height );
}

void
DFBImage::PrepareTarget( IDirectFBSurface &target )
{
     target.SetBlittingFlags( m_flags );
}

void
DFBImage::DisposeImage()
{
     m_surface = NULL;
}

