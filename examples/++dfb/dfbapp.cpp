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

#include "dfbapp.h"

DFBApp::DFBApp()
{
     m_width    = 0;
     m_height   = 0;
     m_flipping = true;
}

DFBApp::~DFBApp()
{
}

bool
DFBApp::Init( int argc, char *argv[] )
{
     DFBSurfaceDescription dsc;

     /* parse application arguments */
     if (!ParseArgs( argc, argv ))
          return false;

     /* create super interface */
     m_dfb = DirectFB::Create();

     try {
          /* try fullscreen mode */
          m_dfb.SetCooperativeLevel( DFSCL_FULLSCREEN );
     }
     catch (DFBException *ex) {
          if (ex->GetResultCode() == DFB_ACCESSDENIED)
               std::cerr << "Warning: " << ex << std::endl;
          else
               throw;
     }

     /* fill surface description */
     dsc.flags = DSDESC_CAPS;
     dsc.caps  = DSCAPS_PRIMARY;

     if (m_flipping)
          DFB_ADD_SURFACE_CAPS( dsc.caps, DSCAPS_DOUBLE );

     if (m_width) {
          DFB_ADD_SURFACE_DESC( dsc.flags, DSDESC_WIDTH );
          dsc.width  = m_width;
     }

     if (m_height) {
          DFB_ADD_SURFACE_DESC( dsc.flags, DSDESC_HEIGHT );
          dsc.height = m_height;
     }

     /* create the primary surface */
     m_primary = m_dfb.CreateSurface( dsc );

     /* create an event buffer with all devices attached */
     m_events = m_dfb.CreateInputEventBuffer( DICAPS_ALL );


     /* get the screen resolution */
     int width;
     int height;

     m_primary.GetSize( &width, &height );

     /* call setup method */
     return Setup( width, height );
}

void
DFBApp::Run()
{
     while (true) {
          DFBInputEvent event;

          /* render to the screen */
          Render( m_primary );

          /* flip the screen */
          if (m_flipping)
               m_primary.Flip();

          /* wait for an event if Update() returns true */
          if (Update())
               m_events.WaitForEvent();

          /* handle all events, exit if HandleEvent() returns true */
          while (m_events.GetEvent( DFB_EVENT(&event) ))
               if (HandleEvent( event ))
                    return;
     }
}

void
DFBApp::SetMode( int width, int height )
{
     m_width  = width;
     m_height = height;
}

void
DFBApp::SetFlipping( bool flipping )
{
     m_flipping = flipping;
}

bool
DFBApp::ParseArgs( int argc, char *argv[] )
{
     return true;
}

bool
DFBApp::Setup( int width, int height )
{
     return true;
}

void
DFBApp::Render( IDirectFBSurface &surface )
{
     surface.Clear();
}

bool
DFBApp::Update()
{
     return true;
}

bool
DFBApp::HandleEvent( DFBInputEvent &event )
{
     switch (event.type) {
          case DIET_KEYPRESS:
               if (event.key_symbol == DIKS_ESCAPE)
                    return true;
               break;

          default:
               break;
     }

     return false;
}

