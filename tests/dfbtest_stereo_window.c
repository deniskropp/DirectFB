#include <directfb.h>

#include <unistd.h>

#define APP "dfbtest_stereo_window"

static DFBResult
create_window( IDirectFBDisplayLayer  *layer,
               int                     x,
               int                     y,
               IDirectFBWindow       **ret_window,
               IDirectFBSurface      **ret_surface )
{
     DFBResult             ret;
     IDirectFBWindow      *window;
     IDirectFBSurface     *surface;
     DFBWindowDescription  window_desc;

     window_desc.flags  = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY | DWDESC_CAPS;
     window_desc.width  = 400;
     window_desc.height = 300;
     window_desc.posx   = x;
     window_desc.posy   = y;
     window_desc.caps   = DWCAPS_ALPHACHANNEL | DWCAPS_LR_MONO;

     ret = layer->CreateWindow( layer, &window_desc, &window );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBDisplayLayer::CreateWindow() failed!\n" );
          return ret;
     }

     ret = window->GetSurface( window, &surface );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBWindow::GetSurface() failed!\n" );
          return ret;
     }

     surface->Clear( surface, 0x00, 0x80, 0xa0, 0xd0 );

     surface->SetColor( surface, 0x00, 0xc0, 0xff, 0xff );
     surface->DrawRectangle( surface, 0, 0, 400, 300 );
     surface->DrawRectangle( surface, 1, 1, 399, 299 );

     surface->Flip( surface, NULL, DSFLIP_NONE );

     window->SetOpacity( window, 0xff );

     *ret_window  = window;
     *ret_surface = surface;

     return DFB_OK;
}
               

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     IDirectFB              *dfb        = NULL;
     IDirectFBDisplayLayer  *layer      = NULL;
     IDirectFBScreen        *screen     = NULL;
     IDirectFBWindow        *window1    = NULL;
     IDirectFBWindow        *window2    = NULL;
     IDirectFBSurface       *surface1   = NULL;
     IDirectFBSurface       *surface2   = NULL;
     DFBDisplayLayerConfig   layer_config;
     DFBScreenEncoderConfig  encoder_config;
     int                     z    = 0;
     int                     zdir = -1;

     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, APP ": DirectFBInit() failed!\n" );
          goto out;
     }

     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, APP ": DirectFBCreate() failed!\n" );
          goto out;
     }

     /* Setup screen. */
     ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFB::GetDisplayLayer() failed!\n" );
          goto out;
     }

     ret = layer->GetScreen( layer, &screen );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBDisplayLayer::GetScreen() failed!\n" );
          goto out;
     }


     encoder_config.flags          = (DFBScreenEncoderConfigFlags)(DSECONF_TV_STANDARD | DSECONF_SCANMODE   | DSECONF_FREQUENCY | 
                                                                   DSECONF_CONNECTORS  | DSECONF_RESOLUTION | DSECONF_FRAMING);
     encoder_config.tv_standard    = DSETV_DIGITAL;
     encoder_config.out_connectors = (DFBScreenOutputConnectors)(DSOC_COMPONENT | DSOC_HDMI);
     encoder_config.scanmode       = DSESM_PROGRESSIVE;
     encoder_config.frequency      = DSEF_24HZ;
     encoder_config.framing        = DSEPF_STEREO_FRAME_PACKING;
     encoder_config.resolution     = DSOR_1920_1080;

     ret = screen->SetEncoderConfiguration( screen, 0, &encoder_config );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBScreen::SetEncoderConfig() failed!\n" );
          goto out;
     }


     /* 
      * Setup primary layer as stereo.
      */
     ret = layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBDisplayLayer::SetCooperativeLevel() failed!\n" );
          goto out;
     }

     layer_config.flags        = (DFBDisplayLayerConfigFlags)(DLCONF_SURFACE_CAPS | DLCONF_OPTIONS | DLCONF_BUFFERMODE);
     layer_config.options      = (DFBDisplayLayerOptions)(DLOP_STEREO | DLOP_ALPHACHANNEL);
     layer_config.surface_caps = (DFBSurfaceCapabilities)(DSCAPS_PREMULTIPLIED | DSCAPS_VIDEOONLY | DSCAPS_STEREO);
     layer_config.buffermode   = DLBM_BACKVIDEO;

     ret = layer->SetConfiguration( layer, &layer_config );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBDisplayLayer::SetConfiguration() failed!\n" );
          goto out;
     }


     /* 
      * Create first window.
      */
     ret = create_window( layer, 200, 200, &window1, &surface1 );
     if (ret)
          goto out;


     /* 
      * Create second window.
      */
     ret = create_window( layer, 300, 300, &window2, &surface2 );
     if (ret)
          goto out;


     while (true) {
          z += zdir;

          if (z == 20)
               zdir = -1;
          else if (z == -20)
               zdir = 1;

          ret = window2->SetStereoDepth( window2, z );
          if (ret) {
               D_DERROR( ret, APP ": IDirectFBWindow::SetStereoDepth() failed!\n" );
               goto out;
          }

          usleep( 100000 );
     }

out:
     if (screen)
          screen->Release( screen );

     if (surface2)
          surface2->Release( surface2 );

     if (surface1)
          surface1->Release( surface1 );

     if (window2)
          window2->Release( window2 );

     if (window1)
          window1->Release( window1 );

     if (layer)
          layer->Release( layer );

     if (dfb)
          dfb->Release( dfb );

     return 0;
}

