/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <signal.h>

#include <directfb.h>
#include <fusion/types.h>
#include <fusion/vector.h>

#include <core/coredefs.h>


typedef struct {
     bool                                init;

     DFBDisplayLayerConfig               config;
     DFBColor                            src_key;
     int                                 src_key_index;

     struct {
          DFBDisplayLayerBackgroundMode  mode;
          DFBColor                       color;
          int                            color_index;
          char                          *filename;
     } background;

     DFBWindowStackingClass              stacking;

     DFBColor                           *palette;
     bool                                palette_set;

     int                                 rotate;
} DFBConfigLayer;

typedef enum {
     DCWF_NONE                          = 0x00000000,

     DCWF_CREATE_SURFACE                = 0x00000001,
     DCWF_CREATE_WINDOW                 = 0x00000002,

     DCWF_ALLOCATE_BUFFER               = 0x00000010,

     DCWF_ALL                           = 0x00000013
} DFBConfigWarnFlags;

typedef struct
{
     bool      mouse_motion_compression;          /* use motion compression? */
     char     *mouse_protocol;                    /* mouse protocol */
     char     *mouse_source;                      /* mouse source device name */
     bool      mouse_gpm_source;                  /* mouse source is gpm? */

     int       window_policy;                     /* swapping policy for the
                                                     surface of a window */
     int       buffer_mode;                       /* default buffer mode for
                                                     primary layer */

     bool      pollvsync_after;
     bool      pollvsync_none;

     bool      software_only;                     /* disable hardware acceleration */
     bool      hardware_only;                     /* disable software fallbacks */

     bool      mmx;                               /* mmx support */

     bool      banner;                            /* startup banner */

     bool      force_windowed;                    /* prohibit exclusive modes */

     bool      deinit_check;

     bool      vt_switch;                         /* allocate a new VT */
     int       vt_num;                            /* number of TTY to use or -1
                                                     if the default */
     bool      kd_graphics;                       /* put terminal into graphics
                                                     mode */

     DFBScreenEncoderTVStandards matrox_tv_std;   /* Matrox TV standard */
     int       matrox_cable;                      /* Matrox cable type */
     bool      matrox_sgram;                      /* Use Matrox SGRAM features */
     bool      matrox_crtc2;                      /* Experimental CRTC2 stuff */

     bool      sync;                              /* Do sync() in core_init() */
     bool      vt_switching;                      /* Allow VT switching by
                                                     pressing Ctrl+Alt+<F?> */

     char     *fb_device;                         /* Used framebuffer device,
                                                     e.g. "/dev/fb0" */

     struct {
          int  bus;                               /* PCI Bus */
          int  dev;                               /* PCI Device */
          int  func;                              /* PCI Function */
     } pci;

     bool      lefty;                             /* Left handed mouse, swaps
                                                     left/right mouse buttons */
     bool      no_cursor;                         /* Never create a cursor */
     bool      translucent_windows;               /* Allow translucent
                                                     windows */

     struct {
          int                   width;            /* primary layer width */
          int                   height;           /* primary layer height */
          int                   depth;            /* primary layer depth */
          DFBSurfacePixelFormat format;           /* primary layer format */
     } mode;

     struct {
          int                   width;            /* scaled window width */
          int                   height;           /* scaled window height */
     } scaled;

     int       videoram_limit;                    /* limit amount of video
                                                     memory used by DirectFB */

     char     *screenshot_dir;                    /* dump screen content into
                                                     this directory */

     char     *system;                            /* FBDev, SDL, etc. */

     bool      capslock_meta;                     /* map CapsLock -> Meta */

     bool      block_all_signals;                 /* block all signals */

     int       session;                           /* select multi app world */

     int       primary_layer;                     /* select alternative primary
                                                     display layer */

     bool      force_desktop;                     /* Desktop background is
                                                     the primary surface. */

     bool      linux_input_ir_only;               /* Ignore non-IR devices. */

     struct {
          char *host;                             /* Remote host to connect to. */
          int   port;                             /* Remote port number. */
     } remote;

     char      *wm;                               /* Window manager to use. */

     bool       vt;                               /* Use VT stuff at all? */

     bool       decorations;                      /* Enable window decorations. */

     DFBSurfacePixelFormat font_format;           /* Preferred font format. */

     char      *h3600_device;                     /* H3600 Touchscreen Device */

     char      *mut_device;                       /* MuTouch Device */

     char      *penmount_device;                  /* PenMount Device */

     char      *zytronic_device;                  /* Zytronic Device */

     char      *elo_device;                       /* elo Device */

     int        unichrome_revision;               /* Unichrome hardware
                                                     revision number override */

     bool       dma;                              /* Enable DMA */

     int        agp;                              /* AGP mode */
     int        agpmem_limit;                     /* Limit of AGP memory
                                                     used by DirectFB */
     bool       i8xx_overlay_pipe_b;              /* video overlay output via pixel pipe B */
     bool       primary_only;                     /* tell application only about primary layer */

     bool       thrifty_surface_buffers;          /* don't keep system instance while video instance is alive */
     bool       surface_sentinel;

     DFBConfigLayer  layers[MAX_LAYERS];
     DFBConfigLayer *config_layer;

     DFBSurfaceRenderOptions  render_options;     /* default render options */

     bool       startstop;                        /* Issue StartDrawing/StopDrawing to driver */

     unsigned long video_phys;                    /* Physical base address of video memory */
     unsigned int  video_length;                  /* Size of video memory */
     unsigned long mmio_phys;                     /* Physical base address of MMIO area */
     unsigned int  mmio_length;                   /* Size of MMIO area */
     int           accelerator;                   /* Accelerator ID */

     bool          font_premult;                  /* Use premultiplied data in case of ARGB glyph images */

     FusionVector  linux_input_devices;
     FusionVector  tslib_devices;

     bool          thread_block_signals;          /* Call direct_signals_block_all() in direct_thread_main() startup. */

     bool          linux_input_grab;              /* Grab input devices. */

     bool          autoflip_window;               /* If primary surface is non-flipping, but windowed, flip automatically. */
     bool          software_warn;                 /* Show warnings when doing/dropping software operations. */

     int           surface_shmpool_size;          /* Set the size of the shared memory pool used for
                                                     shared system memory surfaces. */

     unsigned int  system_surface_align_base;     /* If GPU supports system memory, byte alignment for system
                                                     surface's base address (must be a positive power of two
                                                     that is four or greater), or zero for no alignment. */
     unsigned int  system_surface_align_pitch;    /* If GPU supports system memory, byte alignment for system
                                                     surface's pitch (must be a positive power of two), or
                                                     zero for no alignment. */

     bool          no_cursor_updates;             /* Never show the cursor etc. */

     struct {
          DFBConfigWarnFlags  flags;              /* Warn on various actions as window/surface creation. */

          struct {
               DFBDimension   min_size;
          } create_surface;

          struct {
               DFBDimension   min_size;
          } allocate_buffer;
     } warn;

     int           keep_accumulators;             /* Free accumulators above this limit */

     bool          software_trace;

     unsigned int  max_axis_rate;

     bool          cursor_automation;

     bool          wm_fullscreen_updates;

     int           max_font_rows;
     int           max_font_row_width;

     bool          core_sighandler;

     bool          linux_input_force;              /* use linux-input with all system modules */

     u64           resource_id;

     bool          no_singleton;

     bool          x11_borderless;
     DFBPoint      x11_position;

     bool          flip_notify;

     char         *resource_manager;

     u32           input_hub_qid;

     unsigned long font_resource_id;

     unsigned int  flip_notify_max_latency;

     DFBWindowCursorFlags default_cursor_flags;

     bool                 discard_repeat_events;

     bool                 accel1;

     DFBSurfaceID         primary_id;              /* id for primary surface */

     bool                 layers_clear;

     FusionCallExecFlags  call_nodirect;

     u32           input_hub_service_qid;

     bool          cursor_videoonly;
     u64           cursor_resource_id;

     unsigned int  graphics_state_call_limit;

} DFBConfig;

extern DFBConfig DIRECTFB_API *dfb_config;

/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as DirectFB options are stripped out
 * of the array.
 */
DFBResult DIRECTFB_API dfb_config_init( int *argc, char *(*argv[]) );

/*
 * Read configuration options from file. Called by config_init().
 *
 * Returns DFB_IO if config file could not be opened.
 * Returns DFB_UNSUPPORTED if file contains an invalid option.
 * Returns DFB_INVARG if an invalid option assignment is done,
 * e.g. "--desktop-buffer-mode=somethingwrong".
 */
DFBResult DIRECTFB_API dfb_config_read( const char *filename );


/*
 * Set indiviual option. Used by config_init(), config_read() and
 * DirectFBSetOption()
 */
DFBResult DIRECTFB_API dfb_config_set( const char *name, const char *value );

const char DIRECTFB_API *dfb_config_usage( void );

DFBSurfacePixelFormat DIRECTFB_API dfb_config_parse_pixelformat( const char *format );

#endif

