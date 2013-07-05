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

#include <config.h>

#include <sawman.h>


#include <unistd.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/messages.h>


#define CHECK(x)                                  \
     do {                                         \
          DFBResult ret = (x);                    \
          if (ret != DFB_OK) {                    \
               DirectFBError(#x,ret);             \
               goto out;                          \
          }                                       \
     } while (0)



int
main( int argc, char** argv )
{
     IDirectFB *dfb = NULL; 
     ISaWMan   *saw = NULL; 
     pid_t      pid;

     D_INFO( "SaWMan/TestRun: Initializing...\n" );

     CHECK( DirectFBInit( &argc, &argv ) );

     CHECK( DirectFBCreate( &dfb ) );

     CHECK( SaWManCreate( &saw ) );

     CHECK( saw->Start( saw, ((argc > 1) && argv[1]) ? argv[1] : "Test Application", &pid ) );

     
     D_INFO( "SaWMan/TestRun: New process has pid %d.\n", pid );


//     sleep( 2 );
//     CHECK( saw->Stop( saw, pid ) );


out:
     D_INFO( "SaWMan/TestRun: Shutting down...\n" );

     if (saw)
          saw->Release( saw );

     if (dfb)
          dfb->Release( dfb );

     return 0;
}

