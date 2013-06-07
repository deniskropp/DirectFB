#include <directfb.h>

#include <unistd.h>

#define APP "dfbtest_stereo"

int
main( int argc, char *argv[] )
{
     DFBResult               ret;
     IDirectFB              *dfb        = NULL;
     IDirectFBDisplayLayer  *layer      = NULL;
     IDirectFBSurface       *surface    = NULL;
     IDirectFBScreen        *screen     = NULL;
     int                     width, height;
     DFBDisplayLayerConfig   layer_config;
     DFBScreenEncoderConfig  encoder_config;

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

     /* Wait for TV to sync and stuff. */
     usleep(300000);

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

     ret = screen->SetEncoderConfiguration( screen, 0, &encoder_config);
     if (ret) {

          encoder_config.frequency      = DSEF_60HZ;
          encoder_config.framing        = DSEPF_STEREO_SIDE_BY_SIDE_HALF;
          encoder_config.resolution     = DSOR_1280_720;

          ret = screen->SetEncoderConfiguration( screen, 0, &encoder_config);

          if (ret) {
               D_DERROR( ret, APP ": IDirectFBScreen::TestEncoderConfig() failed!\n" );
               goto out;
          }
     }

     /*
      * Setup primary layer as stereo.
      */
     ret = layer->SetCooperativeLevel( layer, DLSCL_EXCLUSIVE );
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

     ret = layer->GetSurface( layer, &surface );
     if (ret) {
          D_DERROR( ret, APP ": IDirectFBDisplayLayer::GetSurface() failed!\n" );
          goto out;
     }

     surface->GetSize( surface, &width, &height );

     printf( "Layer resolution is %dx%d\n", width, height );

     while (true) {
          /*
           * Draw left
           */
          surface->SetStereoEye( surface, DSSE_LEFT );

          surface->Clear( surface, 0, 0, 0, 0 );

          surface->SetColor( surface, 0xff, 0xff, 0xff, 0xff );
          surface->FillRectangle( surface, 200, 200, 500, 500 );

          surface->SetColor( surface, 0x00, 0x00, 0xff, 0xff );
          surface->FillRectangle( surface, 310, 400, 500, 500 );


          /*
           * Draw right
           */
          surface->SetStereoEye( surface, DSSE_RIGHT );

          surface->Clear( surface, 0, 0, 0, 0 );

          surface->SetColor( surface, 0xff, 0xff, 0xff, 0xff );
//          surface->FillRectangle( surface, 200, 200, 500, 500 );

          surface->SetColor( surface, 0x00, 0x00, 0xff, 0xff );
          surface->FillRectangle( surface, 290, 400, 500, 500 );


          /*
           * Flip both
           */
          surface->FlipStereo( surface, NULL, NULL, DSFLIP_WAITFORSYNC );
     }

out:
     if (screen)
          screen->Release( screen );

     if (surface)
          surface->Release( surface );

     if (layer)
          layer->Release( layer );

     if (dfb)
          dfb->Release( dfb );

     return 0;
}

