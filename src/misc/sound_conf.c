/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net.

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

#include <fusionsound.h>
#include <fusionsound_limits.h>

#include <direct/conf.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/debug.h>
#include <direct/util.h>

#include <fusion/conf.h>

#include "sound_conf.h"
#include "sound_util.h"


FSConfig *fs_config = NULL;

static const char *config_usage =
     "FusionSound version " FUSIONSOUND_VERSION "\n"
     "\n"
     " --fs-help                        Output FusionSound usage information and exit\n"
     " --fs:<option>[,<option>]...      Pass options to FusionSound (see below)\n"
     "\n"
     "FusionSound options:\n"
     "\n"
     "  driver=<driver>                 Specify driver to use (e.g \"oss\")\n"
     "  device=<device>                 Specify driver ouput device (e.g \"/dev/dsp\")\n"
     "  channels=<channels>             Set the default number of channels\n"
     "  channelmode=<channelmode>       Set the default channel mode\n"
     "  sampleformat=<sampleformat>     Set the default sample format\n"
     "  samplerate=<samplerate>         Set the default sample rate\n"
     "  buffertime=<millisec>           Set the default buffer time\n"
     "  session=<num>                   Select local multi app world (-1 = new)\n"
     "  remote=<host>[:<session>]       Select remote session for Voodoo Sound\n"
     "  remote-compression=(none|dpack) Select compression method for remote session\n"
     "  [no-]banner                     Show FusionSound banner on startup\n"
     "  [no-]wait                       Wait slaves before quitting\n"
     "  [no-]deinit-check               Enable deinit check at exit\n"
     "  [no-]dither                     Enable dithering\n"
     "  [no-]dma                        Enable DMA\n"
     "\n";
     
typedef struct {
     const char     *string;
     FSSampleFormat  format;
} FormatString;

static const FormatString format_strings[] = {
     { "U8",    FSSF_U8    },
     { "S16",   FSSF_S16   },
     { "S24",   FSSF_S24   },
     { "S32",   FSSF_S32   },
     { "FLOAT", FSSF_FLOAT }
};

static FSSampleFormat
parse_sampleformat( const char *format )
{
     int i;
     
     for (i = 0; i < D_ARRAY_SIZE(format_strings); i++) {
          if (!strcmp( format, format_strings[i].string ))
               return format_strings[i].format;
     }

     return FSSF_UNKNOWN;
}


typedef struct {
     const char     *string;
     FSChannelMode   mode;
} ModeString;

static const ModeString mode_strings[] = {
     { "MONO",            FSCM_MONO            },
     { "STEREO",          FSCM_STEREO          },
#if FS_MAX_CHANNELS > 2
     { "STEREO21",        FSCM_STEREO21        },
     { "STEREO30",        FSCM_STEREO30        },
     { "STEREO31",        FSCM_STEREO31        },
     { "SURROUND30",      FSCM_SURROUND30      },
     { "SURROUND31",      FSCM_SURROUND31      },
     { "SURROUND40_2F2R", FSCM_SURROUND40_2F2R },
     { "SURROUND41_2F2R", FSCM_SURROUND41_2F2R },
     { "SURROUND40_3F1R", FSCM_SURROUND40_3F1R },
     { "SURROUND41_3F1R", FSCM_SURROUND41_3F1R },
     { "SURROUND50",      FSCM_SURROUND50      },
     { "SURROUND51",      FSCM_SURROUND51      }
#endif
};

static FSChannelMode
parse_modestring( const char *mode )
{
     int i;
     
     for (i = 0; i < D_ARRAY_SIZE(mode_strings); i++) {
          if (!strcmp( mode, mode_strings[i].string ))
               return mode_strings[i].mode;
     }

     return FSCM_UNKNOWN;
}


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
               fprintf( stderr, "%s%s%s", config_usage, fusion_config_usage, direct_config_usage );
               exit(1);
          }

          if ((value = strchr( buf, '=' )) != NULL)
               *value++ = '\0';

          ret = fs_config_set( buf, value );
          switch (ret) {
               case DFB_OK:
                    break;
               case DFB_UNSUPPORTED:
                    D_ERROR( "FusionSound/Config: Unknown option '%s'!\n", buf );
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
     if (fs_config)
          return;
          
     fs_config = (FSConfig*) D_CALLOC( 1, sizeof(FSConfig) );
     
     fs_config->sampleformat = FSSF_S16;
     fs_config->channelmode  = FSCM_STEREO;
     fs_config->samplerate   = 48000;
     fs_config->buffertime   = 25;
     fs_config->session      = 2;
     fs_config->banner       = true;
     fs_config->wait         = true;
     fs_config->deinit_check = true;
}

const char*
fs_config_usage( void )
{
     return config_usage;
}

DFBResult 
fs_config_set( const char *name, const char *value )
{
     if (!strcmp( name, "driver" )) {
          if (value) {
               if (fs_config->driver)
                    D_FREE( fs_config->driver );
               fs_config->driver = D_STRDUP( value );
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          } 
     }
     else if (!strcmp( name, "device" )) {
          if (value) {
               if (fs_config->device)
                    D_FREE( fs_config->device );
               fs_config->device = D_STRDUP( value );
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "channels" )) {
          if (value) {
               int channels;

               if (sscanf( value, "%d", &channels ) < 1) {
                    D_ERROR( "FusionSound/Config '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }
               else if (channels < 1 || channels > FS_MAX_CHANNELS) {
                    D_ERROR( "FusionSound/Config '%s': Unsupported value '%d'!\n", name, channels );
                    return DFB_INVARG;
               }      

               fs_config->channelmode = fs_mode_for_channels( channels );
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "channelmode" )) {
          if (value) {
               FSChannelMode mode;

               mode = parse_modestring( value );
               if (mode == FSCM_UNKNOWN) {
                    D_ERROR( "FusionSound/Config '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }      

               fs_config->channelmode = mode;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "sampleformat" )) {
          if (value) {
               FSSampleFormat format;

               format = parse_sampleformat( value );
               if (format == FSSF_UNKNOWN) {
                    D_ERROR( "FusionSound/Config '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               fs_config->sampleformat = format;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No format specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "samplerate" )) {
          if (value) {
               int rate;

               if (sscanf( value, "%d", &rate ) < 1) {
                    D_ERROR( "FusionSound/Config 'samplerate': "
                             "Could not parse value!\n" );
                    return DFB_INVARG;
               }
               else if (rate < 1) {
                    D_ERROR( "FusionSound/Config '%s': Unsupported value '%d'!\n", name, rate );
                    return DFB_INVARG;
               }      

               fs_config->samplerate = rate;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "buffertime" )) {
          if (value) {
               int time;

               if (sscanf( value, "%d", &time ) < 1) {
                    D_ERROR( "FusionSound/Config 'buffertime': "
                             "Could not parse value!\n" );
                    return DFB_INVARG;
               }
               else if (time < 1 || time > 5000) {
                    D_ERROR( "FusionSound/Config '%s': Unsupported value '%d'!\n", name, time );
                    return DFB_INVARG;
               }      

               fs_config->buffertime = time;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "session" )) {
          if (value) {
               int session;

               if (sscanf( value, "%d", &session ) < 1) {
                    D_ERROR( "FusionSound/Config '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               fs_config->session = session;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp (name, "remote" )) {
          if (value) {
               char host[128];
               int  session = 0;

               if (sscanf( value, "%127s:%d", host, &session ) < 1) {
                    D_ERROR( "FusionSound/Config '%s': "
                             "Could not parse value (format is <host>[:<session>])!\n", name );
                    return DFB_INVARG;
               }

               if (fs_config->remote.host)
                    D_FREE( fs_config->remote.host );

               fs_config->remote.host    = D_STRDUP( host );
               fs_config->remote.session = session;
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else if (!strcmp( name, "remote-compression" )) {
          if (value) {
               if (!strcasecmp( value, "none" )) {
                    fs_config->remote_compression = FSRM_NONE;
               }
               else if (!strcasecmp( value, "dpack" )) {
                    fs_config->remote_compression = FSRM_DPACK;
               }
               else {
                    D_ERROR( "FusionSound/Config '%s': Unsupported value '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "FusionSound/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } 
     else if (!strcmp( name, "banner" )) {
          fs_config->banner = true;
     }
     else if (!strcmp( name, "no-banner" )) {
          fs_config->banner = false;
     }
     else if (!strcmp( name, "wait" )) {
          fs_config->wait = true;
     }
     else if (!strcmp( name, "no-wait" )) {
          fs_config->wait = false;
     }
     else if (!strcmp( name, "deinit-check" )) {
          fs_config->deinit_check = true;
     }
     else if (!strcmp( name, "no-deinit-check" )) {
          fs_config->deinit_check = false;
     }
     else if (!strcmp( name, "dither" )) {
          fs_config->dither = true;
     }
     else if (!strcmp( name, "no-dither" )) {
          fs_config->dither = false;
     }
     else if (!strcmp( name, "dma" )) {
          fs_config->dma = true;
     }
     else if (!strcmp( name, "no-dma" )) {
          fs_config->dma = false;
     }
     else if (fusion_config_set( name, value ) && direct_config_set( name, value ))
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult 
fs_config_read( const char *filename )
{
     DFBResult  ret = DFB_OK;
     char       line[400];
     FILE      *f;

     f = fopen( filename, "r" );
     if (!f) {
          D_DEBUG( "FusionSound/Config: "
                   "Unable to open config file `%s'!\n", filename );
          return DFB_IO;
     } else {
          D_INFO( "FusionSound/Config: "
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

          ret = fs_config_set( name, value );
          if (ret) {
               if (ret == DFB_UNSUPPORTED)
                    D_ERROR( "FusionSound/Config: In config file `%s': "
                             "Invalid option `%s'!\n", filename, name );
               break;
          }
     }

     fclose( f );

     return ret;
}

DFBResult 
fs_config_init( int *argc, char **argv[] )
{
     DFBResult  ret;
     char      *home   = getenv( "HOME" );
     char      *prog   = NULL;
     char      *fsargs;
     
     if (fs_config)
          return DFB_OK;
          
     config_allocate();
     
     /* Read system settings. */
     ret = fs_config_read( SYSCONFDIR"/fusionsoundrc" );
     if (ret  &&  ret != DFB_IO)
          return ret;
          
     /* Read user settings. */
     if (home) {
          int  len = strlen(home) + sizeof("/.fusionsoundrc");
          char buf[len];

          snprintf( buf, len, "%s/.fusionsoundrc", home );

          ret = fs_config_read( buf );
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
          int  len = sizeof(SYSCONFDIR"/fusionsoundrc.") + strlen(prog);
          char buf[len];

          snprintf( buf, len, SYSCONFDIR"/fusionsoundrc.%s", prog );

          ret = fs_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
     
     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen(home) + sizeof("/.fusionsoundrc.") + strlen(prog);
          char buf[len];

          snprintf( buf, len, "%s/.fusionsoundrc.%s", home, prog );

          ret = fs_config_read( buf );
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

               if (!strcmp( (*argv)[i], "--fs-help" )) {
                    fprintf( stderr, "%s%s%s", config_usage, fusion_config_usage, direct_config_usage );
                    exit(1);
               }

               if (!strncmp( (*argv)[i], "--fs:", 5 )) {
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
