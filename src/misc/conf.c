/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

static const char *config_usage =
    "DirectFB version " DIRECTFB_VERSION "\n\n"
    " --dfb-help                      "
    "Output DirectFB usage information and exit\n"
    " --dfb:<option>[,<option>]...    "
    "Pass options to DirectFB (see below)\n"
    "\n"
    "DirectFB options:\n\n"
    "  fbdev=<device>                 "
    "Open <device> instead of /dev/fb0\n"
    "  mode=<width>x<height>          "
    "Set the default resolution\n"
    "  depth=<pixeldepth>             "
    "Set the default pixel depth\n"
    "  quiet                          "
    "No text output except debugging\n"
    "  [no-]banner                    "
    "Show DirectFB Banner on startup\n"
    "  [no-]debug                     "
    "Enable debug output\n"
    "  force-windowed                 "
    "Primary surface always is a window\n"
    "  [no-]hardware                  "
    "Hardware acceleration\n"
    "  [no-]sync                      "
    "Do `sync()' (default=no)\n"
#ifdef USE_MMX
    "  [no-]mmx                       "
    "Enable mmx support\n"
#endif
    "  [no-]argb-font                 "
    "Load glyphs into ARGB surfaces\n"
    "  dont-catch=<num>[[,<num>]...]  "
    "Don't catch these signals\n"
    "  [no-]sighandler                "
    "Enable signal handler\n"
    "  [no-]deinit-check              "
    "Enable deinit check at exit\n"
    "  [no-]vt-switch                 "
    "Allocate/switch to a new VT\n"
    "  [no-]vt-switching              "
    "Allow Ctrl+Alt+<F?> (EXPERIMENTAL)\n"
    "  [no-]graphics-vt               "
    "Put terminal into graphics mode\n"
    "  [no-]motion-compression        "
    "Mouse motion event compression\n"
    "  mouse-protocol=<protocol>      "
    "Mouse protocol (serial mouse)\n"
    "  [no-]lefty                     "
    "Swap left and right mouse buttons\n"
    "  [no-]cursor                    "
    "Show cursor on start up (default)\n"
    "  bg-none                        "
    "Disable background clear\n"
    "  bg-color=AARRGGBB              "
    "Use background color (hex)\n"
    "  bg-image=<filename>            "
    "Use background image\n"
    "  bg-tile=<filename>             "
    "Use tiled background image\n"
    "  [no-]translucent-windows       "
    "Allow translucent windows\n"
    "  videoram-limit=<amount>        "
    "Limit amount of Video RAM in kb\n"
    "  [no-]matrox-sgram              "
    "Use Matrox SGRAM features\n"
    "  screenshot-dir=<directory>     "
    "Dump screen content on <Print> key presses\n"
    "  fbdebug=<device>               "
    "Use a second frame buffer device for debugging\n"
    "  disable-module=<module_name>   "
    "suppress loading this module\n"
    "\n"
    " Window surface swapping policy:\n"
    "  window-surface-policy=(auto|videohigh|videolow|systemonly|videoonly)\n"
    "     auto:       DirectFB decides depending on hardware.\n"
    "     videohigh:  Swapping system/video with high priority.\n"
    "     videolow:   Swapping system/video with low priority.\n"
    "     systemonly: Window surface is always stored in system memory.\n"
    "     videoonly:  Window surface is always stored in video memory.\n"
    "\n"
    " Desktop buffer mode:\n"
    "  desktop-buffer-mode=(auto|backvideo|backsystem|frontonly)\n"
    "     auto:       DirectFB decides depending on hardware.\n"
    "     backvideo:  Front and back buffer are video only.\n"
    "     backsystem: Back buffer is system only.\n"
    "     frontonly:  There is no back buffer.\n"
    "\n"
    " Force synchronization of vertical retrace:\n"
    "  vsync-after:   Wait for the vertical retrace after flipping.\n"
    "  vsync-none:    disable polling for vertical retrace.\n"
    "\n";

typedef struct {
     char                  *string;
     DFBSurfacePixelFormat  format;
} FormatString;
 
static const FormatString format_strings[] = {
     { "A8",     DSPF_A8 },
     { "ARGB",   DSPF_ARGB },
     { "I420",   DSPF_I420 },
     { "LUT8",   DSPF_LUT8 },
     { "RGB15",  DSPF_RGB15 },
     { "RGB16",  DSPF_RGB16 },
     { "RGB24",  DSPF_RGB24 },
     { "RGB32",  DSPF_RGB32 },
     { "RGB332", DSPF_RGB332 },
     { "UYVY",   DSPF_UYVY },
     { "YUY2",   DSPF_YUY2 },
     { "YV12",   DSPF_YV12 }
};

#define NUM_FORMAT_STRINGS    (sizeof(format_strings)/sizeof(FormatString))

static int
format_string_compare (const void *key,
                       const void *base)
{
  return strcmp ((const char *) key,
                 ((const FormatString *) base)->string);
}

static DFBSurfacePixelFormat
parse_pixelformat( const char *format )
{
     FormatString *format_string;
      
     format_string = bsearch( format, format_strings,
                              NUM_FORMAT_STRINGS, sizeof(FormatString),
                              format_string_compare );
     if (!format_string)
          return DSPF_UNKNOWN;

     return format_string->format;
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

     if (dfb_config->fbdebug_device)
          DFBFREE( dfb_config->fbdebug_device );

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

     dfb_config->layer_bg_color.a         = 0xFF;
     dfb_config->layer_bg_color.r         = 0x24;
     dfb_config->layer_bg_color.g         = 0x50;
     dfb_config->layer_bg_color.b         = 0x9f;
     dfb_config->layer_bg_mode            = DLBM_COLOR;

     dfb_config->banner                   = true;
     dfb_config->debug                    = true;
     dfb_config->deinit_check             = true;
     dfb_config->mmx                      = true;
     dfb_config->sighandler               = true;
     dfb_config->vt_switch                = true;
     dfb_config->show_cursor              = true;
     dfb_config->translucent_windows      = true;
     dfb_config->mouse_motion_compression = true;
     dfb_config->window_policy            = -1;
     dfb_config->buffer_mode              = -1;

     sigemptyset( &dfb_config->dont_catch );
}

const char *dfb_config_usage( void )
{
     return config_usage;
}

DFBResult dfb_config_set( const char *name, const char *value )
{
     if (strcmp (name, "disable-module" ) == 0) {
          if (value) {
	       int n = 0;

	       while (dfb_config->disable_module &&
		      dfb_config->disable_module[n])
		    n++;

	       dfb_config->disable_module = 
		    DFBREALLOC( dfb_config->disable_module,
                                sizeof(char*) * (n + 2) );

	       dfb_config->disable_module[n] = DFBSTRDUP( value );
               dfb_config->disable_module[n+1] = NULL;
          }
          else {
               ERRORMSG("DirectFB/Config 'disable_module': expect module name\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "fbdev" ) == 0) {
          if (value) {
               if (dfb_config->fb_device)
                    DFBFREE( dfb_config->fb_device );
               dfb_config->fb_device = DFBSTRDUP( value );
          }
          else {
               ERRORMSG("DirectFB/Config 'fbdev': No device name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "fbdebug" ) == 0) {
          if (value) {
               if (dfb_config->fbdebug_device)
                    DFBFREE( dfb_config->fbdebug_device );
               dfb_config->fbdebug_device = DFBSTRDUP( value );
          }
          else {
               ERRORMSG("DirectFB/Config 'fbdebug': No device name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "screenshot-dir" ) == 0) {
          if (value) {
               if (dfb_config->screenshot_dir)
                    DFBFREE( dfb_config->screenshot_dir );
               dfb_config->screenshot_dir = DFBSTRDUP( value );
          }
          else {
               ERRORMSG("DirectFB/Config 'screenshot-dir': No directory name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mode" ) == 0) {
          if (value) {
               int width, height;

               if (sscanf( value, "%dx%d", &width, &height ) < 2) {
                    ERRORMSG("DirectFB/Config 'mode': Could not parse mode!\n");
                    return DFB_INVARG;
               }

               dfb_config->mode.width  = width;
               dfb_config->mode.height = height;
          }
          else {
               ERRORMSG("DirectFB/Config 'mode': No mode specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "depth" ) == 0) {
          if (value) {
               int depth;

               if (sscanf( value, "%d", &depth ) < 1) {
                    ERRORMSG("DirectFB/Config 'depth': Could not parse value!\n");
                    return DFB_INVARG;
               }

               dfb_config->mode.depth = depth;
          }
          else {
               ERRORMSG("DirectFB/Config 'depth': No value specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "pixelformat" ) == 0) {
          if (value) {
               DFBSurfacePixelFormat format;

               format = parse_pixelformat( value );
               if (format == DSPF_UNKNOWN) {
                    ERRORMSG("DirectFB/Config 'pixelformat': Could not parse format!\n");
                    return DFB_INVARG;
               }

               dfb_config->mode.format = format;
          }
          else {
               ERRORMSG("DirectFB/Config 'pixelformat': No format specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "videoram-limit" ) == 0) {
          if (value) {
               int limit;
               
               if (sscanf( value, "%d", &limit ) < 1) {
                    ERRORMSG("DirectFB/Config 'videoram-limit': Could not parse value!\n");
                    return DFB_INVARG;
               }

               if (limit < 0)
                    limit = 0;
               
               dfb_config->videoram_limit = PAGE_ALIGN(limit<<10);
          }
          else {
               ERRORMSG("DirectFB/Config 'videoram-limit': No value specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "quiet" ) == 0) {
          dfb_config->quiet = true;
     } else
     if (strcmp (name, "banner" ) == 0) {
          dfb_config->banner = true;
     } else
     if (strcmp (name, "no-banner" ) == 0) {
          dfb_config->banner = false;
     } else
     if (strcmp (name, "debug" ) == 0) {
          dfb_config->debug = true;
     } else
     if (strcmp (name, "no-debug" ) == 0) {
          dfb_config->debug = false;
     } else
     if (strcmp (name, "force-windowed" ) == 0) {
          dfb_config->force_windowed = true;
     } else
     if (strcmp (name, "hardware" ) == 0) {
          dfb_config->software_only = false;
     } else
     if (strcmp (name, "no-hardware" ) == 0) {
          dfb_config->software_only = true;
     } else
     if (strcmp (name, "mmx" ) == 0) {
          dfb_config->mmx = true;
     } else
     if (strcmp (name, "no-mmx" ) == 0) {
          dfb_config->mmx = false;
     } else
     if (strcmp (name, "argb-font" ) == 0) {
          dfb_config->argb_font = true;
     } else
     if (strcmp (name, "no-argb-font" ) == 0) {
          dfb_config->argb_font = false;
     } else
     if (strcmp (name, "sighandler" ) == 0) {
          dfb_config->sighandler = true;
     } else
     if (strcmp (name, "no-sighandler" ) == 0) {
          dfb_config->sighandler = false;
     } else
     if (strcmp (name, "deinit-check" ) == 0) {
          dfb_config->deinit_check = true;
     } else
     if (strcmp (name, "no-deinit-check" ) == 0) {
          dfb_config->deinit_check = false;
     } else
     if (strcmp (name, "cursor" ) == 0) {
          dfb_config->show_cursor = true;
     } else
     if (strcmp (name, "no-cursor" ) == 0) {
          dfb_config->show_cursor = false;
     } else
     if (strcmp (name, "motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = true;
     } else
     if (strcmp (name, "no-motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = false;
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
     if (strcmp (name, "translucent-windows" ) == 0) {
          dfb_config->translucent_windows = true;
     } else
     if (strcmp (name, "no-translucent-windows" ) == 0) {
          dfb_config->translucent_windows = false;
     } else
     if (strcmp (name, "vsync-none" ) == 0) {
          dfb_config->pollvsync_none = true;
     } else
     if (strcmp (name, "vsync-after" ) == 0) {
          dfb_config->pollvsync_after = true;
     } else
     if (strcmp (name, "vt-switch" ) == 0) {
          dfb_config->vt_switch = true;
     } else
     if (strcmp (name, "no-vt-switch" ) == 0) {
          dfb_config->vt_switch = false;
     } else
     if (strcmp (name, "vt-switching" ) == 0) {
          dfb_config->vt_switching = true;
     } else
     if (strcmp (name, "no-vt-switching" ) == 0) {
          dfb_config->vt_switching = false;
     } else
     if (strcmp (name, "graphics-vt" ) == 0) {
          dfb_config->kd_graphics = true;
     } else
     if (strcmp (name, "no-graphics-vt" ) == 0) {
          dfb_config->kd_graphics = false;
     } else
     if (strcmp (name, "bg-none" ) == 0) {
          dfb_config->layer_bg_mode = DLBM_DONTCARE;
     } else
     if (strcmp (name, "bg-image" ) == 0 || strcmp (name, "bg-tile" ) == 0) {
          if (value) {
               if (dfb_config->layer_bg_filename)
                    DFBFREE( dfb_config->layer_bg_filename );
               dfb_config->layer_bg_filename = DFBSTRDUP( value );
               dfb_config->layer_bg_mode = 
                    strcmp (name, "bg-tile" ) ? DLBM_IMAGE : DLBM_TILE;
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
     if (strcmp (name, "dont-catch" ) == 0) {
          if (value) {
               char *signals   = DFBSTRDUP( value );
               char *p, *r, *s = signals;

               while ((r = strtok_r( s, ",", &p ))) {
                    char          *error;
                    unsigned long  signum;

                    dfb_trim( &r );

                    signum = strtoul( r, &error, 10 );

                    if (*error) {
                         ERRORMSG( "DirectFB/Config: Error in dont-catch: "
                                   "'%s'!\n", error );
                         DFBFREE( signals );
                         return DFB_INVARG;
                    }

                    sigaddset( &dfb_config->dont_catch, signum );

                    s = NULL;
               }
               
               DFBFREE( signals );
          }
          else {
               ERRORMSG( "DirectFB/Config: Missing value for dont-catch!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "matrox-sgram" ) == 0) {
          dfb_config->matrox_sgram = true;
     } else
     if (strcmp (name, "no-matrox-sgram" ) == 0) {
          dfb_config->matrox_sgram = false;
     } else
     if (strcmp (name, "sync" ) == 0) {
          dfb_config->sync = true;
     } else
     if (strcmp (name, "no-sync" ) == 0) {
          dfb_config->sync = false;
     } else
     if (strcmp (name, "lefty" ) == 0) {
          dfb_config->lefty = true;
     } else
     if (strcmp (name, "no-lefty" ) == 0) {
          dfb_config->lefty = false;
     }
     else
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

DFBResult dfb_config_init( int *argc, char **argv[] )
{
     DFBResult ret;
     int i;
     char *home = getenv( "HOME" );

     if (dfb_config)
          return DFB_OK;

     config_allocate();

     ret = dfb_config_read( "/etc/directfbrc" );
     if (ret  &&  ret != DFB_IO)
          return ret;

     if (home) {
          char *filename = alloca( strlen(home) + strlen("/.directfbrc") + 1 );

          filename = strcpy( filename, home );
          filename = strcat( filename, "/.directfbrc" );

          ret = dfb_config_read( filename );

          if (ret  &&  ret != DFB_IO)
               return ret;
     }

     if (argc && argv) {
          for (i = 1; i < *argc; i++) {

               if (strcmp ((*argv)[i], "--dfb-help") == 0) {
                    fprintf( stderr, config_usage );
                    exit(1);
               }

               if (strncmp ((*argv)[i], "--dfb:", 6) == 0) {
                    int len = strlen( (*argv)[i] ) - 6;
                    char *arg = (*argv)[i] + 6;

                    while (len) {
                         char *name, *value, *comma;
                         
                         if ((comma = strchr( arg, ',' )) != NULL)
                              *comma = '\0';

                         if (strcmp (arg, "help") == 0) {
                              fprintf( stderr, config_usage );
                              exit(1);
                         }

                         name = DFBSTRDUP( arg );
                         len -= strlen( arg );

                         value = strchr( name, '=' );
                         if (value)
                              *value++ = '\0';

                         ret = dfb_config_set( name, value );

                         DFBFREE( name );

                         if (ret == DFB_OK)
                              (*argv)[i] = NULL;
                         else if (ret != DFB_UNSUPPORTED)
                              return ret;
                         
                         if (comma && len) {
                              arg = comma + 1;
                              len--;
                         }
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

DFBResult dfb_config_read( const char *filename )
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
               dfb_trim( &value );
          }

          dfb_trim( &name );

          if (!*name  ||  *name == '#')
               continue;

          ret = dfb_config_set( name, value );
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

