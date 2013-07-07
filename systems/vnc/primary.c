/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <directfb.h>
#include <directfb_util.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/system.h>
#include <core/input.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include "vnc.h"
#include "primary.h"


D_DEBUG_DOMAIN( VNC_Layer,  "VNC/Layer",  "VNC Layer" );

/**********************************************************************************************************************/

/******************************************************************************/
/*VNC server setup*/
/* Here we create a structure so that every client has it's own pointer */

typedef struct ClientData {
     DFBVNC *vnc;
     int oldButtonMask;
     int oldx,oldy;
} ClientData;

static void process_key_event(rfbBool down, rfbKeySym key, struct _rfbClientRec* cl);
static void process_pointer_event(int buttonMask, int x, int y, struct _rfbClientRec* cl);
static bool translate_key(rfbKeySym key, DFBInputEvent *evt );
static void clientgone(rfbClientPtr cl);
static enum rfbNewClientAction newclient(rfbClientPtr cl);

extern CoreInputDevice *vncInputDevice;

/******************************************************************************/

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     int            argc   = 0;
     char         **argv   = NULL;
     DFBVNC        *vnc    = driver_data;
     DFBVNCShared  *shared = vnc->shared;

     /* Set the screen capabilities. */
     description->caps = DSCCAPS_NONE;

     /* Set the screen name. */
     direct_snputs( description->name, "VNC Primary Screen", DFB_SCREEN_DESC_NAME_LENGTH );

     /* Set video mode */
     vnc->rfb_screen = rfbGetScreen( &argc, argv, shared->screen_size.w, shared->screen_size.h, 8, 3, 4 );
     if (!vnc->rfb_screen) {
          D_ERROR( "DirectFB/VNC: rfbGetScreen( %dx%d, 8, 3, 4 ) failed!\n", shared->screen_size.w, shared->screen_size.h );
          return DFB_FAILURE;
     }

     vnc->rfb_screen->screenData = vnc;


     vnc->rfb_screen->serverFormat.redShift   = 16;
     vnc->rfb_screen->serverFormat.greenShift = 8;
     vnc->rfb_screen->serverFormat.blueShift  = 0;
     vnc->rfb_screen->serverFormat.redMax     = 255;
     vnc->rfb_screen->serverFormat.greenMax   = 255;
     vnc->rfb_screen->serverFormat.blueMax    = 255;


     /* Connect key handler */

     vnc->rfb_screen->kbdAddEvent   = process_key_event;
     vnc->rfb_screen->ptrAddEvent   = process_pointer_event;
     vnc->rfb_screen->newClientHook = newclient;
     vnc->rfb_screen->autoPort      = TRUE;

     /* Initialize VNC */

     rfbInitServer(vnc->rfb_screen);

     if (vnc->rfb_screen->listenSock == -1) {
          D_ERROR( "DirectFB/VNC: rfbInitServer() failed to initialize listening socket!\n" );
          return DFB_FAILURE;
     }


     DFBResult         ret;
     CoreSurfaceConfig config;

     config.flags                  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
     config.size.w                 = shared->screen_size.w;
     config.size.h                 = shared->screen_size.h;
     config.format                 = DSPF_ARGB;
     config.caps                   = DSCAPS_SYSTEMONLY | DSCAPS_DOUBLE;// | DSCAPS_SHARED;

     ret = dfb_surface_create( vnc->core, &config, CSTF_NONE, 0, NULL, &shared->screen_surface );
     if (ret) {
          D_DERROR( ret, "DirectFB/VNC: Could not createscreen surface!\n" );
          return ret;
     }


     dfb_surface_lock_buffer( shared->screen_surface, 0, CSAID_CPU, CSAF_WRITE, &vnc->buffer_lock );

     rfbNewFramebuffer( vnc->rfb_screen, vnc->buffer_lock.addr, shared->screen_size.w, shared->screen_size.h, 8, 3, 4 );

     /* patch serverFormat structure for ARGB */
     vnc->rfb_screen->serverFormat.redShift   = 16;
     vnc->rfb_screen->serverFormat.greenShift = 8;
     vnc->rfb_screen->serverFormat.blueShift  = 0;
     vnc->rfb_screen->serverFormat.redMax     = 255;
     vnc->rfb_screen->serverFormat.greenMax   = 255;
     vnc->rfb_screen->serverFormat.blueMax    = 255;

     rfbRunEventLoop( vnc->rfb_screen, -1, TRUE );

     return DFB_OK;
}
/*
static DFBResult
primaryShutdownScreen( CoreScreen *screen,
                       void       *driver_data,
                       void       *screen_data )
{
     DFBVNC        *vnc = driver_data;
     DFBVNCShared  *shared = vnc->shared;

     rfbScreenCleanup( vnc->rfb_screen );

     dfb_surface_unlock_buffer( shared->screen_surface, &vnc->buffer_lock );

     return DFB_OK;
}
*/
static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     DFBVNC *vnc = driver_data;

     *ret_width  = vnc->shared->screen_size.w;
     *ret_height = vnc->shared->screen_size.h;

     return DFB_OK;
}

static const ScreenFuncs _vncPrimaryScreenFuncs = {
     .InitScreen     = primaryInitScreen,
//     .ShutdownScreen = primaryShutdownScreen,
     .GetScreenSize  = primaryGetScreenSize,
};

const ScreenFuncs *vncPrimaryScreenFuncs = &_vncPrimaryScreenFuncs;

/******************************************************************************/

static int
primaryLayerDataSize( void )
{
     return sizeof(VNCLayerData);
}

static int
primaryRegionDataSize( void )
{
     return 0;
}

static DFBResult
primaryInitLayer( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  DFBDisplayLayerDescription *description,
                  DFBDisplayLayerConfig      *config,
                  DFBColorAdjustment         *adjustment )
{
     DFBVNC       *vnc  = driver_data;
     VNCLayerData *data = layer_data;
     const char   *name;

     D_DEBUG( "DirectFB/VNC: primaryInitLayer\n");

     char *names[] = { "Primary", "Secondary", "Tertiary" };

     if (vnc->layer_count < 3)
          name = names[vnc->layer_count];
     else
          name = "Other";

     data->layer_id = vnc->layer_count;

     vnc->shared->layer_data[vnc->layer_count++] = data;


     /* set capabilities and type */
     description->caps             = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION | DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY;
     description->type             = DLTF_GRAPHICS;
     description->surface_accessor = CSAID_CPU;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VNC %s Layer", name );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode  = DLBM_FRONTONLY;
     config->width       = vnc->shared->screen_size.w;
     config->height      = vnc->shared->screen_size.h;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else
          config->pixelformat = DSPF_RGB32;

     return DFB_OK;
}

static DFBResult
primaryTestRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
          case DLBM_TRIPLE:
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }

     if (config->options & ~(DLOP_ALPHACHANNEL | DLOP_OPACITY))
          fail |= CLRCF_OPTIONS;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primaryAddRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
UpdateScreen( DFBVNC                *vnc,
              const DFBRectangle    *update )
{
     DFBVNCShared *shared = vnc->shared;
     DFBRegion     clip   = { 0, 0, shared->screen_size.w - 1, shared->screen_size.h - 1 };
     CardState     state;
     unsigned int  i;

     D_DEBUG_AT( VNC_Layer, "%s()\n", __FUNCTION__ );

     if (update) {
          if (!dfb_region_rectangle_intersect( &clip, update )) {
               D_DEBUG_AT( VNC_Layer, "  -> update not intersecting with screen area!\n" );
               return DFB_OK;
          }
     }

     dfb_state_init( &state, vnc->core );

     state.destination = shared->screen_surface;
     state.clip        = clip;
     state.src_blend   = DSBF_ONE;
     state.dst_blend   = DSBF_INVSRCALPHA;

     // FIXME: optimize out clear
     DFBRectangle rect = {
          0, 0, shared->screen_size.w, shared->screen_size.h
     };

     dfb_gfxcard_fillrectangles( &rect, 1, &state );


     for (i=0; i<VNC_MAX_LAYERS; i++) {
          VNCLayerData *data = shared->layer_data[i];

          if (data->surface) {
               DFBRectangle src = data->config.source;
               DFBRectangle dst = data->config.dest;

               state.modified |= SMF_SOURCE | SMF_BLITTING_FLAGS;
               state.source    = data->surface;

               /* emulate missing hw layer clipping */
//               if (dst.x + dst.w > shared->screen_size.w || dst.y + dst.h > shared->screen_size.h)
//                    return DFB_ACCESSDENIED;


               if (data->config.options & DLOP_ALPHACHANNEL) {
                    state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL;

                    if (!(data->surface->config.caps & DSCAPS_PREMULTIPLIED))
                         state.blittingflags |= DSBLIT_SRC_PREMULTIPLY;
               }
               else
                    state.blittingflags = DSBLIT_NOFX;

               if ((data->config.options & DLOP_OPACITY) && data->config.opacity != 0xff) {
                    state.blittingflags |= DSBLIT_BLEND_COLORALPHA;

                    if (data->surface->config.caps & DSCAPS_PREMULTIPLIED)
                         state.blittingflags |= DSBLIT_SRC_PREMULTCOLOR;


                    state.modified |= SMF_COLOR;
                    state.color.a   = data->config.opacity;
               }

               dfb_gfxcard_stretchblit( &src, &dst, &state );
          }
     }

     state.destination = NULL;
     state.source      = NULL;

     dfb_state_destroy( &state );


     dfb_back_to_front_copy( shared->screen_surface, &clip );


     dfb_gfxcard_sync();

     DirectResult ret;

     ret = fusion_call_execute( &shared->call, FCEF_ONEWAY,
                                (int)((clip.x1 << 16) | clip.x2),
                                (void*)(long)((clip.y1 << 16) | clip.y2),
                                NULL );
     if (ret) {
          D_DERROR( ret, "DirectFB/VNC: fusion_call_execute2() failed!\n" );
          return ret;
     }

     return DFB_OK;
}

static DFBResult
primarySetRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette,
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     DFBVNC       *vnc  = driver_data;
     VNCLayerData *data = layer_data;

     D_DEBUG_AT( VNC_Layer, "%s()\n", __FUNCTION__ );

     data->config = *config;

     if (updated & CLRCF_OPACITY)
          D_DEBUG_AT( VNC_Layer, "  -> opacity %d\n", config->opacity );

     dfb_surface_link( &vnc->shared->layer_data[data->layer_id]->surface, surface );

     if (data->shown)
          return UpdateScreen( vnc, NULL );
     else
          D_DEBUG_AT( VNC_Layer, "  -> not shown\n" );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     DFBVNC       *vnc  = driver_data;
     VNCLayerData *data = layer_data;

     D_DEBUG_AT( VNC_Layer, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( VNC_Layer, "  -> setting shown to false\n" );

     data->shown = false;

     dfb_surface_unlink( &vnc->shared->layer_data[data->layer_id]->surface );

     return UpdateScreen( vnc, NULL );
}

static DFBResult
primaryFlipRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   const DFBRegion       *left_update,
                   CoreSurfaceBufferLock *left_lock,
                   const DFBRegion       *right_update,
                   CoreSurfaceBufferLock *right_lock )
{
     DFBVNC       *vnc  = driver_data;
     VNCLayerData *data = layer_data;

     D_DEBUG_AT( VNC_Layer, "%s()\n", __FUNCTION__ );

     dfb_surface_flip( surface, false );

     D_DEBUG_AT( VNC_Layer, "  -> setting shown to true\n" );

     data->shown = true;

     return UpdateScreen( vnc, &data->config.dest );
}

static DFBResult
primaryUpdateRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data,
                     CoreSurface           *surface,
                     const DFBRegion       *left_update,
                     CoreSurfaceBufferLock *left_lock,
                     const DFBRegion       *right_update,
                     CoreSurfaceBufferLock *right_lock )
{
     DFBVNC       *vnc    = driver_data;
     VNCLayerData *data   = layer_data;

     DFBRectangle  update = data->config.dest;

     D_DEBUG_AT( VNC_Layer, "%s()\n", __FUNCTION__ );

     if (left_update) {
          DFBRegion screen_update = {
               left_update->x1 + data->config.dest.x,
               left_update->y1 + data->config.dest.y,
               left_update->x2 + data->config.dest.x,
               left_update->y2 + data->config.dest.y
          };

          D_DEBUG_AT( VNC_Layer, "  -> update %d,%d-%dx%d\n", DFB_RECTANGLE_VALS_FROM_REGION(&screen_update) );

          if (!dfb_rectangle_intersect_by_region( &update, &screen_update )) {
               D_DEBUG_AT( VNC_Layer, "  -> no intersection with %d,%d-%dx%d!\n", DFB_RECTANGLE_VALS( &data->config.dest ) );

               return DFB_OK;
          }
     }

     D_DEBUG_AT( VNC_Layer, "  -> setting shown to true\n" );

     data->shown = true;

     return UpdateScreen( vnc, &update );
}

static DFBResult
primaryAllocateSurface( CoreLayer              *layer,
                        void                   *driver_data,
                        void                   *layer_data,
                        void                   *region_data,
                        CoreLayerRegionConfig  *config,
                        CoreSurface           **ret_surface )
{
     DFBVNC            *vnc = driver_data;
     CoreSurfaceConfig  conf;

     conf.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
     conf.size.w = config->width;
     conf.size.h = config->height;
     conf.format = config->format;
     conf.caps   = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          conf.caps |= DSCAPS_DOUBLE;

     return dfb_surface_create( vnc->core, &conf, CSTF_LAYER, DLID_PRIMARY, NULL, ret_surface );
}

static DFBResult
primaryReallocateSurface( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          void                  *region_data,
                          CoreLayerRegionConfig *config,
                          CoreSurface           *surface )
{
     DFBResult         ret;
     CoreSurfaceConfig conf;

     conf.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
     conf.size.w = config->width;
     conf.size.h = config->height;
     conf.format = config->format;
     conf.caps   = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          conf.caps |= DSCAPS_DOUBLE;

     ret = dfb_surface_reconfig( surface, &conf );
     if (ret)
          return ret;

     if (DFB_PIXELFORMAT_IS_INDEXED(config->format) && !surface->palette) {
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( NULL,    /* FIXME */
                                    1 << DFB_COLOR_BITS_PER_PIXEL( config->format ),
                                    &palette );
          if (ret)
               return ret;

          if (config->format == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     return DFB_OK;
}

static const DisplayLayerFuncs _vncPrimaryLayerFuncs = {
     .LayerDataSize     = primaryLayerDataSize,
     .RegionDataSize    = primaryRegionDataSize,
     .InitLayer         = primaryInitLayer,

     .TestRegion        = primaryTestRegion,
     .AddRegion         = primaryAddRegion,
     .SetRegion         = primarySetRegion,
     .RemoveRegion      = primaryRemoveRegion,
     .FlipRegion        = primaryFlipRegion,
     .UpdateRegion      = primaryUpdateRegion,

     .AllocateSurface   = primaryAllocateSurface,
     .ReallocateSurface = primaryReallocateSurface,
};

const DisplayLayerFuncs *vncPrimaryLayerFuncs = &_vncPrimaryLayerFuncs;


/**********************************************************************************************************************/

/**
  VNC Server setup
**/

static void
clientgone(rfbClientPtr cl)
{
     D_FREE( cl->clientData );
}

static enum rfbNewClientAction
newclient(rfbClientPtr cl)
{
     ClientData *cd;
     DFBVNC     *vnc = cl->screen->screenData;

     cd = D_CALLOC( 1, sizeof(ClientData) );
     if (!cd) {
          D_OOM();
          return RFB_CLIENT_REFUSE;
     }

     cd->vnc = vnc;

     cl->clientData     = cd;
     cl->clientGoneHook = clientgone;

     return RFB_CLIENT_ACCEPT;
}

static void
send_button_event( DFBInputDeviceButtonIdentifier button,
                   bool                           press )
{
     if (vncInputDevice) {
          DFBInputEvent evt;
     
          evt.flags  = DIEF_NONE;
          evt.type   = press ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
          evt.button = button;
     
          dfb_input_dispatch( vncInputDevice, &evt );
     }
}

static void
process_pointer_event(int buttonMask, int x, int y, rfbClientPtr cl)
{
     if (vncInputDevice) {
          ClientData    *cd = cl->clientData;
          DFBInputEvent  evt;
     
          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISABS | DIEF_MIN | DIEF_MAX;
     
          if (cd->oldx != x) {
               cd->oldx = x;
     
               evt.axis    = DIAI_X;
               evt.axisabs = x;
               evt.min     = 0;
               evt.max     = cd->vnc->shared->screen_size.w - 1;
     
               dfb_input_dispatch( vncInputDevice, &evt );
          }
     
          if (cd->oldy != y) {
               cd->oldy = y;
     
               evt.axis    = DIAI_Y;
               evt.axisabs = y;
               evt.min     = 0;
               evt.max     = cd->vnc->shared->screen_size.h - 1;
     
               dfb_input_dispatch( vncInputDevice, &evt );
          }
     
          if (buttonMask != cd->oldButtonMask) {
               if ((buttonMask & (1 << 0)) != (cd->oldButtonMask & (1 << 0)))
                    send_button_event( DIBI_LEFT, !!(buttonMask & (1 << 0)) );
     
               if ((buttonMask & (1 << 1)) != (cd->oldButtonMask & (1 << 1)))
                    send_button_event( DIBI_MIDDLE, !!(buttonMask & (1 << 1)) );
     
               if ((buttonMask & (1 << 2)) != (cd->oldButtonMask & (1 << 2)))
                    send_button_event( DIBI_RIGHT, !!(buttonMask & (1 << 2)) );
     
               cd->oldButtonMask = buttonMask;
          }
     }

     rfbDefaultPtrAddEvent(buttonMask,x,y,cl);
}

/*
 * declaration of private data
 */
static void
process_key_event(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
     if (vncInputDevice) {
          DFBInputEvent evt;
     
          if (down)
               evt.type = DIET_KEYPRESS;
          else
               evt.type = DIET_KEYRELEASE;
     
          if (translate_key( key, &evt ))
               dfb_input_dispatch( vncInputDevice, &evt );
     }
}


static bool
translate_key(rfbKeySym key, DFBInputEvent *evt )
{
     /* Unicode */
     if (key <= 0xf000) {
          evt->flags = DIEF_KEYSYMBOL;
          evt->key_symbol = key;
          return true;
     }

     /* Dead keys */
     /* todo */

     /* Numeric keypad */
     if (key >= XK_KP_0  &&  key <= XK_KP_9) {
          evt->flags = DIEF_KEYID;
          evt->key_id = DIKI_KP_0 + key - XK_KP_0;
          return true;
     }

     /* Function keys */
     if (key >= XK_F1  &&  key <= XK_F11) {
          evt->flags = DIEF_KEYID;
          evt->key_id = DIKI_F1 + key - XK_F1;
          return true;
     }

     switch (key) {
          /* Numeric keypad */
          case XK_KP_Decimal:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_DECIMAL;
               break;

          case XK_KP_Separator:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_SEPARATOR;
               break;

          case XK_KP_Divide:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_DIV;
               break;

          case XK_KP_Multiply:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_MULT;
               break;

          case XK_KP_Subtract:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_MINUS;
               break;

          case XK_KP_Add:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_PLUS;
               break;

          case XK_KP_Enter:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_ENTER;
               break;

          case XK_KP_Equal:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_EQUAL;
               break;


               /* Arrows + Home/End pad */
          case XK_Up:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_UP;
               break;

          case XK_Down:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_DOWN;
               break;

          case XK_Right:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_RIGHT;
               break;

          case XK_Left:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_LEFT;
               break;

          case XK_Insert:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_INSERT;
               break;

          case XK_Delete:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_DELETE;
               break;

          case XK_Home:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_HOME;
               break;

          case XK_End:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_END;
               break;

          case XK_Page_Up:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_PAGE_UP;
               break;

          case XK_Page_Down:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_PAGE_DOWN;
               break;


               /* Key state modifier keys */
          case XK_Num_Lock:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_NUM_LOCK;
               break;

          case XK_Caps_Lock:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_CAPS_LOCK;
               break;

          case XK_Scroll_Lock:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SCROLL_LOCK;
               break;

          case XK_Shift_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SHIFT_R;
               break;

          case XK_Shift_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SHIFT_L;
               break;

          case XK_Control_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_CONTROL_R;
               break;

          case XK_Control_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_CONTROL_L;
               break;

          case XK_Alt_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ALT_R;
               break;

          case XK_Alt_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ALT_L;
               break;

          case XK_Meta_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_META_R;
               break;

          case XK_Meta_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_META_L;
               break;

          case XK_Super_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SUPER_L;
               break;

          case XK_Super_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SUPER_R;
               break;

          case XK_Hyper_L:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_HYPER_L;
               break;

          case XK_Hyper_R:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_HYPER_R;
               break;

               /*case ??:
                    evt->flags = DIEF_KEYID;
                    evt->key_id = DIKI_ALTGR;
                    break;*/

          case XK_BackSpace:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_BACKSPACE;
               break;

          case XK_Tab:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_HYPER_L;
               break;

          case XK_Return:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ENTER;
               break;

          case XK_Escape:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ESCAPE;
               break;

          case XK_Pause:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_PAUSE;
               break;

               /* Miscellaneous function keys */
          case XK_Help:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_HELP;
               break;

          case XK_Print:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_PRINT;
               break;

          case XK_Break:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_BREAK;
               break;

          default:
               return false;
     }

     return true;
}

