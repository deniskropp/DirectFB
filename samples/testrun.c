/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <core/windows_internal.h>

#include <sawman_manager.h>


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

