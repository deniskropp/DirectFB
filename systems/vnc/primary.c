/*
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

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/input.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include "vnc.h"
#include "primary.h"


/******************************************************************************/
/*VNC server setup*/
/* Here we create a structure so that every client has it's own pointer */

typedef struct ClientData {
  int oldButtonMask;
  int oldButton;
  int oldx,oldy;
} ClientData;

static void process_key_event(rfbBool down, rfbKeySym key, struct _rfbClientRec* cl);
static void process_pointer_event(int buttonMask, int x, int y, struct _rfbClientRec* cl);
static bool translate_key(rfbKeySym key, DFBInputEvent *evt );
static void clientgone(rfbClientPtr cl);
static enum rfbNewClientAction newclient(rfbClientPtr cl);



static DFBEnumerationResult attach_input_device( CoreInputDevice *device, void *ctx );

static DFBResult dfb_vnc_set_video_mode( CoreDFB* core, CoreLayerRegionConfig *config );
static DFBResult dfb_vnc_update_screen( CoreDFB *core, DFBRegion *region );
static DFBResult dfb_vnc_set_palette( CorePalette *palette );

static DFBResult update_screen( CoreSurface *surface,
                                int x, int y, int w, int h );

static void* vnc_server_thread( DirectThread *thread, void *data );
static void* vnc_refresh_thread( DirectThread *thread, void *data );

extern DFBVNC *dfb_vnc;
extern CoreDFB *dfb_vnc_core;
rfbScreenInfoPtr rfb_screen = NULL;
static CoreInputDevice *vncInputDevice;

/******************************************************************************/

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   GraphicsDevice       *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_NONE;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "VNC Primary Screen" );

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_ASSERT( dfb_vnc != NULL );

     if (dfb_vnc->primary) {
          *ret_width  = dfb_vnc->primary->width;
          *ret_height = dfb_vnc->primary->height;
     }
     else {
          if (dfb_config->mode.width)
               *ret_width  = dfb_config->mode.width;
          else
               *ret_width  = 640;

          if (dfb_config->mode.height)
               *ret_height = dfb_config->mode.height;
          else
               *ret_height = 480;
     }

     return DFB_OK;
}

ScreenFuncs vncPrimaryScreenFuncs = {
     .InitScreen   = primaryInitScreen,
     .GetScreenSize = primaryGetScreenSize
};

/******************************************************************************/

static int
primaryLayerDataSize()
{
     return 0;
}

static int
primaryRegionDataSize()
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
     D_DEBUG( "DirectFB/VNC: primaryInitLayer\n");
          
     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "VNC Primary Layer" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode  = DLBM_FRONTONLY;

     if (dfb_config->mode.width)
          config->width  = dfb_config->mode.width;
     else
          config->width  = 640;

     if (dfb_config->mode.height)
          config->height = dfb_config->mode.height;
     else
          config->height = 480;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else
          config->pixelformat = DSPF_RGB24;

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
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }

     if (config->options)
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
primarySetRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette )
{
     DFBResult ret;

     D_DEBUG( "DirectFB/VNC: primarySetRegion\n");

     ret = dfb_vnc_set_video_mode( dfb_vnc_core, config );
     if (ret)
          return ret;

     if (surface)
          dfb_vnc->primary = surface;

     if (palette)
          dfb_vnc_set_palette( palette );

     driver_data = (void*) rfb_screen; 
     
     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     dfb_vnc->primary = NULL;

     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer           *layer,
                   void                *driver_data,
                   void                *layer_data,
                   void                *region_data,
                   CoreSurface         *surface,
                   DFBSurfaceFlipFlags  flags )
{
     dfb_surface_flip_buffers( surface, false );

     return dfb_vnc_update_screen( dfb_vnc_core, NULL );
}

static DFBResult
primaryUpdateRegion( CoreLayer           *layer,
                     void                *driver_data,
                     void                *layer_data,
                     void                *region_data,
                     CoreSurface         *surface,
                     const DFBRegion           *update )
{
     if (update) {
          DFBRegion region = *update;

          return dfb_vnc_update_screen( dfb_vnc_core, &region );
     }

     return dfb_vnc_update_screen( dfb_vnc_core, NULL );
}

static DFBResult
primaryAllocateSurface( CoreLayer              *layer,
                        void                   *driver_data,
                        void                   *layer_data,
                        void                   *region_data,
                        CoreLayerRegionConfig  *config,
                        CoreSurface           **ret_surface )
{
     DFBSurfaceCapabilities caps = DSCAPS_SYSTEMONLY;

     if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_DOUBLE;

     return dfb_surface_create( dfb_vnc->core, config->width, config->height,
                                config->format, CSP_SYSTEMONLY,
                                caps, NULL, ret_surface );
}

static DFBResult
primaryReallocateSurface( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          void                  *region_data,
                          CoreLayerRegionConfig *config,
                          CoreSurface           *surface )
{
     DFBResult ret;

     /* FIXME: write surface management functions
               for easier configuration changes */

     switch (config->buffermode) {
          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
               surface->caps |= DSCAPS_DOUBLE;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;

          case DLBM_FRONTONLY:
               surface->caps &= ~DSCAPS_DOUBLE;

               ret = dfb_surface_reconfig( surface,
                                           CSP_SYSTEMONLY, CSP_SYSTEMONLY );
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }
     if (ret)
          return ret;

     ret = dfb_surface_reformat( NULL, surface, config->width,
                                 config->height, config->format );
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

DisplayLayerFuncs vncPrimaryLayerFuncs = {
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
     .ReallocateSurface = primaryReallocateSurface
};

/******************************************************************************/

static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h )
{
     int          i, j, k;
     void        *dst, *p;
     void        *src, *q;
     int          pitch;
     DFBResult    ret;

     D_ASSERT( surface != NULL );
     D_ASSERT( rfb_screen != NULL );
     D_ASSERT( rfb_screen->frameBuffer != NULL );

     ret = dfb_surface_soft_lock( surface, DSLF_READ, &src, &pitch, true );
     if (ret) {
          D_ERROR( "DirectFB/VNC: Couldn't lock layer surface: %s\n",
                   DirectFBErrorString( ret ) );
          return ret;
     }

     dst = rfb_screen->frameBuffer;
     
     src += DFB_BYTES_PER_LINE( surface->format, x ) + y * pitch;
     dst += x * rfb_screen->depth/8 + y * rfb_screen->width * rfb_screen->depth/8;

     for (i=0; i<h; ++i) {
          /*direct_memcpy( dst, src, DFB_BYTES_PER_LINE( surface->format, w ) );*/
      for(j=0, p=src, q=dst; j<w; j++, 
                    p += DFB_BYTES_PER_PIXEL(surface->format),
                    q += rfb_screen->depth/8){
         /*direct_memcpy( q, p, DFB_BYTES_PER_PIXEL(surface->format));*/
         /**(char*) q = *(char*) (p+2);
         *(char*) (q+1) = *(char*) (p+1);
         *(char*) (q+2) = *(char*) p;*/
         for(k=0; k<DFB_BYTES_PER_PIXEL(surface->format); k++)
        *(char*) (q+k) = *(char*) (p+DFB_BYTES_PER_PIXEL(surface->format)-1-k);
          }
      
          src += pitch;
          dst += rfb_screen->width * rfb_screen->depth/8;
     }

     rfbMarkRectAsModified ( rfb_screen, x, y, x+w, y+h );

     dfb_surface_unlock( surface, true );


#if 0 /* Mire test */

     maxx = screen->width ;
     maxy = screen->height ;
     bpp = screen->depth/8 ;
     buffer = screen->frameBuffer ;


      for(j=0;j<maxy;++j) {
         for(i=0;i<maxx;++i) {
                       buffer[(j*maxx+i)*bpp+0]=(i+j)*128/(maxx+maxy); /* red */
                         buffer[(j*maxx+i)*bpp+1]=i*128/maxx; /* green */
                           buffer[(j*maxx+i)*bpp+2]=j*256/maxy; /* blue */
                               }
                 buffer[j*maxx*bpp+0]=0xff;
                 buffer[j*maxx*bpp+1]=0xff;
                     buffer[j*maxx*bpp+2]=0xff;
                     buffer[j*maxx*bpp+3]=0xff;
                       }
     

       
       rfbMarkRectAsModified ( screen, 0, 0, 600, 800);
#endif
     
     return DFB_OK;
}

/******************************************************************************/

typedef enum {
     VNC_SET_VIDEO_MODE,
     VNC_UPDATE_SCREEN,
     VNC_SET_PALETTE
} DFBVNCCall;

static DFBResult
dfb_vnc_set_video_mode_handler( CoreLayerRegionConfig *config )
{
     int argc = 0;
     char** argv = NULL;

     D_DEBUG( "DirectFB/VNC: layer config properties\n");

     if(rfb_screen) /*!!! FIXME*/
         return DFB_OK;

     fusion_skirmish_prevail( &dfb_vnc->lock );

     /* Set video mode */
     rfb_screen = rfbGetScreen(&argc, argv, config->width, config->height, DFB_BITS_PER_PIXEL(config->format)/3, 3, 4);
     
     D_DEBUG( "DirectFB/VNC: rfbGetScreen parameters: width %d height %d bitspersample %d samplesperpixel %d bytesperpixel %d\n", config->width, config->height, DFB_BITS_PER_PIXEL(config->format)/3, 3, 4);
     
     /*screen = rfbGetScreen(&argc, argv, config->width, config->height, 8, 3, 4);*/

     if ( rfb_screen == NULL )
     {
             D_ERROR( "DirectFB/VNC: Couldn't set %dx%dx%d video mode\n",
                      config->width, config->height,
                      DFB_COLOR_BITS_PER_PIXEL(config->format) );

             fusion_skirmish_dismiss( &dfb_vnc->lock );

             return DFB_FAILURE;
     }

     if(DFB_COLOR_BITS_PER_PIXEL(config->format) == DSPF_RGB16)
     {
        rfb_screen->serverFormat.redShift = 11;
        rfb_screen->serverFormat.greenShift = 5;
        rfb_screen->serverFormat.blueShift = 0;
        rfb_screen->serverFormat.redMax = 31;
        rfb_screen->serverFormat.greenMax = 63;
        rfb_screen->serverFormat.blueMax = 31;
     }
    
     /* screen->serverFormat.trueColour=FALSE; */

     rfb_screen->frameBuffer = malloc(rfb_screen->width * rfb_screen->height * rfb_screen->depth / 8) ;
     
     if ( ! rfb_screen->frameBuffer )
     {
             fusion_skirmish_dismiss( &dfb_vnc->lock );

             return DFB_NOSYSTEMMEMORY;
     }

     /* Connect key handler */

     rfb_screen->kbdAddEvent = process_key_event;
     rfb_screen->ptrAddEvent = process_pointer_event;
     rfb_screen->newClientHook = newclient;
     
     /* Initialize VNC */
     
     rfbInitServer(rfb_screen);
     
     /* Now creating a thread to process the server */

     direct_thread_create( DTT_OUTPUT, vnc_server_thread, rfb_screen, "VNC Output" ); 
    
     /* Create a thread for refreshing if necessary */

     if ( !(config->surface_caps & (DSCAPS_DOUBLE | DSCAPS_TRIPLE)) )
          direct_thread_create( DTT_OUTPUT, vnc_refresh_thread, rfb_screen, "VNC Refresh" );
 
     fusion_skirmish_dismiss( &dfb_vnc->lock );

     return DFB_OK;
}

static void*
vnc_server_thread( DirectThread *thread, void *data )
{
   rfbRunEventLoop(rfb_screen, -1, FALSE); 
   return NULL;
}

static void*
vnc_refresh_thread( DirectThread *thread, void *data )
{
   while(1) {
      update_screen(dfb_vnc->primary, 0, 0, rfb_screen->width, rfb_screen->height);
      /* refresh frequency of 50 Hz */
      usleep(20000);
   }
   return NULL;
}


static DFBResult
dfb_vnc_update_screen_handler( DFBRegion *region )
{
     DFBResult    ret;
     CoreSurface *surface;
      
      D_ASSERT(dfb_vnc);
      
      surface = dfb_vnc->primary;

     fusion_skirmish_prevail( &dfb_vnc->lock );

     if (!region)
          ret = update_screen( surface, 0, 0, surface->width, surface->height );
     else
          ret = update_screen( surface,
                               region->x1,  region->y1,
                               region->x2 - region->x1 + 1,
                               region->y2 - region->y1 + 1 );

     fusion_skirmish_dismiss( &dfb_vnc->lock );

     return DFB_OK;
}

static DFBResult
dfb_vnc_set_palette_handler( CorePalette *palette )
{
     unsigned int i;
     uint8_t* map;

     rfb_screen->colourMap.count = palette->num_entries;
     rfb_screen->colourMap.is16 = false;
     rfb_screen->serverFormat.trueColour=FALSE;

     D_DEBUG( "DirectFB/VNC: setting colourmap of size %d\n", palette->num_entries);
     
     if( (map = (uint8_t*) malloc(rfb_screen->colourMap.count*sizeof(uint8_t)*3)) == NULL )
          return DFB_NOSYSTEMMEMORY;

     for (i=0; i<palette->num_entries; i++) {
          *(map++) = palette->entries[i].r;
          *(map++) = palette->entries[i].g;
          *(map++) = palette->entries[i].b;
     }

     fusion_skirmish_prevail( &dfb_vnc->lock );

     if( rfb_screen->colourMap.data.bytes )
          free(rfb_screen->colourMap.data.bytes);
     rfb_screen->colourMap.data.bytes = map;

     fusion_skirmish_dismiss( &dfb_vnc->lock );

     return DFB_OK;
}

int
dfb_vnc_call_handler( int   caller,
                      int   call_arg,
                      void *call_ptr,
                      void *ctx )
{
     switch (call_arg) {
          case VNC_SET_VIDEO_MODE:
               return dfb_vnc_set_video_mode_handler( call_ptr );

          case VNC_UPDATE_SCREEN:
               return dfb_vnc_update_screen_handler( call_ptr );

          case VNC_SET_PALETTE:
               return dfb_vnc_set_palette_handler( call_ptr );

          default:
               D_BUG( "unknown call" );
               break;
     }

     return 0;
}

static DFBResult
dfb_vnc_set_video_mode( CoreDFB *core, CoreLayerRegionConfig *config )
{
     int                    ret;
     CoreLayerRegionConfig *tmp = NULL;

     D_ASSERT( config != NULL );

     if (dfb_core_is_master( core ))
          return dfb_vnc_set_video_mode_handler( config );

     if (!fusion_is_shared( dfb_core_world(core), config )) {
          tmp = SHMALLOC( dfb_core_shmpool(core), sizeof(CoreLayerRegionConfig) );
          if (!tmp)
               return D_OOSHM();

          direct_memcpy( tmp, config, sizeof(CoreLayerRegionConfig) );
     }

     fusion_call_execute( &dfb_vnc->call, FCEF_NONE, VNC_SET_VIDEO_MODE,
                          tmp ? tmp : config, &ret );

     if (tmp)
          SHFREE( dfb_core_shmpool(core), tmp );

     return ret;
}

static DFBResult
dfb_vnc_update_screen( CoreDFB *core, DFBRegion *region )
{
     int        ret;
     DFBRegion *tmp = NULL;

     if (dfb_core_is_master( core ))
          return dfb_vnc_update_screen_handler( region );

     if (region) {
          if (!fusion_is_shared( dfb_core_world(core), region )) {
               tmp = SHMALLOC( dfb_core_shmpool(core), sizeof(DFBRegion) );
               if (!tmp)
                    return D_OOSHM();

               direct_memcpy( tmp, region, sizeof(DFBRegion) );
          }
     }

     fusion_call_execute( &dfb_vnc->call, FCEF_NONE, VNC_UPDATE_SCREEN,
                          tmp ? tmp : region, &ret );

     if (tmp)
          SHFREE( dfb_core_shmpool(core), tmp );

     return DFB_OK;
}

static DFBResult
dfb_vnc_set_palette( CorePalette *palette )
{
     int ret;

     fusion_call_execute( &dfb_vnc->call, FCEF_NONE, VNC_SET_PALETTE,
                          palette, &ret );

     return ret;
}

/**
  VNC Server setup
**/

static DFBEnumerationResult
attach_input_device( CoreInputDevice *device,
                      void            *ctx )
{
  vncInputDevice = device;
  return DFENUM_OK;
}

static void clientgone(rfbClientPtr cl)
{
  free(cl->clientData);
}

static enum rfbNewClientAction newclient(rfbClientPtr cl)
{
  cl->clientData = (void*)calloc(sizeof(ClientData),1);
  cl->clientGoneHook = clientgone;
  return RFB_CLIENT_ACCEPT;
}


static void 
process_pointer_event(int buttonMask, int x, int y, rfbClientPtr cl)
{

    DFBInputEvent evt;
    int button;

    if( vncInputDevice == NULL ){
        /* Attach to first input device */
        dfb_input_enumerate_devices( attach_input_device,NULL, DICAPS_ALL );
        D_ASSERT(vncInputDevice); 
    }

    ClientData* cd=cl->clientData;
    if(buttonMask != cd->oldButtonMask ) {
        int mask = buttonMask^cd->oldButtonMask;
        if( mask & (1 << 0)) { 
            button=DIBI_LEFT;
        } else if( mask & (1 << 1)) {
            button=DIBI_MIDDLE;
        } else if( mask & (1 << 2)) {
            button=DIBI_RIGHT;
        } else {
            return;
        }
        evt.flags = DIEF_NONE;
                
        if(cd->oldButton > button ) {
            evt.type = DIET_BUTTONRELEASE;
            evt.button=cd->oldButton;
            cd->oldButton=0;
        }else {
            evt.type = DIET_BUTTONPRESS;
            evt.button=button;
            cd->oldButton=button;
            cd->oldButtonMask=buttonMask;
        }
        dfb_input_dispatch( vncInputDevice, &evt );
        cd->oldx=x; 
        cd->oldy=y; 
        return;
    }

    evt.type    = DIET_AXISMOTION;
    evt.flags   = DIEF_AXISABS;

    if( cd->oldx != x ) {
          evt.axis    = DIAI_X;
          evt.axisabs = x;
          dfb_input_dispatch( vncInputDevice, &evt );
    }

    if( cd->oldy != y ) {
          evt.axis    = DIAI_Y;
          evt.axisabs = x;
          dfb_input_dispatch( vncInputDevice, &evt );
    }
    cd->oldx=x; 
    cd->oldy=y; 

    dfb_input_dispatch( vncInputDevice, &evt );
    rfbDefaultPtrAddEvent(buttonMask,x,y,cl);

}

/*
 * declaration of private data
 */
static void
process_key_event(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
    DFBInputEvent evt;
    if( vncInputDevice == NULL ){
        /* Attach to first input device */
        dfb_input_enumerate_devices( attach_input_device,NULL, DICAPS_ALL );
        D_ASSERT(vncInputDevice); 
    }
     if (down)
          evt.type = DIET_KEYPRESS;
     else
          evt.type = DIET_KEYRELEASE;
                                                                                  
     if (translate_key( key, &evt )) {
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

