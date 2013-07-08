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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <misc/conf.h>

#include <GLES2/gl2.h>

#include <directfb.h>
#include <directfbgl2.h>


typedef struct {
     IDirectFB             *dfb;
     IDirectFBSurface      *primary;
     IDirectFBGL2          *gl2;
     IDirectFBGL2Context   *gl2context;
     IDirectFBEventBuffer  *events;
     DFBDimension           size;
} Test;


static DFBResult
Initialize( Test   *test,
            int    *argc,
            char ***argv )
{
     DFBResult             ret;
     DFBSurfaceDescription dsc;

     /* 
      * Initialize DirectFB options
      */ 
     ret = DirectFBInit( argc, argv );
     if (ret) {
          D_DERROR( ret, "DirectFBInit() failed!\n" );
          return ret;
     }

     /* 
      * Create the super interface
      */
     ret = DirectFBCreate( &test->dfb );
     if (ret) {
          D_DERROR( ret, "DirectFBCreate() failed!\n" );
          return ret;
     }

     /*
      * Retrieve the DirectFBGL2 API
      */
     ret = test->dfb->GetInterface( test->dfb, "IDirectFBGL2", NULL, test->dfb, (void**) &test->gl2 );
     if (ret) {
          D_DERROR( ret, "IDirectFB::GetInterface( 'IDirectFBGL2' ) failed!\n" );
          return ret;
     }

     /* 
      * Create an event buffer for all devices with these caps
      */
     ret = test->dfb->CreateInputEventBuffer( test->dfb, DICAPS_KEYS | DICAPS_AXES, DFB_FALSE, &test->events );
     if (ret) {
          D_DERROR( ret, "IDirectFB::CreateInputEventBuffer( DICAPS_KEYS | DICAPS_AXES ) failed!\n" );
          return ret;
     }

     /* 
      * Try to set our cooperative level to DFSCL_FULLSCREEN for exclusive access to the primary layer
      */
     test->dfb->SetCooperativeLevel( test->dfb, DFSCL_FULLSCREEN );

     /* 
      * Create the primary surface
      */
     dsc.flags = DSDESC_CAPS;
     dsc.caps  = DSCAPS_PRIMARY | DSCAPS_TRIPLE;

     ret = test->dfb->CreateSurface( test->dfb, &dsc, &test->primary );
     if (ret) {
          D_DERROR( ret, "IDirectFB::CreateSurface( DSCAPS_PRIMARY | DSCAPS_TRIPLE ) failed!\n" );
          return ret;
     }

     /* 
      * Get the size of the surface, clear and show it
      */
     test->primary->GetSize( test->primary, &test->size.w, &test->size.h );

     test->primary->Clear( test->primary, 0, 0, 0, 0 );
     test->primary->Flip( test->primary, NULL, 0 );


     /* 
      * Create an OpenGL rendering context
      */
     ret = test->gl2->CreateContext( test->gl2, NULL, &test->gl2context );
     if (ret) {
          D_DERROR( ret, "IDirectFBGL2::CreateContext() failed!\n" );
          return ret;
     }

     return DFB_OK;
}

static void
Shutdown( Test *test )
{
     if (test->gl2context)
          test->gl2context->Release( test->gl2context );

     if (test->gl2)
          test->gl2->Release( test->gl2 );

     if (test->primary)
          test->primary->Release( test->primary );

     if (test->events)
          test->events->Release( test->events );

     if (test->dfb)
          test->dfb->Release( test->dfb );
}

static DFBResult
InitGL( Test *test )
{
     DFBResult ret;

     /* 
      * Bind the OpenGL rendering context to our primary surface
      */
     ret = test->gl2context->Bind( test->gl2context, test->primary, test->primary );
     if (ret) {
          D_DERROR( ret, "IDirectFBGL2Context::Bind() failed!\n" );
          return ret;
     }

     /* Set the clear color */
     glClearColor( 1.0, 1.0, 1.0, 1.0 );

     /* Setup the viewport */
     glViewport( 0, 0, (GLint) test->size.w, (GLint) test->size.h );

     /* Unbind the OpenGL rendering context */
     test->gl2context->Unbind( test->gl2context );

     return DFB_OK;
}

static DFBResult
RenderGL( Test *test )
{
     DFBResult ret;

     /* 
      * Bind the OpenGL rendering context to our primary surface
      */
     ret = test->gl2context->Bind( test->gl2context, test->primary, test->primary );
     if (ret) {
          D_DERROR( ret, "IDirectFBGL2Context::Bind() failed!\n" );
          return ret;
     }

     /* Clear the buffer */
     glClear( GL_COLOR_BUFFER_BIT );

     /* Unbind the OpenGL rendering context */
     test->gl2context->Unbind( test->gl2context );

     return DFB_OK;
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     bool      quit = false;
     Test      test;

     memset( &test, 0, sizeof(test) );


     ret = Initialize( &test, &argc, &argv );
     if (ret)
          goto error;

     ret = InitGL( &test );
     if (ret)
          goto error;

     /*
      * Main Loop
      */
     while (!quit) {
          DFBInputEvent evt;

          ret = RenderGL( &test );
          if (ret)
               goto error;

          /*
           * Show the rendered buffer
           */
          test.primary->Flip( test.primary, NULL, DSFLIP_ONSYNC );

          /*
           * Process events
           */
          while (test.events->GetEvent( test.events, DFB_EVENT(&evt) ) == DFB_OK) {
               switch (evt.type) {
                    case DIET_KEYPRESS:
                         switch (evt.key_symbol) {
                              case DIKS_ESCAPE:
                                   quit = true;
                                   break;
                              default:
                                   ;
                         }
                         break;
                    default:
                         ;
               }
          }
     }


error:
     Shutdown( &test );

     return ret;
}

