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

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <fusiondale.h>

static char *enter_coma = "FDTestComa";

/**********************************************************************************************************************/

static void
method_func( void         *ctx,
             ComaMethodID  method,
             void         *arg,
             unsigned int  magic )
{
     IComaComponent **component = ctx;

     (*component)->Return( (*component), 0, magic );
}

/**********************************************************************************************************************/

#define NUM_ITEMS   2000000

int
main( int argc, char *argv[] )
{
     DirectResult    ret;
     IFusionDale    *dale = NULL;
     IComa          *coma = NULL;
     IComaComponent *component = NULL;

     /* Initialize FusionDale including command line parsing. */
     ret = FusionDaleInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: FusionDaleInit() failed!\n" );
          goto out;
     }

     /* Create the super interface. */
     ret = FusionDaleCreate( &dale );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: FusionDaleCreate() failed!\n" );
          goto out;
     }

     ret = dale->EnterComa( dale, enter_coma, &coma );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: IFusionDale::EnterComa( '%s' ) failed!\n", enter_coma );
          goto out;
     }

     if (argc > 1) {
          coma->GetComponent( coma, "Test", 5000, &component );
          
                    
          void *mem;

          coma->GetLocal( coma, 99, &mem );



          DirectClock clock;
          int         counter = 0;
          int         retval;

          direct_clock_start( &clock );

          do {
               component->Call( component, 1, mem, &retval );

               counter++;
          } while (counter < NUM_ITEMS);

          direct_clock_stop( &clock );


          D_INFO( "Voodoo/Test: Stopped after %lld.%03lld seconds... (%lld items/sec)\n",
                  DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );



          coma->FreeLocal( coma );
     }
     else {
          coma->CreateComponent( coma, "Test", method_func, 0, &component, &component );

          component->Activate( component );

          pause();
     }


out:
     /* Release the component manager. */
     if (coma)
          coma->Release( coma );

     /* Release the super interface. */
     if (dale)
          dale->Release( dale );

     return ret;
}

