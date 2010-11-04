/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <voodoo/conf.h>
#include <voodoo/play.h>
#include <voodoo/server.h>

/*****************************************************************************/

static DFBBoolean parse_command_line( int argc, char *argv[] );
static DFBResult  server_run( void );

/*****************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     /* Run the server. */
     return server_run();
}

/*****************************************************************************/

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

     return DFB_OK;
}

/*****************************************************************************/

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     return DFB_TRUE;
}

static DFBResult
server_run()
{
     DFBResult     ret;
     VoodooPlayer *player = NULL;
     VoodooServer *server = NULL;

     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the player (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }

     ret = voodoo_server_create( NULL, 0, voodoo_config->server_fork, &server );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the server (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }

     ret = voodoo_server_register( server, "IDirectFB", ConstructDispatcher, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not register super interface 'IDirectFB'!\n" );
          goto out;
     }

     ret = voodoo_server_register( server, "IDiVine", ConstructDispatcher, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not register super interface 'IDiVine'!\n" );
          goto out;
     }

     ret = voodoo_server_run( server );
     if (ret)
          D_ERROR( "Voodoo/Proxy: Server exiting with error (%s)!\n", DirectFBErrorString(ret) );

out:
     if (server)
     voodoo_server_destroy( server );

     if (player)
          voodoo_player_destroy( player );

     return ret;
}

