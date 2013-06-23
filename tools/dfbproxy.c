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


static const char *m_addr;
static int         m_port;

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

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "\nDirectFB Proxy (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -a,  --addr      <address>        Listen on specified address\n");
     fprintf (stderr, "   -p,  --port      <port>           Listen on specified port\n");
     fprintf (stderr, "   -h,  --help                       Show this help message\n");
     fprintf (stderr, "   -v,  --version                    Print version information\n");
     fprintf (stderr, "\n");
     fprintf (stderr, "Followed by '--dfb:'  - %s\n", voodoo_config_usage());
}

static DFBBoolean
parse_addr( const char *arg )
{
     m_addr = arg;

     return DFB_TRUE;
}

static DFBBoolean
parse_port( const char *arg )
{
     if (sscanf( arg, "%d", &m_port ) != 1 || m_port < 0 || m_port > 65535) {
          fprintf (stderr, "\nInvalid port specified!\n\n");

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbdump version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-a") == 0 || strcmp (arg, "--addr") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_addr( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-p") == 0 || strcmp (arg, "--port") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_port( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          print_usage (argv[0]);

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBResult
server_run()
{
     static const char *super_interfaces[] = {
          "IDirectFB",
          "IDiVine",
          "IFusionDale",
          "ISaWMan"
     };

     DFBResult     ret;
     size_t        i;
     VoodooPlayer *player = NULL;
     VoodooServer *server = NULL;

     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the player (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }

     ret = voodoo_server_create( m_addr, m_port, voodoo_config->server_fork, &server );
     if (ret) {
          D_ERROR( "Voodoo/Proxy: Could not create the server (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }

     for (i=0; i<D_ARRAY_SIZE(super_interfaces); i++) {
          ret = voodoo_server_register( server, super_interfaces[i], ConstructDispatcher, NULL );
          if (ret) {
               D_DERROR( ret, "Voodoo/Proxy: Could not register super interface '%s'!\n", super_interfaces[i] );
               goto out;
          }
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

