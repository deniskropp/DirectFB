#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <direct/direct.h>
#include <direct/messages.h>

#include <directfb.h>
#include <directfb_util.h>

#define FONT  DATADIR "/decker.dgiff"


static IDirectFB       *g_dfb;
static IDirectFBScreen *g_screen;
static IDirectFBFont   *g_font;

static DFBDimension     g_screen_size;

typedef struct {
     IDirectFBDisplayLayer *layer;
     IDirectFBSurface      *surface;

     int                    dx;
     int                    dy;

     int                    x;
     int                    y;
     int                    width;
     int                    height;
} Plane;


static Plane g_planes[64];
static int   g_plane_count;


static DFBResult
InitializeLayer( DFBDisplayLayerID  layer_id,
                 int                width,
                 int                height,
                 Plane             *plane )
{
     static DFBColor colors[8] = {
          { 0xff, 0x00, 0x00, 0xff },
          { 0xff, 0xff, 0x00, 0x00 },
          { 0xff, 0xff, 0xff, 0x00 },
          { 0xff, 0x00, 0xff, 0x00 },
          { 0xff, 0xff, 0x00, 0xff },
          { 0xff, 0x00, 0xff, 0xff },
          { 0xff, 0xff, 0xff, 0xff },
          { 0xff, 0x80, 0x80, 0x80 }
     };

     DFBResult              ret;
     DFBDisplayLayerConfig  config;
     IDirectFBDisplayLayer *layer   = NULL;
     IDirectFBSurface      *surface = NULL;
     char                   buf[256];
     DFBSurfacePixelFormat  format = 0;

     D_INFO( "dfbtest_layers: Initializing layer with ID %d...\n", layer_id );


     ret = g_dfb->GetDisplayLayer( g_dfb, layer_id, &layer );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFB::GetDisplayLayer() failed!\n" );
          goto error;
     }

     ret = layer->SetCooperativeLevel( layer, DLSCL_EXCLUSIVE );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFBDisplayLayer::SetCooperativeLevel() failed!\n" );
          goto error;
     }

     config.flags       = DLCONF_PIXELFORMAT;
//   if ( layer_id == 1)
//        config.pixelformat = DSPF_NV12;
//   else
          config.pixelformat = DSPF_ARGB;

     if (layer->TestConfiguration( layer, &config, NULL ) == DFB_OK)
          format = config.pixelformat;
     else
          format = DSPF_RGB32;


//   if (layer->TestConfiguration( layer, &config, NULL ) == DFB_OK) {
//     format = DSPF_ARGB;
//
//         config.options = DLOP_ALPHACHANNEL;
//   }
//   else
           config.options = DLOP_NONE;

     /* Fill in configuration. */
     config.flags        = DLCONF_BUFFERMODE | DLCONF_OPTIONS | DLCONF_SURFACE_CAPS;
     if (width && height) {
          config.flags |= DLCONF_WIDTH | DLCONF_HEIGHT;

          config.width   = width;
          config.height  = height;

          plane->x       = (layer_id * 200) % (g_screen_size.w - width);
          plane->y       = (layer_id * 200) % (g_screen_size.h - height);

     }
     else {
          plane->x       = 0;
          plane->y       = 0;
     }

     config.buffermode   = DLBM_FRONTONLY;
     config.surface_caps = DSCAPS_NONE;

     if (format) {
          config.flags       |= DLCONF_PIXELFORMAT;
          config.pixelformat  = format;
     }


     /* Set new configuration. */
     ret = layer->SetConfiguration( layer, &config );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFBDisplayLayer::SetConfiguration() failed!\n" );
          goto error;
     }

     layer->SetOpacity( layer, 0xbb );

     /* Get the layer surface. */
     ret = layer->GetSurface( layer, &surface );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFBDisplayLayer::GetSurface() failed!\n" );
          goto error;
     }

     surface->GetPixelFormat( surface, &format );

     surface->GetSize( surface, &width, &height );

     surface->SetDrawingFlags( surface, DSDRAW_SRC_PREMULTIPLY );

     surface->SetColor( surface, colors[layer_id].r, colors[layer_id].g, colors[layer_id].b, 0xbb );
     surface->FillRectangle( surface, 0, 0, width, height );

     surface->SetColor( surface, colors[layer_id].r, colors[layer_id].g, colors[layer_id].b, 0x99 );
     surface->FillRectangle( surface, 100, 100, width - 200, height - 200 );

     surface->SetColor( surface, 0xff, 0xff, 0xff, 0xff );
     surface->SetFont( surface, g_font );

     snprintf( buf, sizeof(buf), "Plane %u %dx%d (%s)", layer_id, width, height, dfb_pixelformat_name(format) );     
     surface->DrawString( surface, buf, -1, 20, 20, DSTF_TOPLEFT );

     surface->Flip( surface, NULL, DSFLIP_NONE );


     plane->layer   = layer;
     plane->surface = surface;

     plane->dx      = (layer_id & 1) ? 1 : -1;
     plane->dy      = (layer_id & 2) ? 1 : -1;

     plane->width   = width;
     plane->height  = height;

     return DFB_OK;


error:
     if (layer)
          layer->Release( layer );

     return ret;
}


static DFBEnumerationResult
DisplayLayerCallback( DFBDisplayLayerID           layer_id,
                      DFBDisplayLayerDescription  desc,
                      void                       *callbackdata )
{
     if (InitializeLayer( layer_id, 400, 400, &g_planes[g_plane_count] ) == DFB_OK) {
          g_plane_count++;
     }
     else {
          D_INFO( "Layer id %d could not be set to 400x400, trying fullscreen\n", layer_id );
          if (InitializeLayer( layer_id, 0, 0, &g_planes[g_plane_count] ) == DFB_OK)
               g_plane_count++;
     }

     return DFENUM_OK;
}


static void
TickPlane( Plane *plane )
{
     if (plane->x == g_screen_size.w - plane->width)
          plane->dx = -1;
     else if (plane->x == 0)
          plane->dx = 1;

     if (plane->y == g_screen_size.h - plane->height)
          plane->dy = -1;
     else if (plane->y == 0)
          plane->dy = 1;

     plane->x += plane->dx;
     plane->y += plane->dy;

     plane->layer->SetScreenPosition( plane->layer, plane->x, plane->y );
     plane->surface->Flip( plane->surface, 0, 0);
}

int
main( int argc, char *argv[] )
{
     DFBResult          ret;
     DFBFontDescription fdsc;
     int                i;

     direct_initialize();


     D_INFO( "dfbtest_layers: Starting test program...\n" );

     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: DirectFBInit() failed!\n" );
          return ret;
     }

     ret = DirectFBCreate( &g_dfb );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: DirectFBCreate() failed!\n" );
          return ret;
     }

     ret = g_dfb->GetScreen( g_dfb, DSCID_PRIMARY, &g_screen );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFB::GetScreen( PRIMARY ) failed!\n" );
          return ret;
     }

     g_screen->GetSize( g_screen, &g_screen_size.w, &g_screen_size.h );

     D_INFO( "dfbtest_layers: Screen size is %ux%u\n", g_screen_size.w, g_screen_size.h );

     fdsc.flags  = DFDESC_HEIGHT;
     fdsc.height = 18;

     ret = g_dfb->CreateFont( g_dfb, FONT, &fdsc, &g_font );
     if (ret) {
          D_DERROR( ret, "dfbtest_layers: IDirectFB::CreateFont( %s ) failed!\n", FONT );
          return ret;
     }

     g_dfb->EnumDisplayLayers( g_dfb, DisplayLayerCallback, NULL );

     while (1) {
          for (i=0; i<g_plane_count; i++)
               TickPlane( &g_planes[i] );

          usleep( 10000 );
     }


     g_font->Release( g_font );
     g_dfb->Release( g_dfb );

     direct_shutdown();

     return 0;
}

