/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <malloc.h>

#include "directfb.h"

#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/surfaces.h"
#include "core/layers.h"

#include "conf.h"
#include "util.h"
#include "mem.h"


DFBConfig *dfb_config = NULL;

/*
 * prints a help message if "--help" is supplied
 */
static void config_print_usage()
{
     fprintf(stderr,
             "\n"
             "DirectFB options are:\n"
             " --fbdev=<device>                  "
             "Open <device> instead of /dev/fb0\n"
             " --quiet                           "
             "No text output except debugging\n"
             " --no-banner                       "
             "Disable DirectFB Banner\n"
             " --[no-]debug                      "
             "Disable/enable debug output\n"
             " --force-windowed                  "
             "Primary surface always is a window\n"
             " --[no-]hardware                   "
             "Hardware acceleration\n"
             " --[no-]sync                       "
             "Do `sync()' (default=no)\n"
#ifdef USE_MMX
             " --no-mmx                          "
             "Disable mmx support\n"
#endif
             " --argb-font                       "
             "Load glyphs into ARGB surfaces\n"
             " --no-sighandler                   "
             "Disable signal handler\n"
             " --no-deinit-check                 "
             "Disable deinit check at exit\n"
             " --no-vt-switch                    "
             "Don't allocate/switch to a new VT\n"
             " --[no-]vt-switching               "
             "Allow Ctrl+Alt+<F?> (EXPERIMENTAL)\n"
             " --graphics-vt                     "
             "Put terminal into graphics mode\n"
             " --[no-]motion-compression         "
             "Mouse Motion Event Compression\n"
             " --mouse-protocol=<protocol>       "
             "Mouse Protocol (serial mouse)\n"
             " --lefty                           "
             "Swap left and right mouse buttons\n"
             " --[no-]cursor                     "
             "Show cursor on start up (default)\n"
             " --bg-none                         "
             "Disable Background Clear\n"
             " --bg-color=AARRGGBB               "
             "Use Background Color (hex)\n"
             " --bg-image=<filename>             "
             "Use Background Image\n"
             " --matrox-sgram                    "
             "Use Matrox SGRAM features\n"
             "\n"
             "Window surface swapping policy:\n"
             " --window-surface-policy=(auto|videohigh|videolow|systemonly|videoonly)\n"
             "     auto:       DirectFB decides depending on hardware.\n"
             "     videohigh:  Swapping system/video with high priority.\n"
             "     videolow:   Swapping system/video with low priority.\n"
             "     systemonly: Window surface is always stored in system memory.\n"
             "     videoonly:  Window surface is always stored in video memory.\n"
             "\n"
             "Desktop buffer mode:\n"
             " --desktop-buffer-mode=(auto|backvideo|backsystem|frontonly)\n"
             "     auto:       DirectFB decides depending on hardware.\n"
             "     backvideo:  Front and back buffer are video only.\n"
             "     backsystem: Back buffer is system only.\n"
             "     frontonly:  There is no back buffer.\n"
             "\n"
             "Force synchronization of vertical retrace:\n"
             " --vsync-after:   Wait for the vertical retrace after flipping.\n"
             " --vsync-none:    disable polling for vertical retrace.\n"
             "\n"
           );
}

/*
 * The following function isn't used because the configuration should
 * only go away if the application is completely terminated. In that case
 * the memory is freed anyway.
 */

#if 0
static void config_cleanup()
{
     if (!dfb_config) {
          BUG("config_cleanup() called with no config allocated!");
          return;
     }

     if (dfb_config->fb_device)
          DFBFREE( dfb_config->fb_device );

     if (dfb_config->layer_bg_filename)
          DFBFREE( dfb_config->layer_bg_filename );

     DFBFREE( dfb_config );
     dfb_config = NULL;
}
#endif

/*
 * allocates config and fills it with defaults
 */
static void config_allocate()
{
     if (dfb_config)
          return;

     dfb_config = (DFBConfig*) calloc( 1, sizeof(DFBConfig) );

     dfb_config->layer_bg_color.a = 0xFF;
     dfb_config->layer_bg_color.r = 0x24;//29;
     dfb_config->layer_bg_color.g = 0x50;//69;
     dfb_config->layer_bg_color.b = 0x9f;
     dfb_config->layer_bg_mode    = DLBM_COLOR;

     dfb_config->show_cursor              =  1;
     dfb_config->mouse_motion_compression =  1;
     dfb_config->window_policy            = -1;
     dfb_config->buffer_mode              = -1;
}

DFBResult config_set( const char *name, const char *value )
{
     if (strcmp (name, "fbdev" ) == 0) {
          if (value) {
               if (dfb_config->fb_device)
                    DFBFREE( dfb_config->fb_device );
               dfb_config->fb_device = DFBSTRDUP( value );
          }
          else {
               ERRORMSG("DirectFB/Config --fbdev: No device name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "quiet" ) == 0) {
          dfb_config->quiet = 1;
     } else
     if (strcmp (name, "no-banner" ) == 0) {
          dfb_config->no_banner = 1;
     } else
     if (strcmp (name, "debug" ) == 0) {
          dfb_config->no_debug = 0;
     } else
     if (strcmp (name, "no-debug" ) == 0) {
          dfb_config->no_debug = 1;
     } else
     if (strcmp (name, "force-windowed" ) == 0) {
          dfb_config->force_windowed = 1;
     } else
     if (strcmp (name, "hardware" ) == 0) {
          dfb_config->software_only = 0;
     } else
     if (strcmp (name, "no-hardware" ) == 0) {
          dfb_config->software_only = 1;
     } else
#ifdef USE_MMX
     if (strcmp (name, "no-mmx" ) == 0) {
          dfb_config->no_mmx = 1;
     } else
#endif
     if (strcmp (name, "argb-font" ) == 0) {
          dfb_config->argb_font = 1;
     } else
     if (strcmp (name, "no-sighandler" ) == 0) {
          dfb_config->no_sighandler = 1;
     } else
     if (strcmp (name, "no-deinit-check" ) == 0) {
          dfb_config->no_deinit_check = 1;
     } else
     if (strcmp (name, "no-cursor" ) == 0) {
          dfb_config->show_cursor = 0;
     } else
     if (strcmp (name, "cursor" ) == 0) {
          dfb_config->show_cursor = 1;
     } else
     if (strcmp (name, "motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = 1;
     } else
     if (strcmp (name, "no-motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = 0;
     } else
     if (strcmp (name, "mouse-protocol" ) == 0) {
          if (value) {
               dfb_config->mouse_protocol = DFBSTRDUP( value );
          }
          else {
               ERRORMSG( "DirectFB/Config: No mouse protocol specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "vsync-none" ) == 0) {
          dfb_config->pollvsync_none = 1;
     } else
     if (strcmp (name, "vsync-after" ) == 0) {
          dfb_config->pollvsync_after = 1;
     } else
     if (strcmp (name, "no-vt-switch" ) == 0) {
          dfb_config->no_vt_switch = 1;
     } else
     if (strcmp (name, "vt-switching" ) == 0) {
          dfb_config->vt_switching = 1;
     } else
     if (strcmp (name, "no-vt-switching" ) == 0) {
          dfb_config->vt_switching = 0;
     } else
     if (strcmp (name, "graphics-vt" ) == 0) {
          dfb_config->kd_graphics = 1;
     } else
     if (strcmp (name, "bg-none" ) == 0) {
          dfb_config->layer_bg_mode = DLBM_DONTCARE;
     } else
     if (strcmp (name, "bg-image" ) == 0) {
          if (value) {
               if (dfb_config->layer_bg_filename)
                    DFBFREE( dfb_config->layer_bg_filename );
               dfb_config->layer_bg_filename = DFBSTRDUP( value );
               dfb_config->layer_bg_mode = DLBM_IMAGE;
          }
          else {
               ERRORMSG( "DirectFB/Config: No image filename specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "window-surface-policy" ) == 0) {
          if (value) {
               if (strcmp( value, "auto" ) == 0) {
                    dfb_config->window_policy = -1;
               } else
               if (strcmp( value, "videohigh" ) == 0) {
                    dfb_config->window_policy = CSP_VIDEOHIGH;
               } else
               if (strcmp( value, "videolow" ) == 0) {
                    dfb_config->window_policy = CSP_VIDEOLOW;
               } else
               if (strcmp( value, "systemonly" ) == 0) {
                    dfb_config->window_policy = CSP_SYSTEMONLY;
               } else
               if (strcmp( value, "videoonly" ) == 0) {
                    dfb_config->window_policy = CSP_VIDEOONLY;
               }
               else {
                    ERRORMSG( "DirectFB/Config: "
                              "Unknown window surface policy `%s'!\n", value );
                    return DFB_INVARG;
               }
          }
          else {
               ERRORMSG( "DirectFB/Config: "
                         "No window surface policy specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "desktop-buffer-mode" ) == 0) {
          if (value) {
               if (strcmp( value, "auto" ) == 0) {
                    dfb_config->buffer_mode = -1;
               } else
               if (strcmp( value, "backvideo" ) == 0) {
                    dfb_config->buffer_mode = DLBM_BACKVIDEO;
               } else
               if (strcmp( value, "backsystem" ) == 0) {
                    dfb_config->buffer_mode = DLBM_BACKSYSTEM;
               } else
               if (strcmp( value, "frontonly" ) == 0) {
                    dfb_config->buffer_mode = DLBM_FRONTONLY;
               } else {
                    ERRORMSG( "DirectFB/Config: Unknown buffer mode "
                              "'%s'!\n", value );
                    return DFB_INVARG;
               }
          }
          else {
               ERRORMSG( "DirectFB/Config: "
                         "No desktop buffer mode specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "bg-color" ) == 0) {
          if (value) {
               char *error;
               __u32 argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    ERRORMSG( "DirectFB/Config: Error in bg-color: "
                              "'%s'!\n", error );
                    return DFB_INVARG;
               }

               dfb_config->layer_bg_color.b = argb & 0xFF;
               argb >>= 8;
               dfb_config->layer_bg_color.g = argb & 0xFF;
               argb >>= 8;
               dfb_config->layer_bg_color.r = argb & 0xFF;
               argb >>= 8;
               dfb_config->layer_bg_color.a = argb & 0xFF;

               dfb_config->layer_bg_mode = DLBM_COLOR;
          }
          else {
               ERRORMSG( "DirectFB/Config: No background color specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "matrox-sgram" ) == 0) {
          dfb_config->matrox_sgram = 1;
     } else
     if (strcmp (name, "sync" ) == 0) {
          dfb_config->sync = 1;
     } else
     if (strcmp (name, "lefty" ) == 0) {
          dfb_config->lefty = 1;
     } else
     if (strcmp (name, "no-sync" ) == 0) {
          dfb_config->sync = 0;
     }
     else
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

DFBResult config_init( int *argc, char **argv[] )
{
     DFBResult ret;
     int i;
     char *home = getenv( "HOME" );

     if (dfb_config)
          return DFB_OK;

     config_allocate();

     ret = config_read( "/etc/directfbrc" );
     if (ret  &&  ret != DFB_IO)
          return ret;

     if (home) {
          char *filename = alloca( strlen(home) + strlen("/.directfbrc") + 1 );

          filename = strcpy( filename, home );
          filename = strcat( filename, "/.directfbrc" );

          ret = config_read( filename );

          if (ret  &&  ret != DFB_IO)
               return ret;
     }

     if (argc && argv) {
          for (i = 1; i < *argc; i++) {
               /*  FIXME: shouldn't parse --help myself, leave it to the app  */
               if (strcmp ((*argv)[i], "--help") == 0) {
                    config_print_usage();
                    exit(1);
               }

               if (strncmp ((*argv)[i], "--", 2) == 0) {
                    int len = strlen( (*argv)[i] ) - 2;

                    if (len) {
                         char *name, *value;

                         name = DFBSTRDUP( (*argv)[i] + 2 );
                         value = strchr( name, '=' );

                         if (value)
                              *value++ = '\0';

                         ret = config_set( name, value );

                         DFBFREE( name );

                         if (ret == DFB_OK)
                              (*argv)[i] = NULL;
                         else
                              if (ret != DFB_UNSUPPORTED)
                                   return ret;
                    }
               }
          }

          for (i = 1; i < *argc; i++) {
           int k;

               for (k = i; k < *argc; k++)
                   if ((*argv)[k] != NULL)
                       break;

               if (k > i)
                   {
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

DFBResult config_read( const char *filename )
{
     DFBResult ret = DFB_OK;
     char line[400];
     FILE *f;

     config_allocate();

     f = fopen( filename, "r" );
     if (!f) {
          DEBUGMSG( "DirectFB/Config: "
                    "Unable to open config file `%s'!\n", filename );
          return DFB_IO;
     } else {
          INITMSG( "parsing config file '%s'.\n", filename );
     }

     while (fgets( line, 400, f )) {
          char *name = line;
          char *value = strchr( line, '=' );

          if (value) {
               *value++ = 0;
               trim( &value );
          }

          trim( &name );

          if (!*name  ||  *name == '#')
               continue;

          ret = config_set( name, value );
          if (ret) {
               if (ret == DFB_UNSUPPORTED)
                    ERRORMSG( "DirectFB/Config: In config file `%s': "
                              "Invalid option `%s'!\n", filename, name );
               break;
          }
     }

     fclose( f );

     return ret;
}

