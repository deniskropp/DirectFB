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

#include <stdio.h>
#include <unistd.h>

#include <direct/clock.h>
#include <direct/messages.h>

#include <fusiondale.h>


#define CHECK(x)                                  \
     do {                                         \
          DFBResult ret = (x);                    \
          if (ret && ret != DFB_BUSY)             \
               FusionDaleErrorFatal(#x,ret);      \
     } while (0)


static int first;
static int first_delay;
static int last;
static int count;

static void
EventCallback( FDMessengerEventID  event_id,
               int                 param,
               void               *data,
               int                 data_size,
               void               *context )
{
//     D_INFO( "EventCallback( %lu, %d, %p, %d, %p )\n",
  //           event_id, param, data, data_size, context );

     if (!first) {
          first = param;
          first_delay = direct_clock_get_millis() - first;
     }

     last = param;

     count++;
}

int
main( int argc, char *argv[] )
{
     IFusionDale           *dale;
     IFusionDaleMessenger  *messenger;
     FDMessengerEventID     event_id;
     FDMessengerListenerID  listener_id;

     CHECK( FusionDaleInit( &argc, &argv ) );

     CHECK( FusionDaleCreate( &dale ) );

     CHECK( dale->CreateMessenger( dale, &messenger ) );

     CHECK( messenger->RegisterEvent( messenger, "T2 Event", &event_id ) );


     CHECK( messenger->RegisterListener( messenger, event_id, EventCallback, NULL, &listener_id ) );

     printf( "\n" );

     sleep( 10 );

     printf( "%d ms between first and last of %d messages (%d.%03dk/sec)\n"
             "%d ms latency (1st message)\n", last - first, count,
             count / (last - first), count * 1000 / (last - first) % 1000, first_delay );

     CHECK( messenger->UnregisterEvent( messenger, event_id ) );


     messenger->Release( messenger );
     dale->Release( dale );

     return 0;
}
