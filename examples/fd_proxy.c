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

#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/conf.h>
#include <voodoo/play.h>
#include <voodoo/server.h>

#include <fusiondale.h>


static DirectResult server_run();

/**********************************************************************************************************************/

typedef struct {
     IFusionDale    *dale;
     IComa          *coma;
     IComaComponent *component;
} Test;

static void
Test_MethodFunc( void               *ctx,
                 ComaMethodID        method,
                 void               *arg,
                 unsigned int        magic )
{
     Test           *test      = ctx;
     IComaComponent *component = test->component;

     D_INFO( "%s( %lu, %p, %u\n", __func__, method, arg, magic );

     component->Return( component, 0, magic );
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult ret;
     Test         test;


     ret = FusionDaleInit( &argc, &argv );
     if (ret)
          FusionDaleErrorFatal( "FusionDaleInit", ret );


     pid_t pid;

     pid = fork();
     switch (pid) {
          case -1:
               D_PERROR( "Could not fork()!\n" );
               return -1;

          case 0:
               server_run();
               return 0;

          default:
               break;
     }


     ret = FusionDaleCreate( &test.dale );
     if (ret)
          FusionDaleErrorFatal( "FusionDaleCreate", ret );

     ret = test.dale->EnterComa( test.dale, "TestComa", &test.coma );
     if (ret)
          FusionDaleErrorFatal( "EnterComa", ret );

     ret = test.coma->CreateComponent( test.coma, "TestComponent", Test_MethodFunc, 0, &test, &test.component );
     if (ret)
          FusionDaleErrorFatal( "CreateComponent", ret );

     test.component->Activate( test.component );

     pause();
     //server_run();

     test.component->Release( test.component );
     test.coma->Release( test.coma );
     test.dale->Release( test.dale );

     return 0;
}

/**********************************************************************************************************************/

static DirectResult
ConstructDispatcher( VoodooServer     *server,
                     VoodooManager    *manager,
                     const char       *name,
                     void             *ctx,
                     VoodooInstanceID *ret_instance )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *interface;
     VoodooInstanceID      instance;

     D_ASSERT( server != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     ret = DirectGetInterface( &funcs, name, "Dispatcher", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface );
     if (ret)
          return ret;

     ret = funcs->Construct( interface, manager, &instance );
     if (ret)
          return ret;

     *ret_instance = instance;

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
server_run()
{
     DirectResult  ret;
     VoodooPlayer *player = NULL;
     VoodooServer *server = NULL;

     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the player (%s)!\n", FusionDaleErrorString(ret) );
          goto out;
     }

     ret = voodoo_server_create( NULL, 0, voodoo_config->server_fork, &server );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the server (%s)!\n", FusionDaleErrorString(ret) );
          goto out;
     }

     ret = voodoo_server_register( server, "IFusionDale", ConstructDispatcher, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not register super interface 'IFusionDale'!\n" );
          goto out;
     }

     ret = voodoo_server_run( server );
     if (ret)
          D_ERROR( "Voodoo/Proxy: Server exiting with error (%s)!\n", FusionDaleErrorString(ret) );

out:
     if (server)
          voodoo_server_destroy( server );

     if (player)
          voodoo_player_destroy( player );

     return ret;
}

