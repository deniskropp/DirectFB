/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/play.h>

static const char *m_name   = NULL;
static bool        m_run    = false;
static const char *m_lookup = NULL;

/**********************************************************************************************************************/

static DFBBoolean parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

static DirectEnumerationResult
player_callback( void                    *ctx,
                 const VoodooPlayInfo    *info,
                 const VoodooPlayVersion *version,
                 const char              *address,
                 unsigned int             ms_since_last_seen )
{
     D_INFO( "Voodoo/Play: <%4ums> [ %-30s ]   %s%s   (vendor: %s, model: %s)\n",
             ms_since_last_seen, info->name, address, (info->flags & VPIF_LEVEL2) ? " *" : "",
             info->vendor, info->model );

     return DENUM_OK;
}

int
main( int argc, char *argv[] )
{
     DFBResult       ret;
     int             i;
     VoodooPlayInfo  info;
     VoodooPlayer   *player = NULL;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     if (m_name) {
          direct_snputs( info.name, m_name, VOODOO_PLAYER_NAME_LENGTH );
     }

     ret = voodoo_player_create( m_name ? &info : NULL, &player );
     if (ret) {
          D_ERROR( "Voodoo/Play: Could not create the player (%s)!\n", DirectFBErrorString(ret) );
          goto out;
     }


     do {
          voodoo_player_broadcast( player );

          direct_thread_sleep( 100000 );

          voodoo_player_enumerate( player, player_callback, NULL );

          if (m_lookup) {
               for (i=1; i<argc; i++) {
                    char buf[100];

                    if (voodoo_player_lookup( player, (const u8 *)argv[i], NULL, buf, sizeof(buf) )) {
                         D_ERROR( "Voodoo/Play: No '%s' found!\n", argv[i] );
                         continue;
                    }

                    D_INFO( "Voodoo/Play: Found '%s' with address %s\n", argv[i], buf );
               }
          }

          direct_thread_sleep( 2000000 );
     } while (m_run);


out:
     if (player)
          voodoo_player_destroy( player );

     return ret;
}

/**********************************************************************************************************************/

static DFBBoolean
print_usage( const char *name )
{
     fprintf( stderr, "Usage: %s [-n <name>] [-r]\n", name );

     return DFB_FALSE;
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int i;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-r" )) {
               m_run = true;
          }
          else if (!strcmp( argv[i], "-n" )) {
               if (++i == argc)
                    return print_usage( argv[0] );

               m_name = argv[i];
          }
     }

     return DFB_TRUE;
}

