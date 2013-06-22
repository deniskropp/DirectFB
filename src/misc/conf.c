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

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/conf.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/conf.h>
#include <fusion/vector.h>

#if DIRECTFB_BUILD_VOODOO
#include <voodoo/conf.h>
#endif

#if !DIRECTFB_BUILD_PURE_VOODOO
#include <core/surface.h>
#endif

#include <misc/conf.h>

D_DEBUG_DOMAIN( DirectFB_Config, "DirectFB/Config", "Runtime configuration options for DirectFB" );

DFBConfig *dfb_config = NULL;

/* C99 States that a string cannot be longer than 4095 chars so we have to split the usage up */

static char *config_usage;
static const char *config_usage_strings[]  = {
     "DirectFB version " DIRECTFB_VERSION "\n"
     "\n"
     " --dfb-help                      Output DirectFB usage information and exit\n"
     " --dfb:<option>[,<option>]...    Pass options to DirectFB (see below)\n"
     "\n"
     "DirectFB options:\n"
     "\n",
     "  system=<system>                Specify the system (FBDev, SDL, etc.)\n"
     "  fbdev=<device>                 Open <device> instead of /dev/fb0\n"
     "  busid=<id>                     Specify the bus location of the graphics card (default 1:0:0)\n"
     "  mode=<width>x<height>          Set the default resolution\n"
     "  scaled=<width>x<height>        Scale the window to this size for 'force-windowed' apps\n"
     "  depth=<pixeldepth>             Set the default pixel depth\n"
     "  pixelformat=<pixelformat>      Set the default pixel format\n"
     "  primary-id=<surface-id>        Set ID of primary surface to use\n"
     "  surface-shmpool-size=<kb>      Set the size of the shared memory pool used\n"
     "                                 for shared system memory surfaces.\n"
     "  system-surface-base-alignment=<byte alignment>\n"
     "                                 If GPU supports system memory, sets the byte alignment for\n"
     "                                 system memory based surface's base address (value must be a\n"
     "                                 positive power of two that is four or greater), or zero for\n"
     "                                 no alignment. Aligning the base address (along with the\n"
     "                                 pitch) allows the data to travel more efficiently through\n"
     "                                 the CPU and memory bus to increase performance, and meet GPU\n"
     "                                 requirements (if any). Default is 0 (no alignment).\n"
     "  system-surface-pitch-alignment=<byte alignment>\n"
     "                                 If GPU supports system memory, sets the pitch alignment for\n"
     "                                 system memory based surface's pitch (value must be a positive\n"
     "                                 power of two), or zero for no alignment. Default is 0.\n"
     "  session=<num>                  Select multi app world (zero based, -1 = new)\n"
     "  remote=<host>[:<port>]         Set remote host and port to connect to\n"
     "  primary-layer=<id>             Select an alternative primary layer\n"
     "  primary-only                   Tell application only about the primary layer\n"
     "  resource-id=<id>               Use resource id for surfaces if not specified by application\n"
     "  [no-]banner                    Show DirectFB Banner on startup\n"
     "  [no-]surface-sentinel          Enable surface sentinels at the end of chunks in video memory\n"
     "  force-windowed                 Primary surface always is a window\n"
     "  force-desktop                  Primary surface is the desktop background\n"
     "  [no-]hardware                  Enable/disable hardware acceleration\n"
     "  [no-]software                  Enable/disable software fallbacks\n"
     "  [no-]software-warn             Show warnings when doing/dropping software operations\n"
     "  [no-]software-trace            Show every stage of the software rendering pipeline\n"
     "  [no-]always-indirect           Use purely indirect Flux calls (for secure master)\n"
     "  [no-]always-flush-callbuffer   Flush call buffer upon commit, effectively disabling it\n"
     "  [no-]layers-fps=[<ms>]         Print FPS of layers being updated, optional interval (default 1000)\n"
     "  [no-]gfxcard-stats=[<ms>]      Print GPU usage statistics periodically (default 1000)\n"
     "  screen-frame-interval=<us>     Set default value for screen refresh interval if not encoder defined\n"
     "  [no-]dma                       Enable DMA acceleration\n"
     "  [no-]sync                      Do `sync()' (default=no)\n",
#ifdef USE_MMX
     "  [no-]mmx                       Enable mmx support\n"
#endif
     "  [no-]agp[=<mode>]              Enable AGP support\n"
     "  [no-]thrifty-surface-buffers   Free sysmem instance on xfer to video memory\n"
     "  font-format=<pixelformat>      Set the preferred font format\n"
     "  [no-]font-premult              Enable/disable premultiplied glyph images in ARGB format\n"
     "  [no-]deinit-check              Enable deinit check at exit\n"
     "  [no-]core-sighandler           Enable/disable core signal handler (for emergency shutdowns)\n"
     "  block-all-signals              Block all signals\n"
     "  [no-]vt-switch                 Allocate/switch to a new VT\n"
     "  vt-num=<num>                   Use given VT instead of current/new one\n"
     "  [no-]vt-switching              Allow Ctrl+Alt+<F?> (EXPERIMENTAL)\n"
     "  [no-]graphics-vt               Put terminal into graphics mode\n"
     "  [no-]vt                        Use VT handling code at all?\n"
     "  mouse-source=<device>          Mouse device for serial mouse\n"
     "  [no-]mouse-gpm-source          Enable mouse input repeated by GPM\n"
     "  [no-]motion-compression        Mouse motion event compression\n"
     "  mouse-protocol=<protocol>      Mouse protocol\n"
     "  [no-]lefty                     Swap left and right mouse buttons\n"
     "  [no-]capslock-meta             Map the CapsLock key to Meta\n"
     "  linux-input-ir-only            Ignore all non-IR Linux Input devices\n"
     "  [no-]linux-input-grab          Grab Linux Input devices?\n"
     "  [no-]linux-input-force         Force using linux-input with all system modules\n"
     "  [no-]cursor                    Never create a cursor or handle it\n"
     "  [no-]cursor-automation         Automated cursor show/hide for windowed primary surfaces\n"
     "  [no-]cursor-updates            Never show a cursor, but still handle it\n"
     "  [no-]cursor-videoonly          Make the cursor a video only surface\n"
     "  cursor-resource-id=<id>        Specify a resource id for the cursor surface\n"
     "  wm=<wm>                        Window manager module ('default' or 'unique')\n"
     "  init-layer=<id>                Initialize layer with ID (following layer- options apply)\n"
     "  [no-]layers-clear              Clear layer surface buffers after creation\n"
     "  [no-]surface-clear             Clear all surface buffers after creation\n"
     "  layer-size=<width>x<height>    Set the pixel resolution\n"
     "  layer-format=<pixelformat>     Set the pixel format\n"
     "  layer-depth=<pixeldepth>       Set the pixel depth\n"
     "  layer-buffer-mode=(auto|triple|backvideo|backsystem|frontonly|windows)\n"
     "  layer-bg-none                  Disable background clear\n"
     "  layer-bg-color=AARRGGBB        Use background color (hex)\n"
     "  layer-bg-color-index=<index>   Use background color index (decimal)\n"
     "  layer-bg-image=<filename>      Use background image\n"
     "  layer-bg-tile=<filename>       Use tiled background image\n"
     "  layer-src-key=AARRGGBB         Enable color keying (hex)\n"
     "  layer-palette-<index>=AARRGGBB Set palette entry at index (hex)\n"
     "  layer-rotate=<degree>          Set the layer rotation for double buffer mode (0,90,180,270)\n"
     "  image-format=<pixelformat>     Set the pixel format for loading images\n"
     "  [no-]wm-fullscreen-updates     Force fullscreen updates in window manager\n"
     "  [no-]smooth-upscale            Enable/disable smooth upscaling per default\n"
     "  [no-]smooth-downscale          Enable/disable smooth downscaling per default\n"
     "  [no-]translucent-windows       Allow translucent windows\n"
     "  [no-]decorations               Enable window decorations (if supported by wm)\n"
     "  [no-]startstop                 Issue StartDrawing/StopDrawing to driver\n"
     "  [no-]autoflip-window           Auto flip non-flipping windowed primary surfaces\n"
     "  [no-]discard-repeat-events     Discard repeat events (option per application)\n"
     "  [no-]gfx-emit-early            Early emit GFX commands to prevent being IDLE\n"
     "  [no-]flip-notify               Use FlipNotify for remote display\n"
     "  flip-notify-max-latency=<ms>   Set maximum FlipNotify latency (ms from Flip to Notify, default 200)\n"
     "  videoram-limit=<amount>        Limit amount of Video RAM in kb\n"
     "  agpmem-limit=<amount>          Limit amount of AGP memory in kb\n"
     "  screenshot-dir=<directory>     Dump screen content on <Print> key presses\n"
     "  video-phys=<hexaddress>        Physical start of video memory (devmem system)\n"
     "  video-length=<bytes>           Length of video memory (devmem system)\n"
     "  mmio-phys=<hexaddress>         Physical start of MMIO area (devmem system)\n"
     "  mmio-length=<bytes>            Length of MMIO area (devmem system)\n"
     "  accelerator=<id>               Accelerator ID selecting graphics driver (devmem system)\n"
     "  font-resource-id=<id>          Resource ID to use for font cache row surfaces\n"
     "  resource-manager=<impl>        Use this resource manager implementation\n"
     "  [no-]task-manager              Use experimental task manager (default: no)\n"
     "  software-cores=<num>           Set number of threads to use for software rendering\n"
     "\n",
     "  x11-borderless[=<x>.<y>]       Disable X11 window borders, optionally position window\n"
     "  [no-]matrox-sgram              Use Matrox SGRAM features\n"
     "  [no-]matrox-crtc2              Experimental Matrox CRTC2 support\n"
     "  matrox-tv-standard=(pal|ntsc|pal-60)\n"
     "                                 Matrox TV standard (default=pal)\n"
     "  matrox-cable-type=(composite|scart-rgb|scart-composite)\n"
     "                                 Matrox cable type (default=composite)\n"
     "  h3600-device=<device>          Use this device for the H3600 TS driver\n"
     "  mut-device=<device>            Use this device for the MuTouch driver\n"
     "  zytronic-device=<device>       Use this device for the Zytronic driver\n"
     "  elo-device=<device>            Use this device for the Elo driver\n"
     "  penmount-device=<device>       Use this device for the PenMount driver\n"
     "  linux-input-devices=<device>[[,<device>]...]\n"
     "                                 Use these devices for the Linux Input driver\n"
     "  tslib-devices=<device>[[,<device>]...]\n"
     "                                 Use these devices for the tslib driver\n"
     "  unichrome-revision=<rev>       Override unichrome hardware revision\n"
     "  i8xx_overlay_pipe_b            Redirect videolayer to pixelpipe B\n"
     "  include=<config file>          Include the specified file, relative to the current file\n"
     "\n"
     "  max-font-rows=<number>         Maximum number of glyph cache rows (total for all fonts)\n"
     "  max-font-row-width=<pixels>    Maximum width of glyph cache row surface\n"
     "  graphics-state-call-limit=<n>  Set FusionCall quota for graphics state object (default 5000)\n"
     "\n",
     " Window surface swapping policy:\n"
     "  window-surface-policy=(auto|videohigh|videolow|systemonly|videoonly)\n"
     "     auto:       DirectFB decides depending on hardware.\n"
     "     videohigh:  Swapping system/video with high priority.\n"
     "     videolow:   Swapping system/video with low priority.\n"
     "     systemonly: Window surface is always stored in system memory.\n"
     "     videoonly:  Window surface is always stored in video memory.\n"
     "\n"
     " Desktop buffer mode:\n"
     "  desktop-buffer-mode=(auto|triple|backvideo|backsystem|frontonly|windows)\n"
     "     auto:       DirectFB decides depending on hardware.\n"
     "     triple:     Triple buffering (video only).\n"
     "     backvideo:  Front and back buffer are video only.\n"
     "     backsystem: Back buffer is system only.\n"
     "     frontonly:  There is no back buffer.\n"
     "     windows:    Special mode with window buffers directly displayed.\n"
     "\n"
     " Force synchronization of vertical retrace:\n"
     "  vsync-after:   Wait for the vertical retrace after flipping.\n"
     "  vsync-none:    disable polling for vertical retrace.\n"
     "\n"};

/**********************************************************************************************************************/

/* serial mouse device names */
#define DEV_NAME     "/dev/mouse"
#define DEV_NAME_GPM "/dev/gpmdata"

/**********************************************************************************************************************/

DFBSurfacePixelFormat
dfb_config_parse_pixelformat( const char *format )
{
     int    i;
     size_t length = strlen(format);

     for (i=0; dfb_pixelformat_names[i].format != DSPF_UNKNOWN; i++) {
          if (!direct_strcasecmp( format, dfb_pixelformat_names[i].name ))
               return dfb_pixelformat_names[i].format;
     }

     for (i=0; dfb_pixelformat_names[i].format != DSPF_UNKNOWN; i++) {
          if (!direct_strncasecmp( format, dfb_pixelformat_names[i].name, length ))
               return dfb_pixelformat_names[i].format;
     }

     return DSPF_UNKNOWN;
}

/**********************************************************************************************************************/

static void
print_config_usage( void )
{
    int i;

    for (i=0; i<D_ARRAY_SIZE(config_usage_strings); ++i)
        fprintf(stderr,"%s",(config_usage_strings[i]));

    fprintf( stderr, "%s%s", fusion_config_usage, direct_config_usage );
}

static DFBResult
parse_args( const char *args )
{
     int   len = strlen(args);
     char *buf, *tmp;

     tmp = D_MALLOC( len + 1 );
     if (!tmp)
          return D_OOM();

     buf = tmp;

     direct_memcpy( buf, args, len + 1 );

     while (buf && buf[0]) {
          DFBResult  ret;
          char      *value;
          char      *next;

          if ((next = strchr( buf, ',' )) != NULL)
               *next++ = '\0';

          if (strcmp (buf, "help") == 0) {
               print_config_usage();
               exit(1);
          }

          if (strcmp (buf, "memcpy=help") == 0) {
               direct_print_memcpy_routines();
               exit(1);
          }

          if ((value = strchr( buf, '=' )) != NULL)
               *value++ = '\0';

          ret = dfb_config_set( buf, value );
          switch (ret) {
               case DFB_OK:
                    break;
               case DFB_UNSUPPORTED:
                    D_ERROR( "DirectFB/Config: Unknown option '%s'!\n", buf );
                    break;
               default:
                    D_FREE( tmp );
                    return ret;
          }

          buf = next;
     }

     D_FREE( tmp );

     return DFB_OK;
}

static void config_values_parse( FusionVector *vector, const char *arg )
{
     char *values = D_STRDUP( arg );
     char *s      = values;
     char *r, *p  = NULL;

     if (!values) {
          D_OOM();
          return;
     }

     while ((r = direct_strtok_r( s, ",", &p ))) {
          direct_trim( &r );

          r = D_STRDUP( r );
          if (!r)
               D_OOM();
          else
               fusion_vector_add( vector, r );

          s = NULL;
     }

     D_FREE( values );
}

static void config_values_free( FusionVector *vector )
{
     char *value;
     int   i;

     fusion_vector_foreach (value, i, *vector)
          D_FREE( value );

     fusion_vector_destroy( vector );
     fusion_vector_init( vector, 2, NULL );
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

/*
 * The following function isn't used because the configuration should
 * only go away if the application is completely terminated. In that case
 * the memory is freed anyway.
 */

#if 0
static void config_cleanup( void )
{
     if (!dfb_config) {
          D_BUG("config_cleanup() called with no config allocated!");
          return;
     }

     if (dfb_config->fb_device)
          D_FREE( dfb_config->fb_device );

     if (dfb_config->layer_bg_filename)
          D_FREE( dfb_config->layer_bg_filename );

     D_FREE( dfb_config );
     dfb_config = NULL;
}
#endif

/*
 * allocates config and fills it with defaults
 */
static void config_allocate( void )
{
    int i, usage_length = 0;

     if (dfb_config)
          return;

     dfb_config = (DFBConfig*) calloc( 1, sizeof(DFBConfig) );


     for (i=0; i<D_ARRAY_SIZE(config_usage_strings); ++i)
         usage_length += strlen(config_usage_strings[i]);

     config_usage = (char*) malloc( usage_length );

     for (i=0; i<D_ARRAY_SIZE(config_usage_strings); ++i)
     {
         usage_length = strlen(config_usage_strings[i]);
         strncpy(config_usage,config_usage_strings[i],usage_length);
         config_usage += usage_length;
     }

     for (i=0; i<D_ARRAY_SIZE(dfb_config->layers); i++) {
          dfb_config->layers[i].src_key_index          = -1;

          dfb_config->layers[i].background.color.a     = 0;
          dfb_config->layers[i].background.color.r     = 0;
          dfb_config->layers[i].background.color.g     = 0;
          dfb_config->layers[i].background.color.b     = 0;
          dfb_config->layers[i].background.color_index = -1;
          dfb_config->layers[i].background.mode        = DLBM_COLOR;
     }

     dfb_config->layers[0].init           = true;
     dfb_config->layers[0].stacking       = (1 << DWSC_UPPER)  |
                                            (1 << DWSC_MIDDLE) |
                                            (1 << DWSC_LOWER);


     dfb_config->pci.bus                  = 1;
     dfb_config->pci.dev                  = 0;
     dfb_config->pci.func                 = 0;

     dfb_config->banner                   = true;
     dfb_config->deinit_check             = true;
     dfb_config->mmx                      = true;
     dfb_config->vt                       = true;
     dfb_config->vt_switch                = true;
     dfb_config->vt_num                   = -1;
     dfb_config->vt_switching             = true;
     dfb_config->kd_graphics              = true;
     dfb_config->translucent_windows      = true;
     dfb_config->font_premult             = true;
     dfb_config->mouse_motion_compression = false;
     dfb_config->mouse_gpm_source         = false;
     dfb_config->mouse_source             = D_STRDUP( DEV_NAME );
     dfb_config->linux_input_grab         = false;
     dfb_config->linux_input_force        = false;
     dfb_config->window_policy            = -1;
     dfb_config->buffer_mode              = -1;
     dfb_config->wm                       = D_STRDUP( "default" );
     dfb_config->decorations              = true;
     dfb_config->unichrome_revision       = -1;
     dfb_config->dma                      = false;
     dfb_config->agp                      = 0;
     dfb_config->matrox_tv_std            = DSETV_PAL;
     dfb_config->i8xx_overlay_pipe_b      = false;
     dfb_config->surface_shmpool_size     = 64 * 1024 * 1024;
     dfb_config->system_surface_align_base  = 0;
     dfb_config->system_surface_align_pitch = 0;
     dfb_config->keep_accumulators        = 1024;
     dfb_config->font_format              = DSPF_A8;
     dfb_config->cursor_automation        = true;
     dfb_config->layers_clear             = true;
     dfb_config->wm_fullscreen_updates    = false;

     /* default to x11 if DISPLAY is set, or drm/fbdev if permitted */
     if (getenv( "DISPLAY" ))
          dfb_config->system = D_STRDUP( "x11" );
     else if (!access( "/dev/dri/card0", O_RDWR ))
          dfb_config->system = D_STRDUP( "drmkms" );
     else if (!access( "/dev/fb0", O_RDWR ))
          dfb_config->system = D_STRDUP( "fbdev" );

     /* default to no-vt-switch if we don't have root privileges */
     if (direct_geteuid())
          dfb_config->vt_switch = false;

     fusion_vector_init( &dfb_config->linux_input_devices, 2, NULL );
     fusion_vector_init( &dfb_config->tslib_devices, 2, NULL );


     dfb_config->max_font_rows      = 99;
     dfb_config->max_font_row_width = 2048;

     dfb_config->core_sighandler    = true;

     dfb_config->flip_notify_max_latency = 200;
     dfb_config->screen_frame_interval   = 16666;

     dfb_config->graphics_state_call_limit = 5000;
}

const char *dfb_config_usage( void )
{
     return config_usage;
}

DFBResult dfb_config_set( const char *name, const char *value )
{
     if (strcmp (name, "system" ) == 0) {
          if (value) {
               if (dfb_config->system)
                    D_FREE( dfb_config->system );
               dfb_config->system = D_STRDUP( value );
          }
          else {
               D_ERROR("DirectFB/Config 'system': No system specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "wm" ) == 0) {
          if (value) {
               if (dfb_config->wm)
                    D_FREE( dfb_config->wm );
               dfb_config->wm = D_STRDUP( value );
          }
          else {
               D_ERROR("DirectFB/Config 'wm': No window manager specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "fbdev" ) == 0) {
          if (value) {
               if (dfb_config->fb_device)
                    D_FREE( dfb_config->fb_device );
               dfb_config->fb_device = D_STRDUP( value );
          }
          else {
               D_ERROR("DirectFB/Config 'fbdev': No device name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "busid") == 0 || strcmp (name, "pci-id") == 0) {
          if (value) {
               int bus, dev, func;

               if (direct_sscanf( value, "%d:%d:%d", &bus, &dev, &func ) != 3) {
                    D_ERROR( "DirectFB/Config 'busid': Could not parse busid!\n");
                    return DFB_INVARG;
               }

               dfb_config->pci.bus  = bus;
               dfb_config->pci.dev  = dev;
               dfb_config->pci.func = func;
          }
     } else
     if (strcmp (name, "screenshot-dir" ) == 0) {
          if (value) {
               if (dfb_config->screenshot_dir)
                    D_FREE( dfb_config->screenshot_dir );
               dfb_config->screenshot_dir = D_STRDUP( value );
          }
          else {
               D_ERROR("DirectFB/Config 'screenshot-dir': No directory name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "scaled" ) == 0) {
          if (value) {
               int width, height;

               if (direct_sscanf( value, "%dx%d", &width, &height ) < 2) {
                    D_ERROR("DirectFB/Config 'scaled': Could not parse size!\n");
                    return DFB_INVARG;
               }

               dfb_config->scaled.width  = width;
               dfb_config->scaled.height = height;
          }
          else {
               D_ERROR("DirectFB/Config 'scaled': No size specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "primary-id" ) == 0) {
          if (value) {
               unsigned int id;

               if (sscanf( value, "%u", &id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->primary_id = id;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No id specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "primary-layer" ) == 0) {
          if (value) {
               int id;

               if (direct_sscanf( value, "%d", &id ) < 1) {
                    D_ERROR("DirectFB/Config 'primary-layer': Could not parse id!\n");
                    return DFB_INVARG;
               }

               dfb_config->primary_layer = id;
          }
          else {
               D_ERROR("DirectFB/Config 'primary-layer': No id specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "primary-only" ) == 0) {
          dfb_config->primary_only = true;
     } else
     if (strcmp (name, "resource-id" ) == 0) {
          if (value) {
               unsigned long long resource_id;

               if (direct_sscanf( value, "%llu", &resource_id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->resource_id = resource_id;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No id specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "font-format" ) == 0) {
          if (value) {
               DFBSurfacePixelFormat format;

               format = dfb_config_parse_pixelformat( value );
               if (format == DSPF_UNKNOWN) {
                    D_ERROR("DirectFB/Config 'font-format': Could not parse format!\n");
                    return DFB_INVARG;
               }

               dfb_config->font_format = format;
          }
          else {
               D_ERROR("DirectFB/Config 'font-format': No format specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "font-premult" ) == 0) {
          dfb_config->font_premult = true;
     } else
     if (strcmp (name, "no-font-premult" ) == 0) {
          dfb_config->font_premult = false;
     } else
     if (!strcmp( name, "surface-shmpool-size" )) {
          if (value) {
               int size_kb;

               if (direct_sscanf( value, "%d", &size_kb ) < 1) {
                    D_ERROR( "DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->surface_shmpool_size = size_kb * 1024;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "system-surface-base-alignment" ) == 0) {
          if (value) {
               char *error;
               unsigned long base_align;

               base_align = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in decimal value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               /* If non-zero, value must be a positive power of two that is four or greater. */
               if (base_align && (base_align < 4 || (base_align & (base_align-1)) != 0)) {
                   D_ERROR( "DirectFB/Config '%s': If non-zero, value must be a positive power-of-two "
                            "that is four or greater!\n", name );
                   return DFB_INVARG;
               }

               dfb_config->system_surface_align_base = base_align;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "system-surface-pitch-alignment" ) == 0) {
          if (value) {
               char *error;
               unsigned long pitch_align;

               pitch_align = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in decimal value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               /* If non-zero, value must be a positive power of two. */
               if (pitch_align && (pitch_align == 1 || (pitch_align & (pitch_align-1)) != 0)) {
                   D_ERROR( "DirectFB/Config '%s': If non-zero, value must be a positive power-of-two!\n", name );
                   return DFB_INVARG;
               }

               dfb_config->system_surface_align_pitch = pitch_align;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "session" ) == 0) {
          if (value) {
               int session;

               if (direct_sscanf( value, "%d", &session ) < 1) {
                    D_ERROR("DirectFB/Config 'session': Could not parse value!\n");
                    return DFB_INVARG;
               }

               dfb_config->session = session;
          }
          else {
               D_ERROR("DirectFB/Config 'session': No value specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "remote" ) == 0) {
          if (value) {
               char *colon;

               colon = strchr( value, ':' );
               if (colon) {
                    int len  = (long) colon - (long) value;
                    int port = 0;

                    if (direct_sscanf( colon + 1, "%d", &port ) < 1) {
                         D_ERROR("DirectFB/Config 'remote': "
                                 "Could not parse value (format is <host>[:<port>])!\n");
                         return DFB_INVARG;
                    }

                    if (dfb_config->remote.host)
                         D_FREE( dfb_config->remote.host );

                    dfb_config->remote.host = D_MALLOC( len+1 );
                    dfb_config->remote.port = port;

                    direct_snputs( dfb_config->remote.host, value, len+1 );
               }
               else {
                    if (dfb_config->remote.host)
                         D_FREE( dfb_config->remote.host );

                    dfb_config->remote.host = D_STRDUP( value );
                    dfb_config->remote.port = 0;
               }
          }
          else {
               if (dfb_config->remote.host)
                    D_FREE( dfb_config->remote.host );

               dfb_config->remote.host = D_STRDUP( "" );
               dfb_config->remote.port = 0;
          }
     } else
     if (strcmp (name, "videoram-limit" ) == 0) {
          if (value) {
               int limit;

               if (direct_sscanf( value, "%d", &limit ) < 1) {
                    D_ERROR("DirectFB/Config 'videoram-limit': Could not parse value!\n");
                    return DFB_INVARG;
               }

               if (limit < 0)
                    limit = 0;

               dfb_config->videoram_limit = limit << 10;
          }
          else {
               D_ERROR("DirectFB/Config 'videoram-limit': No value specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "keep-accumulators" ) == 0) {
          if (value) {
               int limit;

               if (direct_sscanf( value, "%d", &limit ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->keep_accumulators = limit;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No value specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "banner" ) == 0) {
          dfb_config->banner = true;
     } else
     if (strcmp (name, "no-banner" ) == 0) {
          dfb_config->banner = false;
     } else
     if (strcmp (name, "surface-sentinel" ) == 0) {
          dfb_config->surface_sentinel = true;
     } else
     if (strcmp (name, "no-surface-sentinel" ) == 0) {
          dfb_config->surface_sentinel = false;
     } else
     if (strcmp (name, "force-windowed" ) == 0) {
          dfb_config->force_windowed = true;
     } else
     if (strcmp (name, "force-desktop" ) == 0) {
          dfb_config->force_desktop = true;
     } else
     if (strcmp (name, "hardware" ) == 0) {
          dfb_config->software_only = false;
     } else
     if (strcmp (name, "no-hardware" ) == 0) {
          dfb_config->software_only = true;
     } else
     if (strcmp (name, "software" ) == 0) {
          dfb_config->hardware_only = false;
     } else
     if (strcmp (name, "no-software" ) == 0) {
          dfb_config->hardware_only = true;
     } else
     if (strcmp (name, "software-warn" ) == 0) {
          dfb_config->software_warn = true;
     } else
     if (strcmp (name, "no-software-warn" ) == 0) {
          dfb_config->software_warn = false;
     } else
     if (strcmp (name, "software-trace" ) == 0) {
          dfb_config->software_trace = true;
     } else
     if (strcmp (name, "no-software-trace" ) == 0) {
          dfb_config->software_trace = false;
     } else
     if (strcmp (name, "always-indirect" ) == 0) {
          dfb_config->call_nodirect = FCEF_NODIRECT;
     } else
     if (strcmp (name, "no-always-indirect" ) == 0) {
          dfb_config->call_nodirect = FCEF_NONE;
     } else
     if (strcmp (name, "warn" ) == 0 || strcmp (name, "no-warn" ) == 0) {
          /* Enable/disable all at once by default. */
          DFBConfigWarnFlags flags = DMT_ALL;

          /* Find out the specific message type being configured. */
          if (value) {
               char *opt = strchr( value, ':' );

               if (opt)
                    opt++;

               if (!strncmp( value, "create-surface", 14 )) {
                    flags = DCWF_CREATE_SURFACE;

                    if (opt)
                         direct_sscanf( opt, "%dx%d",
                                 &dfb_config->warn.create_surface.min_size.w,
                                 &dfb_config->warn.create_surface.min_size.h );
               } else
               if (!strncmp( value, "create-window", 13 )) {
                    flags = DCWF_CREATE_WINDOW;
               } else
               if (!strncmp( value, "allocate-buffer", 15 )) {
                    flags = DCWF_ALLOCATE_BUFFER;

                    if (opt)
                         direct_sscanf( opt, "%dx%d",
                                 &dfb_config->warn.allocate_buffer.min_size.w,
                                 &dfb_config->warn.allocate_buffer.min_size.h );
               }
               else {
                    D_ERROR( "DirectFB/Config '%s': Unknown warning type '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }

          /* Set/clear the corresponding flag in the configuration. */
          if (name[0] == 'w')
               dfb_config->warn.flags |= flags;
          else
               dfb_config->warn.flags &= ~flags;
     } else
     if (strcmp (name, "dma" ) == 0) {
          dfb_config->dma = true;
     } else
     if (strcmp (name, "no-dma" ) == 0) {
          dfb_config->dma = false;
     } else
     if (strcmp (name, "mmx" ) == 0) {
          dfb_config->mmx = true;
     } else
     if (strcmp (name, "no-mmx" ) == 0) {
          dfb_config->mmx = false;
     } else
     if (strcmp (name, "agp" ) == 0) {
          if (value) {
               int mode;

               if (direct_sscanf( value, "%d", &mode ) < 1 || mode < 0 || mode > 8) {
                    D_ERROR( "DirectFB/Config 'agp': "
                             "invalid agp mode '%s'!\n", value );
                    return DFB_INVARG;
               }

               dfb_config->agp = mode;
          }
          else {
               dfb_config->agp = 8; /* maximum possible */
          }
     } else
     if (strcmp (name, "thrifty-surface-buffers" ) == 0) {
          dfb_config->thrifty_surface_buffers = true;
     } else
     if (strcmp (name, "no-thrifty-surface-buffers" ) == 0) {
          dfb_config->thrifty_surface_buffers = false;
     } else
     if (strcmp (name, "no-agp" ) == 0) {
          dfb_config->agp = 0;
     } else
     if (strcmp (name, "agpmem-limit" ) == 0) {
          if (value) {
               int limit;

               if (direct_sscanf( value, "%d", &limit ) < 1) {
                    D_ERROR( "DirectFB/Config 'agpmem-limit': "
                             "Could not parse value!\n" );
                    return DFB_INVARG;
               }

               if (limit < 0)
                    limit = 0;

               dfb_config->agpmem_limit = limit << 10;
          }
          else {
               D_ERROR("DirectFB/Config 'agpmem-limit': No value specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "vt" ) == 0) {
          dfb_config->vt = true;
     } else
     if (strcmp (name, "no-vt" ) == 0) {
          dfb_config->vt = false;
     } else
     if (strcmp (name, "block-all-signals" ) == 0) {
          dfb_config->block_all_signals = true;
     } else
     if (strcmp (name, "core-sighandler" ) == 0) {
          dfb_config->core_sighandler = true;
     } else
     if (strcmp (name, "no-core-sighandler" ) == 0) {
          dfb_config->core_sighandler = false;
     } else
     if (strcmp (name, "deinit-check" ) == 0) {
          dfb_config->deinit_check = true;
     } else
     if (strcmp (name, "no-deinit-check" ) == 0) {
          dfb_config->deinit_check = false;
     } else
     if (strcmp (name, "cursor" ) == 0) {
          dfb_config->no_cursor = false;
     } else
     if (strcmp (name, "no-cursor" ) == 0) {
          dfb_config->no_cursor = true;
     } else
     if (strcmp (name, "cursor-automation" ) == 0) {
          dfb_config->cursor_automation = true;
     } else
     if (strcmp (name, "no-cursor-automation" ) == 0) {
          dfb_config->cursor_automation = false;
     } else
     if (strcmp (name, "wm-fullscreen-updates" ) == 0) {
          dfb_config->wm_fullscreen_updates = true;
     } else
     if (strcmp (name, "no-wm-fullscreen-updates" ) == 0) {
          dfb_config->wm_fullscreen_updates = false;
     } else
     if (strcmp (name, "cursor-updates" ) == 0) {
          dfb_config->no_cursor_updates = false;
     } else
     if (strcmp (name, "no-cursor-updates" ) == 0) {
          dfb_config->no_cursor_updates = true;
     } else
     if (strcmp (name, "linux-input-ir-only" ) == 0) {
          dfb_config->linux_input_ir_only = true;
     } else
     if (strcmp (name, "linux-input-touch-abs" ) == 0) {
          dfb_config->linux_input_touch_abs = true;
     } else
     if (strcmp (name, "no-linux-input-touch-abs" ) == 0) {
          dfb_config->linux_input_touch_abs = false;
     } else
     if (strcmp (name, "linux-input-grab" ) == 0) {
          dfb_config->linux_input_grab = true;
     } else
     if (strcmp (name, "no-linux-input-grab" ) == 0) {
          dfb_config->linux_input_grab = false;
     } else
     if (strcmp (name, "linux-input-force" ) == 0) {
          dfb_config->linux_input_force = true;
     } else
     if (strcmp (name, "no-linux-input-force" ) == 0) {
          dfb_config->linux_input_force = false;
     } else
     if (strcmp (name, "motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = true;
     } else
     if (strcmp (name, "no-motion-compression" ) == 0) {
          dfb_config->mouse_motion_compression = false;
     } else
     if (strcmp (name, "mouse-protocol" ) == 0) {
          if (value) {
               dfb_config->mouse_protocol = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No mouse protocol specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mouse-source" ) == 0) {
          if (value) {
               D_FREE( dfb_config->mouse_source );
               dfb_config->mouse_source = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No mouse source specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mouse-gpm-source" ) == 0) {
          dfb_config->mouse_gpm_source = true;
	     D_FREE( dfb_config->mouse_source );
	     dfb_config->mouse_source = D_STRDUP( DEV_NAME_GPM );
     } else
     if (strcmp (name, "no-mouse-gpm-source" ) == 0) {
          dfb_config->mouse_gpm_source = false;
          D_FREE( dfb_config->mouse_source );
          dfb_config->mouse_source = D_STRDUP( DEV_NAME );
     } else
     if (strcmp (name, "smooth-upscale" ) == 0) {
          dfb_config->render_options |= DSRO_SMOOTH_UPSCALE;
     } else
     if (strcmp (name, "no-smooth-upscale" ) == 0) {
          dfb_config->render_options &= ~DSRO_SMOOTH_UPSCALE;
     } else
     if (strcmp (name, "smooth-downscale" ) == 0) {
          dfb_config->render_options |= DSRO_SMOOTH_DOWNSCALE;
     } else
     if (strcmp (name, "no-smooth-downscale" ) == 0) {
          dfb_config->render_options &= ~DSRO_SMOOTH_DOWNSCALE;
     } else
     if (strcmp (name, "translucent-windows" ) == 0) {
          dfb_config->translucent_windows = true;
     } else
     if (strcmp (name, "no-translucent-windows" ) == 0) {
          dfb_config->translucent_windows = false;
     } else
     if (strcmp (name, "decorations" ) == 0) {
          dfb_config->decorations = true;
     } else
     if (strcmp (name, "no-decorations" ) == 0) {
          dfb_config->decorations = false;
     } else
     if (strcmp (name, "startstop" ) == 0) {
          dfb_config->startstop = true;
     } else
     if (strcmp (name, "no-startstop" ) == 0) {
          dfb_config->startstop = false;
     } else
     if (strcmp (name, "autoflip-window" ) == 0) {
          dfb_config->autoflip_window = true;
     } else
     if (strcmp (name, "no-autoflip-window" ) == 0) {
          dfb_config->autoflip_window = false;
     } else
     if (strcmp (name, "gfx-emit-early" ) == 0) {
          dfb_config->gfx_emit_early = true;
     } else
     if (strcmp (name, "no-gfx-emit-early" ) == 0) {
          dfb_config->gfx_emit_early = false;
     } else
     if (strcmp (name, "flip-notify" ) == 0) {
          dfb_config->flip_notify = true;
     } else
     if (strcmp (name, "no-flip-notify" ) == 0) {
          dfb_config->flip_notify = false;
     } else
     if (strcmp (name, "flip-notify-max-latency" ) == 0) {
          if (value) {
               char *error;
               unsigned long latency;

               latency = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->flip_notify_max_latency = latency;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "discard-repeat-events" ) == 0) {
          dfb_config->discard_repeat_events = true;
     } else
     if (strcmp (name, "no-discard-repeat-events" ) == 0) {
          dfb_config->discard_repeat_events = false;
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
     if (strcmp (name, "vt-num" ) == 0) {
          if (value) {
               int vt_num;

               if (direct_sscanf( value, "%d", &vt_num ) < 1) {
                    D_ERROR("DirectFB/Config 'vt-num': Could not parse value!\n");
                    return DFB_INVARG;
               }

               dfb_config->vt_num = vt_num;
          }
          else {
               D_ERROR("DirectFB/Config 'vt-num': No value specified!\n");
               return DFB_INVARG;
          }
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
#if !DIRECTFB_BUILD_PURE_VOODOO
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
                    D_ERROR( "DirectFB/Config: "
                             "Unknown window surface policy `%s'!\n", value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config: "
                        "No window surface policy specified!\n" );
               return DFB_INVARG;
          }
     } else
#endif
     if (strcmp (name, "init-layer" ) == 0) {
          if (value) {
               int id;

               if (direct_sscanf( value, "%d", &id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               if (id < 0 || id > D_ARRAY_SIZE(dfb_config->layers)) {
                    D_ERROR("DirectFB/Config '%s': ID %d out of bounds!\n", name, id);
                    return DFB_INVARG;
               }

               dfb_config->layers[id].init = true;

               dfb_config->config_layer = &dfb_config->layers[id];
          }
          else {
               D_ERROR("DirectFB/Config '%s': No id specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "no-init-layer" ) == 0) {
          if (value) {
               int id;

               if (direct_sscanf( value, "%d", &id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               if (id < 0 || id > D_ARRAY_SIZE(dfb_config->layers)) {
                    D_ERROR("DirectFB/Config '%s': ID %d out of bounds!\n", name, id);
                    return DFB_INVARG;
               }

               dfb_config->layers[id].init = false;

               dfb_config->config_layer = &dfb_config->layers[id];
          }
          else
               dfb_config->layers[0].init = false;
     } else
     if (strcmp (name, "mode" ) == 0 || strcmp (name, "layer-size" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int width, height;

               if (direct_sscanf( value, "%dx%d", &width, &height ) < 2) {
                    D_ERROR("DirectFB/Config '%s': Could not parse width and height!\n", name);
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0]) {
                    dfb_config->mode.width  = width;
                    dfb_config->mode.height = height;
               }

               conf->config.width  = width;
               conf->config.height = height;

               conf->config.flags |= DLCONF_WIDTH | DLCONF_HEIGHT;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No width and height specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "depth" ) == 0 || strcmp (name, "layer-depth" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int depth;

               if (direct_sscanf( value, "%d", &depth ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0]) {
                    dfb_config->mode.depth = depth;
               }

               conf->config.pixelformat = dfb_pixelformat_for_depth( depth );
               conf->config.flags      |= DLCONF_PIXELFORMAT;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No value specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "pixelformat" ) == 0 || strcmp (name, "layer-format" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               DFBSurfacePixelFormat format;

               format = dfb_config_parse_pixelformat( value );
               if (format == DSPF_UNKNOWN) {
                    D_ERROR("DirectFB/Config '%s': Could not parse format!\n", name);
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0])
                    dfb_config->mode.format = format;

               conf->config.pixelformat = format;
               conf->config.flags      |= DLCONF_PIXELFORMAT;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No format specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "desktop-buffer-mode" ) == 0 || strcmp (name, "layer-buffer-mode" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               if (strcmp( value, "auto" ) == 0) {
                    conf->config.flags &= ~DLCONF_BUFFERMODE;
               } else
               if (strcmp( value, "triple" ) == 0) {
                    conf->config.buffermode = DLBM_TRIPLE;
                    conf->config.flags     |= DLCONF_BUFFERMODE;
               } else
               if (strcmp( value, "backvideo" ) == 0) {
                    conf->config.buffermode = DLBM_BACKVIDEO;
                    conf->config.flags     |= DLCONF_BUFFERMODE;
               } else
               if (strcmp( value, "backsystem" ) == 0) {
                    conf->config.buffermode = DLBM_BACKSYSTEM;
                    conf->config.flags     |= DLCONF_BUFFERMODE;
               } else
               if (strcmp( value, "frontonly" ) == 0) {
                    conf->config.buffermode = DLBM_FRONTONLY;
                    conf->config.flags     |= DLCONF_BUFFERMODE;
               } else
               if (strcmp( value, "windows" ) == 0) {
                    conf->config.buffermode = DLBM_WINDOWS;
                    conf->config.flags     |= DLCONF_BUFFERMODE;
               } else {
                    D_ERROR( "DirectFB/Config '%s': Unknown mode '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No buffer mode specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layer-src-key" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->src_key.b = argb & 0xFF;
               argb >>= 8;
               conf->src_key.g = argb & 0xFF;
               argb >>= 8;
               conf->src_key.r = argb & 0xFF;
               argb >>= 8;
               conf->src_key.a = argb & 0xFF;

               conf->config.options |= DLOP_SRC_COLORKEY;
               conf->config.flags   |= DLCONF_OPTIONS;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layer-src-key-index" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   index;

               index = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in index '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->src_key_index = index;
               conf->config.options |= DLOP_SRC_COLORKEY;
               conf->config.flags   |= DLCONF_OPTIONS;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No index specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "bg-none" ) == 0 || strcmp (name, "layer-bg-none" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          conf->background.mode = DLBM_DONTCARE;
     } else
     if (strcmp (name, "bg-image" ) == 0 || strcmp (name, "bg-tile" ) == 0 ||
         strcmp (name, "layer-bg-image" ) == 0 || strcmp (name, "layer-bg-tile" ) == 0)
     {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               if (conf->background.filename)
                    D_FREE( conf->background.filename );

               conf->background.filename = D_STRDUP( value );
               conf->background.mode     = strcmp (name, "bg-tile" ) ? DLBM_IMAGE : DLBM_TILE;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No filename specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "bg-color" ) == 0 || strcmp (name, "layer-bg-color" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->background.color.b = argb & 0xFF;
               argb >>= 8;
               conf->background.color.g = argb & 0xFF;
               argb >>= 8;
               conf->background.color.r = argb & 0xFF;
               argb >>= 8;
               conf->background.color.a = argb & 0xFF;

               conf->background.color_index = -1;
               conf->background.mode        = DLBM_COLOR;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layer-bg-color-index" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   index;

               index = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in index '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->background.color_index = index;
               conf->background.mode        = DLBM_COLOR;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No index specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layer-stacking" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *stackings = D_STRDUP( value );
               char *p = NULL, *r, *s = stackings;

               conf->stacking = 0;

               while ((r = direct_strtok_r( s, ",", &p ))) {
                    direct_trim( &r );

                    if (!strcmp( r, "lower" ))
                         conf->stacking |= (1 << DWSC_LOWER);
                    else if (!strcmp( r, "middle" ))
                         conf->stacking |= (1 << DWSC_MIDDLE);
                    else if (!strcmp( r, "upper" ))
                         conf->stacking |= (1 << DWSC_UPPER);
                    else {
                         D_ERROR( "DirectFB/Config '%s': Unknown class '%s'!\n", name, r );
                         D_FREE( stackings );
                         return DFB_INVARG;
                    }

                    s = NULL;
               }

               D_FREE( stackings );
          }
          else {
               D_ERROR( "DirectFB/Config '%s': Missing value!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strncmp (name, "layer-palette-", 14 ) == 0) {
          int             index;
          char           *error;
          DFBConfigLayer *conf = dfb_config->config_layer;

          index = strtoul( name + 14, &error, 10 );

          if (*error) {
               D_ERROR( "DirectFB/Config '%s': Error in index '%s'!\n", name, error );
               return DFB_INVARG;
          }

          if (index < 0 || index > 255) {
               D_ERROR("DirectFB/Config '%s': Index %d out of bounds!\n", name, index);
               return DFB_INVARG;
          }

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               if (!conf->palette) {
                    conf->palette = D_CALLOC( 256, sizeof(DFBColor) );
                    if (!conf->palette)
                         return D_OOM();
               }

               conf->palette[index].a = (argb & 0xFF000000) >> 24;
               conf->palette[index].r = (argb & 0xFF0000) >> 16;
               conf->palette[index].g = (argb & 0xFF00) >> 8;
               conf->palette[index].b = (argb & 0xFF);

               conf->palette_set = true;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layer-rotate" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int rotate;

               if (direct_sscanf( value, "%d", &rotate ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270) {
                    D_ERROR("DirectFB/Config '%s': Only 0, 90, 180 or 270 supported!\n", name);
                    return DFB_UNSUPPORTED;
               }

               conf->rotate = rotate;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No value specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "image-format" ) == 0) {
          if (value) {
               DFBSurfacePixelFormat format;

               format = dfb_config_parse_pixelformat( value );
               if (format == DSPF_UNKNOWN) {
                    D_ERROR("DirectFB/Config '%s': Could not parse format!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->image_format = format;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No format specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "layers-clear" ) == 0) {
          dfb_config->layers_clear = true;
     } else
     if (strcmp (name, "no-layers-clear" ) == 0) {
          dfb_config->layers_clear = false;
     } else
     if (strcmp (name, "surface-clear" ) == 0) {
          dfb_config->surface_clear = true;
     } else
     if (strcmp (name, "no-surface-clear" ) == 0) {
          dfb_config->surface_clear = false;
     } else
     if (strcmp (name, "input-hub" ) == 0) {
          if (value) {
               char *error;
               unsigned long qid;

               qid = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->input_hub_qid = qid;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "input-hub-service" ) == 0) {
          if (value) {
               char *error;
               unsigned long qid;

               qid = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->input_hub_service_qid = qid;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "video-phys" ) == 0) {
          if (value) {
               char *error;
               unsigned long phys;

               phys = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in hex value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->video_phys = phys;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "video-length" ) == 0) {
          if (value) {
               char *error;
               unsigned long length;

               length = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->video_length = length;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mmio-phys" ) == 0) {
          if (value) {
               char *error;
               unsigned long phys;

               phys = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in hex value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->mmio_phys = phys;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mmio-length" ) == 0) {
          if (value) {
               char *error;
               unsigned long length;

               length = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->mmio_length = length;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "accelerator" ) == 0) {
          if (value) {
               char *error;
               unsigned long accel;

               accel = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->accelerator = accel;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "font-resource-id" ) == 0) {
          if (value) {
               char *error;
               unsigned long resource_id;

               resource_id = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->font_resource_id = resource_id;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "max-font-rows" ) == 0) {
          if (value) {
               char *error;
               unsigned long num;

               num = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->max_font_rows = num;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "max-font-row-width" ) == 0) {
          if (value) {
               char *error;
               unsigned long width;

               width = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->max_font_row_width = width;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "graphics-state-call-limit" ) == 0) {
          if (value) {
               char *error;
               unsigned long limit;

               limit = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->graphics_state_call_limit = limit;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "always-flush-callbuffer" ) == 0) {
          dfb_config->always_flush_callbuffer = true;
     } else
     if (strcmp (name, "no-always-flush-callbuffer" ) == 0) {
          dfb_config->always_flush_callbuffer = false;
     } else
     if (strcmp (name, "layers-fps" ) == 0) {
          if (value) {
               char *error;
               unsigned long interval;

               interval = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->layers_fps = interval;
          }
          else
               dfb_config->layers_fps = 1000;
     } else
     if (strcmp (name, "no-layers-fps" ) == 0) {
          dfb_config->layers_fps = 0;
     } else
     if (strcmp (name, "gfxcard-stats" ) == 0) {
          if (value) {
               char *error;
               unsigned long interval;

               interval = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->gfxcard_stats = interval;
          }
          else
               dfb_config->gfxcard_stats = 1000;
     } else
     if (strcmp (name, "no-gfxcard-stats" ) == 0) {
          dfb_config->gfxcard_stats = 0;
     } else
     if (strcmp (name, "screen-frame-interval" ) == 0) {
          if (value) {
               char *error;
               long long interval;

               interval = strtoll( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config '%s': Error in value '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               dfb_config->screen_frame_interval = interval;
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "matrox-tv-standard" ) == 0) {
          if (value) {
               if (strcmp( value, "pal-60" ) == 0) {
                    dfb_config->matrox_tv_std = DSETV_PAL_60;
               } else
               if (strcmp( value, "pal" ) == 0) {
                    dfb_config->matrox_tv_std = DSETV_PAL;
               } else
               if (strcmp( value, "ntsc" ) == 0) {
                    dfb_config->matrox_tv_std = DSETV_NTSC;
               } else {
                    D_ERROR( "DirectFB/Config: Unknown TV standard "
                             "'%s'!\n", value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config: "
                        "No TV standard specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "matrox-cable-type" ) == 0) {
          if (value) {
               if (strcmp( value, "composite" ) == 0) {
                    dfb_config->matrox_cable = 0;
               } else
               if (strcmp( value, "scart-rgb" ) == 0) {
                    dfb_config->matrox_cable = 1;
               } else
               if (strcmp( value, "scart-composite" ) == 0) {
                    dfb_config->matrox_cable = 2;
               } else {
                    D_ERROR( "DirectFB/Config: Unknown cable type "
                             "'%s'!\n", value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config: "
                        "No cable type specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "x11-borderless" ) == 0) {
          dfb_config->x11_borderless = true;

          if (value && direct_sscanf( value, "%d.%d", &dfb_config->x11_position.x, &dfb_config->x11_position.y ) < 2) {
               D_ERROR("DirectFB/Config '%s': Could not parse x and y!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "matrox-sgram" ) == 0) {
          dfb_config->matrox_sgram = true;
     } else
     if (strcmp (name, "matrox-crtc2" ) == 0) {
          dfb_config->matrox_crtc2 = true;
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
     } else
     if (strcmp (name, "capslock-meta" ) == 0) {
          dfb_config->capslock_meta = true;
     } else
     if (strcmp (name, "no-capslock-meta" ) == 0) {
          dfb_config->capslock_meta = false;
     } else
     if (strcmp (name, "task-manager" ) == 0) {
          dfb_config->task_manager = true;
     } else
     if (strcmp (name, "no-task-manager" ) == 0) {
          dfb_config->task_manager = false;
     } else
     if (strcmp (name, "software-cores" ) == 0) {
          if (value) {
               int cores;

               if (direct_sscanf( value, "%d", &cores ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               if (cores < 1) {
                    D_ERROR("DirectFB/Config '%s': Invalid value specified!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->software_cores = cores;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No value specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "resource-manager" ) == 0) {
          if (value) {
               if (dfb_config->resource_manager)
                    D_FREE( dfb_config->resource_manager );

               dfb_config->resource_manager = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config '%s': No implementation specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "h3600-device" ) == 0) {
          if (value) {
               if (dfb_config->h3600_device)
                    D_FREE( dfb_config->h3600_device );

               dfb_config->h3600_device = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No H3600 TS device specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "mut-device" ) == 0) {
          if (value) {
               if (dfb_config->mut_device)
                    D_FREE( dfb_config->mut_device );

               dfb_config->mut_device = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No MuTouch device specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "zytronic-device" ) == 0) {
          if (value) {
               if (dfb_config->zytronic_device)
                    D_FREE( dfb_config->zytronic_device );

               dfb_config->zytronic_device = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No Zytronic device specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "elo-device" ) == 0) {
          if (value) {
               if (dfb_config->elo_device)
                    D_FREE( dfb_config->elo_device );

               dfb_config->elo_device = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No Elo device specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "penmount-device" ) == 0) {
          if (value) {
               if (dfb_config->penmount_device)
                    D_FREE( dfb_config->penmount_device );

               dfb_config->penmount_device = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: No PenMount device specified!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "linux-input-devices" ) == 0) {
          if (value) {
               config_values_free( &dfb_config->linux_input_devices );
               config_values_parse( &dfb_config->linux_input_devices, value );
          }
          else {
               D_ERROR( "DirectFB/Config: Missing value for linux-input-devices!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "tslib-devices" ) == 0) {
          if (value) {
               config_values_free( &dfb_config->tslib_devices );
               config_values_parse( &dfb_config->tslib_devices, value );
          }
          else {
               D_ERROR( "DirectFB/Config: Missing value for tslib-devices!\n" );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "unichrome-revision" ) == 0) {
          if (value) {
               int rev;

               if (direct_sscanf( value, "%d", &rev ) < 1) {
                    D_ERROR("DirectFB/Config 'unichrome-revision': Could not parse revision!\n");
                    return DFB_INVARG;
               }

               dfb_config->unichrome_revision = rev;
          }
          else {
               D_ERROR("DirectFB/Config 'unichrome-revision': No revision specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "i8xx_overlay_pipe_b") == 0) {
          dfb_config->i8xx_overlay_pipe_b = true;
     } else
     if (strcmp (name, "window-cursor-invisible") == 0) {
          dfb_config->default_cursor_flags = DWCF_INVISIBLE;
     } else
     if (strcmp (name, "max-axis-rate" ) == 0) {
          if (value) {
               unsigned int rate;

               if (direct_sscanf( value, "%u", &rate ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->max_axis_rate = rate;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No value specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "include") == 0) {
          if( value ) {
               DFBResult ret;
               ret = dfb_config_read( value );
               if( ret )
                    return ret;
          }
          else {
               D_ERROR("DirectFB/Config 'include': No include file specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "cursor-videoonly" ) == 0) {
          dfb_config->cursor_videoonly = true;
     } else
     if (strcmp (name, "no-cursor-videoonly" ) == 0) {
          dfb_config->cursor_videoonly = false;
     } else
     if (strcmp (name, "cursor-resource-id" ) == 0) {
          if (value) {
               unsigned long long cursor_resource_id;

               if (direct_sscanf( value, "%llu", &cursor_resource_id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->cursor_resource_id = cursor_resource_id;
          }
          else {
               D_ERROR("DirectFB/Config '%s': No id specified!\n", name);
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "single-window" ) == 0) {
          dfb_config->single_window = true;
     } else
     if (
#if DIRECTFB_BUILD_VOODOO
         voodoo_config_set( name, value ) &&
#endif
         fusion_config_set( name, value ) && direct_config_set( name, value ))
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

DFBResult dfb_config_init( int *argc, char *(*argv[]) )
{
     DFBResult ret;
     int i;
#ifndef WIN32
     char *home = direct_getenv( "HOME" );
#endif
     char *prog = NULL;
     char *session;
     char *dfbargs;
#ifndef WIN32
     char  cmdbuf[1024];
#endif

     if (dfb_config) {
          /* Check again for session environment setting. */
          session = direct_getenv( "DIRECTFB_SESSION" );
          if (session)
               dfb_config_set( "session", session );

          return DFB_OK;
     }

     config_allocate();

     /* Read system settings. */
     ret = dfb_config_read( SYSCONFDIR"/directfbrc" );
     if (ret  &&  ret != DFB_IO)
          return ret;

#ifndef WIN32
     /* Read user settings. */
     if (home) {
          int  len = strlen(home) + strlen("/.directfbrc") + 1;
          char buf[len];

          direct_snprintf( buf, len, "%s/.directfbrc", home );

          ret = dfb_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
#endif

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

#ifndef WIN32
     /* Read global application settings. */
     if (prog && prog[0]) {
          int  len = strlen( SYSCONFDIR"/directfbrc." ) + strlen(prog) + 1;
          char buf[len];

          direct_snprintf( buf, len, SYSCONFDIR"/directfbrc.%s", prog );

          ret = dfb_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }

     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen(home) + strlen("/.directfbrc.") + strlen(prog) + 1;
          char buf[len];

          direct_snprintf( buf, len, "%s/.directfbrc.%s", home, prog );

          ret = dfb_config_read( buf );
          if (ret  &&  ret != DFB_IO)
               return ret;
     }
#endif

     /* Read settings from environment variable. */
     dfbargs = direct_getenv( "DFBARGS" );
     if (dfbargs) {
          ret = parse_args( dfbargs );
          if (ret)
               return ret;
     }

     /* Active session is used if present, only command line can override. */
     session = direct_getenv( "DIRECTFB_SESSION" );
     if (session)
          dfb_config_set( "session", session );

     /* Read settings from command line. */
     if (argc && argv) {
          for (i = 1; i < *argc; i++) {

               if (strcmp ((*argv)[i], "--dfb-help") == 0) {
                    print_config_usage();
                    exit(1);
               }

               if (strncmp ((*argv)[i], "--dfb:", 6) == 0) {
                    ret = parse_args( (*argv)[i] + 6 );
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

     if (!dfb_config->vt_switch)
          dfb_config->kd_graphics = true;

     return DFB_OK;
}

DFBResult dfb_config_read( const char *filename )
{
     DFBResult ret = DFB_OK;
     char line[400];
     FILE *f;

     char *slash = 0;
     char *cwd   = 0;

     config_allocate();

     dfb_config->config_layer = &dfb_config->layers[0];

     f = fopen( filename, "r" );
     if (!f) {
          D_DEBUG_AT( DirectFB_Config, "Unable to open config file `%s'!\n", filename );
          return DFB_IO;
     } else {
          D_DEBUG_AT( DirectFB_Config, "Parsing config file '%s'.\n", filename );
     }

#ifndef WIN32
     /* store/restore the cwd (needed for the "include" command */
     slash = strrchr( filename, '/' );
     if( slash ) {
          cwd = malloc( 4096 );
          if (!cwd) {
               fclose( f );
               return D_OOM();
          }

          if (!getcwd( cwd, 4096 )) {
               ret = errno2result( errno );
               free( cwd );
               fclose( f );
               return ret;
          }

          /* must copy filename for path, due to const'ness */
          char nwd[strlen(filename)];
          strcpy( nwd, filename );
          nwd[slash-filename] = 0;
          if (chdir( nwd ))
               D_WARN( "config: failed to change directory to %s.\n", nwd );
          else
               D_DEBUG_AT( DirectFB_Config, "changing configuration lookup directory to '%s'.\n", nwd );
     }
#endif

     while (fgets( line, 400, f )) {
          char *name = line;
          char *comment = strchr( line, '#');
          char *value;

          if (comment) {
               *comment = 0;
          }

          value = strchr( line, '=' );

          if (value) {
               *value++ = 0;
               direct_trim( &value );
          }

          direct_trim( &name );

          if (!*name  ||  *name == '#')
               continue;

          ret = dfb_config_set( name, value );
          if (ret) {
               if (ret == DFB_UNSUPPORTED) {
                    D_ERROR( "DirectFB/Config: *********** In config file `%s': "
                             "Invalid option `%s'! ***********\n", filename, name );
                    ret = DFB_OK;
                    continue;
               }
               break;
          }
     }

     fclose( f );

#ifndef WIN32
     /* restore original cwd */
     if( cwd ) {
          if (chdir( cwd ))
               D_WARN( "config: failed to change directory to %s.\n", cwd );
          else
               D_DEBUG_AT( DirectFB_Config, "changing directtory back to '%s'.\n", cwd );

          free( cwd );
     }
#endif

     return ret;
}

