/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fusiondale.h>

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/conf.h>

#include "dale_config.h"


FusionDaleConfig *fusiondale_config = NULL;

static const char *config_usage =
     "FusionDale version " FUSIONDALE_VERSION "\n"
     "\n"
     " --fd-help                       Output FusionDale usage information and exit\n"
     " --fd:<option>[,<option>]...     Pass options to FusionDale (see below)\n"
     "\n"
     "FusionDale options:\n"
     "\n"
     "  session=<num>                  Select multi app world (-1 = new)\n"
     "  [no-]banner                    Show FusionDale banner on startup\n"
     "\n";
     

static DFBResult
parse_args( const char *args )
{
     char *buf = alloca( strlen(args) + 1 );

     strcpy( buf, args );

     while (buf && buf[0]) {
          DFBResult  ret;
          char      *value;
          char      *next;

          if ((next = strchr( buf, ',' )) != NULL)
               *next++ = '\0';

          if (strcmp (buf, "help") == 0) {
               fprintf( stderr, config_usage );
               exit(1);
          }

          if ((value = strchr( buf, '=' )) != NULL)
               *value++ = '\0';

          ret = fd_config_set( buf, value );
          switch (ret) {
               case DFB_OK:
                    break;
               case DFB_UNSUPPORTED:
                    D_ERROR( "FusionDale/Config: Unknown option '%s'!\n", buf );
                    break;
               default:
                    return ret;
          }

          buf = next;
     }

     return DFB_OK;
}

static void 
config_allocate()
{
     if (fusiondale_config)
          return;
          
     fusiondale_config = D_CALLOC( 1, sizeof(FusionDaleConfig) );
     
     fusiondale_config->session = 5;  // FIXME!!!

     fusiondale_config->banner  = true;
}

const char*
fd_config_usage( void )
{
     return config_usage;
}

DFBResult 
fd_config_set( const char *name, const char *value )
{
     if (!strcmp( name, "session" )) {
          if (value) {
               int session;

               if (sscanf( value, "%d", &session ) < 1) {
                    D_ERROR( "FusionDale/Config 'session': "
                             "Could not parse value!\n");
                    return DFB_INVARG;
               }

               fusiondale_config->session = session;
          }
          else {
               D_ERROR( "FusionDale/Config 'session': "
                        "No value specified!\n" );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "banner" )) {
          fusiondale_config->banner = true;
     }
     else if (!strcmp( name, "no-banner" )) {
          fusiondale_config->banner = false;
     }
     else if (!strcmp( name, "debug" )) {
          direct_config->debug = true;
     }
     else if (!strcmp( name, "no-debug" )) {
          direct_config->debug = false;
     }
     else if (!strcmp( name, "debugshm" )) {
          fusion_config->debugshm = true;
     }
     else if (!strcmp( name, "no-debugshm" )) {
          fusion_config->debugshm = false;
     }
     else
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult 
fd_config_read( const char *filename )
{
     DFBResult  ret = DFB_OK;
     char       line[400];
     FILE      *f;

     f = fopen( filename, "r" );
     if (!f) {
          D_DEBUG( "FusionDale/Config: "
                   "Unable to open config file `%s'!\n", filename );
          return DFB_IO;
     } else {
          D_INFO( "FusionDale/Config: "
                  "Parsing config file '%s'.\n", filename );
     }

     while (fgets( line, 400, f )) {
          char *name  = line;
          char *value = strchr( line, '=' );

          if (value) {
               *value++ = 0;
               direct_trim( &value );
          }

          direct_trim( &name );

          if (!*name || *name == '#')
               continue;

          ret = fd_config_set( name, value );
          if (ret) {
               if (ret == DFB_UNSUPPORTED)
                    D_ERROR( "FusionDale/Config: In config file `%s': "
                             "Invalid option `%s'!\n", filename, name );
               break;
          }
     }

     fclose( f );

     return ret;
}

DFBResult 
fd_config_init( int *argc, char **argv[] )
{
     DFBResult  ret;
     char      *home   = getenv( "HOME" );
     char      *prog   = NULL;
     char      *fsargs;
     
     if (fusiondale_config)
          return DFB_OK;
          
     config_allocate();
     
     /* Read system settings. */
     ret = fd_config_read( "/etc/fusiondalerc" );
     if (ret  &&  ret != DFB_IO)
          return ret;
          
     /* Read user settings. */
     if (home) {
          int  len = strlen(home) + sizeof("/.fusiondalerc");
          char buf[len];

          snprintf( buf, len, "%s/.fusiondalerc", home );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
     
     /* Get application name. */
     if (argc && *argc && argv && *argv) {
          prog = strrchr( (*argv)[0], '/' );

          if (prog)
               prog++;
          else
               prog = (*argv)[0];
     }

     /* Read global application settings. */
     if (prog && prog[0]) {
          int  len = sizeof("/etc/fusiondalerc.") + strlen(prog);
          char buf[len];

          snprintf( buf, len, "/etc/fusiondalerc.%s", prog );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
     
     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen(home) + sizeof("/.fusiondalerc.") + strlen(prog);
          char buf[len];

          snprintf( buf, len, "%s/.fusiondalerc.%s", home, prog );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
     
     /* Read settings from environment variable. */
     fsargs = getenv( "FSARGS" );
     if (fsargs) {
          ret = parse_args( fsargs );
          if (ret)
               return ret;
     }
     
     /* Read settings from command line. */
     if (argc && argv) {
          int i;
          
          for (i = 1; i < *argc; i++) {

               if (!strcmp( (*argv)[i], "--fd-help" )) {
                    fprintf( stderr, config_usage );
                    exit(1);
               }

               if (!strncmp( (*argv)[i], "--fd:", 5 )) {
                    ret = parse_args( (*argv)[i] + 5 );
                    if (ret)
                         return ret;

                    (*argv)[i] = NULL;
               }
          }

          for (i = 1; i < *argc; i++) {
               int k;

               for (k = i; k < *argc; k++)
                    if ((*argv)[k] != NULL)
                         break;

               if (k > i) {
                    int j;

                    k -= i;

                    for (j = i + k; j < *argc; j++)
                         (*argv)[j-k] = (*argv)[j];

                    *argc -= k;
               }
          }
     }

     return DFB_OK;
}

