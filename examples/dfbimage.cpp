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

