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

#include <++dfb.h>

class DFBApp {
public:
     DFBApp();
     virtual ~DFBApp();

     bool Init( int argc, char *argv[] );
     void Run();

protected:
     void SetMode( int width, int height );
     void SetFlipping( bool flipping );

private:
     /* called before initialization */
     virtual bool ParseArgs( int argc, char *argv[] );

     /* called after initialization */
     virtual bool Setup( int width, int height );

     /* render callback */
     virtual void Render( IDirectFBSurface &surface );

     /* update application state, return true to block until events arrive */
     virtual bool Update();

     /* event callback, return true to exit application */
     virtual bool HandleEvent( DFBInputEvent &event );

     /* application setup information */
     int                  m_width;
     int                  m_height;
     bool                 m_flipping;

     /* DirectFB interfaces */
     IDirectFB            m_dfb;
     IDirectFBSurface     m_primary;
     IDirectFBEventBuffer m_events;
};

