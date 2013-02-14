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

#include <directfb_build.h>

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

#if !DIRECTFB_BUILD_PURE_VOODOO
#include <coma/policy.h>
#endif

#include "dale_config.h"


FusionDaleConfig *fusiondale_config = NULL;

static const char *config_usage =
     "FusionDale version " DIRECTFB_VERSION "\n"
     "\n"
     " --fd-help                       Output FusionDale usage information and exit\n"
     " --fd:<option>[,<option>]...     Pass options to FusionDale (see below)\n"
     "\n"
     "FusionDale options:\n"
     "\n"
     "  coma-shmpool-size=<kb>         Set the maximum size of the shared memory pool created by\n"
     "                                 each component manager (once for all EnterComa with same name)\n"
     "  session=<num>                  Select multi app world (-1 = new)\n"
     "  remote=<host>[:<session>]      Select remote session to connect to\n"
     "  [no-]banner                    Show FusionDale banner on startup\n"
     "\n";
     

static void
print_config_usage( void )
{
     fprintf( stderr, "%s%s%s", config_usage, fusion_config_usage, direct_config_usage );
}

static DirectResult
parse_args( const char *args )
{
     char *buf = malloc( strlen(args) + 1 );

     strcpy( buf, args );

     while (buf && buf[0]) {
          DirectResult  ret;
          char         *value;
          char         *next;

          if ((next = strchr( buf, ',' )) != NULL)
               *next++ = '\0';

          if (strcmp (buf, "help") == 0) {
               print_config_usage();
               exit(1);
          }

          if ((value = strchr( buf, '=' )) != NULL)
               *value++ = '\0';

          ret = fd_config_set( buf, value );
          switch (ret) {
               case DR_OK:
                    break;
               case DR_UNSUPPORTED:
                    D_ERROR( "FusionDale/Config: Unknown option '%s'!\n", buf );
                    break;
               default:
		            free( buf );
                    return ret;
          }

          buf = next;
     }

     free( buf );

     return DR_OK;
}

static int config_read_cmdline( char *cmdbuf, int size, FILE *f )
{
     int ret = 0;
     int len = 0;

     ret = fread( cmdbuf, 1, 1, f );

     /* empty dividing 0 */
     if( ret==1 && *cmdbuf==0 ) {
          ret = fread( cmdbuf, 1, 1, f );
     }

     while(ret==1 && len<(size-1)) {
          len++;
          ret = fread( ++cmdbuf, 1, 1, f );
          if( *cmdbuf == 0 )
               break;
     }

     if( len ) {
          cmdbuf[len]=0;
     }

     return  len != 0;
}

static void 
config_allocate( void )
{
     if (fusiondale_config)
          return;
          
     fusiondale_config = D_CALLOC( 1, sizeof(FusionDaleConfig) );
     
     fusiondale_config->session           = 5;  // FIXME!!!

     fusiondale_config->banner            = true;
     fusiondale_config->coma_shmpool_size = 16 * 1024 * 1024;

#if FUSIONDALE_USE_ONE
     fusiondale_config->remote.host       = D_STRDUP( "%" );
#endif
}

const char*
fd_config_usage( void )
{
     return config_usage;
}

DirectResult 
fd_config_set( const char *name, const char *value )
{
     if (!strcmp( name, "session" )) {
          if (value) {
               int session;

               if (sscanf( value, "%d", &session ) < 1) {
                    D_ERROR( "FusionDale/Config 'session': "
                             "Could not parse value!\n");
                    return DR_INVARG;
               }

               fusiondale_config->session = session;

               if (fusiondale_config->remote.host) {
                    D_FREE( fusiondale_config->remote.host );
                    fusiondale_config->remote.host = NULL;
               }
          }
          else {
               D_ERROR( "FusionDale/Config 'session': "
                        "No value specified!\n" );
               return DR_INVARG;
          }
     }
     else if (strcmp (name, "remote" ) == 0) {
          if (value) {
               char host[128];
               int  session = 0;

               if (sscanf( value, "%127s:%d", host, &session ) < 1) {
                    D_ERROR("FusionDale/Config 'remote': "
                            "Could not parse value (format is <host>[:<session>])!\n");
                    return DR_INVARG;
               }

               if (fusiondale_config->remote.host)
                    D_FREE( fusiondale_config->remote.host );

               fusiondale_config->remote.host    = D_STRDUP( host );
               fusiondale_config->remote.session = session;
          }
          else {
               fusiondale_config->remote.host    = D_STRDUP( "" );
               fusiondale_config->remote.session = 0;
          }
     }
     else if (!strcmp( name, "coma-shmpool-size" )) {
          if (value) {
               int size_kb;

               if (sscanf( value, "%d", &size_kb ) < 1) {
                    D_ERROR( "FusionDale/Config '%s': Could not parse value!\n", name);
                    return DR_INVARG;
               }

               fusiondale_config->coma_shmpool_size = size_kb * 1024;
          }
          else {
               D_ERROR( "FusionDale/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     }
     else if (!strcmp( name, "banner" )) {
          fusiondale_config->banner = true;
     }
     else if (!strcmp( name, "no-banner" )) {
          fusiondale_config->banner = false;
     }
     else if (strcmp ( name, "force-slave" ) == 0) {
          fusiondale_config->force_slave = true;
     }
     else if (strcmp ( name, "no-force-slave" ) == 0) {
          fusiondale_config->force_slave = false;
     }
#if !DIRECTFB_BUILD_PURE_VOODOO
     else if (strcmp ( name, "coma-allow" ) == 0) {
          if (value) {
               coma_policy_config( value, true );
          }
          else {
               fusiondale_config->coma_policy = true;
          }
     }
     else if (strcmp ( name, "coma-deny" ) == 0) {
          if (value) {
               coma_policy_config( value, false );
          }
          else {
               fusiondale_config->coma_policy = false;
          }
     } else
#endif
     if (fusion_config_set( name, value ) && direct_config_set( name, value ))
          return DR_UNSUPPORTED;

     return DR_OK;
}

static DirectResult 
fd_config_read( const char *filename )
{
     DirectResult  ret = DR_OK;
     char          line[400];
     FILE         *f;

     f = fopen( filename, "r" );
     if (!f) {
          D_DEBUG( "FusionDale/Config: "
                   "Unable to open config file `%s'!\n", filename );
          return DR_IO;
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
               if (ret == DR_UNSUPPORTED)
                    D_ERROR( "FusionDale/Config: In config file `%s': "
                             "Invalid option `%s'!\n", filename, name );
               break;
          }
     }

     fclose( f );

     return ret;
}

DirectResult 
fd_config_init( int *argc, char **argv[] )
{
     DirectResult  ret;
     char         *home   = direct_getenv( "HOME" );
     char         *prog   = NULL;
     char         *fdargs;
#ifndef WIN32
     char          cmdbuf[1024];
#endif
     
     if (fusiondale_config)
          return DR_OK;
          
     config_allocate();
     
     /* Read system settings. */
     ret = fd_config_read( SYSCONFDIR"/fusiondalerc" );
     if (ret  &&  ret != DR_IO)
          return ret;
          
     /* Read user settings. */
     if (home) {
          int  len = strlen(home) + sizeof("/.fusiondalerc");
#ifndef WIN32
          char buf[len];
#else
          char buf[4096];
#endif

          direct_snprintf( buf, len, "%s/.fusiondalerc", home );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DR_IO)
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
#ifndef WIN32
     else {
          /* if we didn't receive argc/argv we try the proc system */
          FILE *f;
          int   len;

          f = fopen( "/proc/self/cmdline", "r" );
          if (f) {
               len = fread( cmdbuf, 1, 1023, f );
               if (len) {
                    cmdbuf[len] = 0; /* in case of no arguments, or long program name */
                    prog = strrchr( cmdbuf, '/' );
                    if (prog)
                         prog++;
                    else
                         prog = cmdbuf;
               }
               fprintf(stderr,"commandline read: %s\n", prog );
               fclose( f );
          }
     }
#endif

     /* Strip lt- prefix. */
     if (prog) {
          if (prog[0] == 'l' && prog[1] == 't' && prog[2] == '-')
            prog += 3;
     }

     /* Read global application settings. */
     if (prog && prog[0]) {
          int  len = sizeof(SYSCONFDIR"/fusiondalerc.") + strlen(prog);
#ifndef WIN32
          char buf[len];
#else
          char buf[4096];
#endif

          direct_snprintf( buf, len, SYSCONFDIR"/fusiondalerc.%s", prog );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DR_IO)
               return ret;
     }
     
     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen(home) + sizeof("/.fusiondalerc.") + strlen(prog);
#ifndef WIN32
          char buf[len];
#else
          char buf[4096];
#endif

          direct_snprintf( buf, len, "%s/.fusiondalerc.%s", home, prog );

          ret = fd_config_read( buf );
          if (ret  &&  ret != DR_IO)
               return ret;
     }
     
     /* Read settings from environment variable. */
     fdargs = direct_getenv( "FDARGS" );
     if (fdargs) {
          ret = parse_args( fdargs );
          if (ret)
               return ret;
     }
     
     /* Read settings from command line. */
     if (argc && argv) {
          int i;
          
          for (i = 1; i < *argc; i++) {

               if (!strcmp( (*argv)[i], "--fd-help" )) {
                    print_config_usage();
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
#ifndef WIN32
     else if (prog) {
          /* we have prog, so we try again the proc filesystem */
          FILE *f;
          int   len;

          len = strlen( cmdbuf );
          f = fopen( "/proc/self/cmdline", "r" );
          if (f) {
               len = fread( cmdbuf, 1, len, f ); /* skip arg 0 */
               while( config_read_cmdline( cmdbuf, 1024, f ) ) {
                    fprintf(stderr,"commandline read: %s\n", cmdbuf );
                    if (strcmp (cmdbuf, "--dfb-help") == 0) {
                         print_config_usage();
                         exit(1);
                    }

                    if (strncmp (cmdbuf, "--dfb:", 6) == 0) {
                         ret = parse_args( cmdbuf + 6 );
                         if (ret) {
                              fclose( f );
                              return ret;
                         }
                    }
               }
               fclose( f );
          }
     }
#endif

     return DR_OK;
}

