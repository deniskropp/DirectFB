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

#include <unistd.h>

#include <direct/messages.h>

#include <fusiondale.h>

static void
EventCallback( FDMessengerEventID  event_id,
               int                 param,
               void               *data,
               int                 data_size,
               void               *context )
{
     D_INFO( "EventCallback( %lu, %d, %p, %d, %p )\n",
             event_id, param, data, data_size, context );
}

int
main( int argc, char *argv[] )
{
     DirectResult           ret;
     IFusionDale           *dale;
     IFusionDaleMessenger  *messenger;
     FDMessengerEventID     event_id1;
     FDMessengerEventID     event_id2;
     FDMessengerListenerID  listener_id1;
     FDMessengerListenerID  listener_id2;

     ret = FusionDaleInit( &argc, &argv );
     if (ret)
          FusionDaleErrorFatal( "FusionDaleInit", ret );

     ret = FusionDaleCreate( &dale );
     if (ret)
          FusionDaleErrorFatal( "FusionDaleCreate", ret );

     ret = dale->CreateMessenger( dale, &messenger );
     if (ret)
          FusionDaleErrorFatal( "IFusionDale::CreateMessenger", ret );



     ret = messenger->RegisterEvent( messenger, "Test Event", &event_id1 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::RegisterEvent", ret );

     ret = messenger->IsEventRegistered( messenger, "Test Event" );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::IsEventRegistered", ret );


     ret = messenger->RegisterEvent( messenger, "Test Event2", &event_id2 );
     if (ret)
          FusionDaleError( "IFusionDaleMessenger::RegisterEvent", ret );


     ret = messenger->RegisterListener( messenger, event_id1, EventCallback, NULL, &listener_id1 );
     if (ret)
          FusionDaleError( "IFusionDaleMessenger::RegisterListener", ret );


     ret = messenger->RegisterListener( messenger, event_id2, EventCallback, NULL, &listener_id2 );
     if (ret)
          FusionDaleError( "IFusionDaleMessenger::RegisterListener", ret );


     ret = messenger->SendSimpleEvent( messenger, event_id1, 23 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::UnregisterEvent", ret );

//     sleep(1);

     ret = messenger->UnregisterListener( messenger, listener_id1 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::UnregisterEvent", ret );


     ret = messenger->SendSimpleEvent( messenger, event_id1, 23 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::UnregisterEvent", ret );

//     sleep(1);

     ret = messenger->UnregisterEvent( messenger, event_id1 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::UnregisterEvent", ret );


     sleep(2);


     ret = messenger->UnregisterEvent( messenger, event_id2 );
     if (ret)
          FusionDaleErrorFatal( "IFusionDaleMessenger::UnregisterEvent", ret );


     messenger->Release( messenger );
     dale->Release( dale );

     return 0;
}
