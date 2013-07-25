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

public:
     /* DirectFB interfaces */
     IDirectFB            m_dfb;
     IDirectFBSurface     m_primary;
     IDirectFBEventBuffer m_events;
};

