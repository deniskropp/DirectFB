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

#ifndef __CONF_H__
#define __CONF_H__

#include <directfb.h>

#include <core/fusion/fusion_types.h>

typedef struct
{
     DFBDisplayLayerBackgroundMode layer_bg_mode; /* background mode for
                                                     primary layer */
     DFBColor  layer_bg_color;                    /* background color for
                                                     primary layer */
     char     *layer_bg_filename;                 /* background image for
                                                     primary layer */

     bool      mouse_motion_compression;          /* use motion compression? */
     char     *mouse_protocol;                    /* mouse protocol */

     int       window_policy;                     /* swapping policy for the
                                                     surface of a window */
     int       buffer_mode;                       /* default buffer mode for
                                                     primary layer */

     bool      pollvsync_after;
     bool      pollvsync_none;
     bool      software_only;                     /* disable hardware
                                                     acceleration */

     bool      mmx;                               /* mmx support */

     bool      banner;                            /* startup banner */
     bool      quiet;                             /* no output at all
                                                     except debugging */

     bool      debug;                             /* debug output */

     bool      force_windowed;                    /* prohibit exclusive modes */

     bool      sighandler;
     bool      deinit_check;

     bool      vt_switch;                         /* allocate a new VT */
     bool      kd_graphics;                       /* put terminal into graphics
                                                     mode */

     bool      argb_font;                         /* whether to load fontmap
                                                     as argb and not a8 */

     bool      matrox_sgram;                      /* Use Matrox SGRAM features*/
     bool      sync;                              /* Do sync() in core_init() */
     bool      vt_switching;                      /* Allow VT switching by
                                                     pressing Ctrl+Alt+<F?> */

     char     *fb_device;                         /* Used framebuffer device,
                                                     e.g. "/dev/fb0" */

     bool      lefty;                             /* Left handed mouse, swaps
                                                     left/right mouse buttons */
     bool      show_cursor;                       /* Show default mouse cursor
                                                     on start up */
     bool      translucent_windows;               /* Allow translucent
                                                     windows */

     char     *fbdebug_device;                    /* Device for framebuffer
                                                     based debug output */

     struct {
          int                   width;            /* primary layer width */
          int                   height;           /* primary layer height */
          int                   depth;            /* primary layer depth */
          DFBSurfacePixelFormat format;           /* primary layer format */
     } mode;

     int       videoram_limit;                    /* limit amount of video
                                                     memory used by DirectFB */

     sigset_t  dont_catch;                        /* don't catch these signals */

     char     *screenshot_dir;                    /* dump screen content into
                                                     this directory */

     char    **disable_module;                    /* don't load these modules */
} DFBConfig;

extern DFBConfig *dfb_config;

/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as DirectFB options are stripped out
 * of the array.
 */
DFBResult dfb_config_init( int *argc, char **argv[] );

/*
 * Read configuration options from file. Called by config_init().
 *
 * Returns DFB_IO if config file could not be opened.
 * Returns DFB_UNSUPPORTED if file contains an invalid option.
 * Returns DFB_INVARG if an invalid option assignment is done,
 * e.g. "--desktop-buffer-mode=somethingwrong".
 */
DFBResult dfb_config_read( const char *filename );


/*
 * Set indiviual option. Used by config_init(), config_read() and
 * DirectFBSetOption()
 */
DFBResult dfb_config_set( const char *name, const char *value );
#endif

const char *dfb_config_usage( void );
