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

#ifndef __CONF_H__
#define __CONF_H__

#include <directfb.h>

typedef struct
{
     DFBDisplayLayerBackgroundMode layer_bg_mode; /* background mode for
                                                     primary layer */
     DFBColor  layer_bg_color;                    /* background color for
                                                     primary layer */
     char     *layer_bg_filename;                 /* background image for
                                                     primary layer */

     int       mouse_motion_compression;          /* use motion compression? */
     char     *mouse_protocol;                    /* mouse protocol */

     int       window_policy;                     /* swapping policy for the
                                                     surface of a window */
     int       buffer_mode;                       /* default buffer mode for
                                                     primary layer */

     int       pollvsync_after;
     int       pollvsync_none;
     int       software_only;                     /* disable hardware
                                                     acceleration */

     int       no_mmx;                            /* disable mmx support */

     int       no_banner;                         /* disable banner */
     int       quiet;                             /* no output at all
                                                     except debugging */

     int       no_debug;                          /* disable debug output */

     int       force_windowed;                    /* prohibit exclusive modes */

     int       no_sighandler;
     int       no_deinit_check;

     int       no_vt_switch;                      /* don't allocate a new VT */
     int       kd_graphics;                       /* put terminal into graphics
                                                     mode */

     int       argb_font;                         /* whether to load fontmap
                                                     as argb and not a8 */

     int       matrox_sgram;                      /* Use Matrox SGRAM features*/
     int       sync;                              /* Do sync() in core_init() */
     int       vt_switching;                      /* Allow VT switching by
                                                     pressing Ctrl+Alt+<F?> */

     char     *fb_device;                         /* Used framebuffer device,
                                                     e.g. "/dev/fb0" */
} DFBConfig;

extern DFBConfig *dfb_config;

/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as DirectFB options are stripped out
 * of the array.
 *
 * Exits if "--help" is passed, FIXME!
 */
DFBResult config_init( int *argc, char **argv[] );

/*
 * Read configuration options from file. Called by config_init().
 *
 * Returns DFB_IO if config file could not be opened.
 * Returns DFB_UNSUPPORTED if file contains an invalid option.
 * Returns DFB_INVARG if an invalid option assignment is done,
 * e.g. "--desktop-buffer-mode=somethingwrong".
 */
DFBResult config_read( const char *filename );


/*
 * Set indiviual option. Used by config_init(), config_read() and
 * DirectFBSetOption()
 */
DFBResult config_set( const char *name, const char *value );
#endif
