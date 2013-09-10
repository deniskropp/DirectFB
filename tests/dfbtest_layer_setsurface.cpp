#include <config.h>

#include <++dfb.h>


int
main( int argc, char *argv[] )
{
     IDirectFB             dfb;
     IDirectFBDisplayLayer layer;
     IDirectFBSurface      surface;

     try {
          /* Initialize DirectFB command line parsing. */
          DirectFB::Init( &argc, &argv );

          dfb = DirectFB::Create();

          layer = dfb.GetDisplayLayer( DLID_PRIMARY );

          layer.SetCooperativeLevel( DLSCL_EXCLUSIVE );

          DFBDisplayLayerConfig config;

          layer.GetConfiguration( &config );


          DFBSurfaceDescription desc;

          desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_HINTS);
          desc.width       = config.width;
          desc.height      = config.height;
          desc.pixelformat = config.pixelformat;
          desc.hints       = DSHF_LAYER;

          surface = dfb.CreateSurface( desc );

          surface.Clear( 100, 200, 255, 255 );
          surface.Flush();

          sleep( 1 );

          layer.SetSurface( surface );

          sleep( 100 );
     }
     catch (DFBException *ex) {
          /*
           * Exception has been caught, destructor of 'app' will deinitialize
           * anything at return time (below) that got initialized until now.
           */
          std::cerr << std::endl;
          std::cerr << "Caught exception!" << std::endl;
          std::cerr << "  -- " << ex << std::endl;
     }

     return 0;
}

