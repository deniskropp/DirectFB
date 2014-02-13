

#include <++dfb.h>

#include "dfbapp.h"


class DFBTestFontBlend : public DFBApp {
private:
     IDirectFBFont font;

     /* called after initialization */
     virtual bool Setup( int width, int height ) {
          DFBFontDescription desc;

          desc.flags  = DFDESC_HEIGHT;
          desc.height = 256;

          font = m_dfb.CreateFont( FONT, desc );

          return true;
     }

     /* render callback */
     virtual void Render( IDirectFBSurface &surface ) {
          surface.Clear( 0, 0, 255, 255 );


          surface.SetFont( font );

          surface.SetColor( 0, 0, 0, 255 );
          surface.SetSrcBlendFunction( DSBF_INVSRCALPHA );
          surface.SetDstBlendFunction( DSBF_INVSRCALPHA );

          surface.DrawString( "Test Text", -1, 10, 10, (DFBSurfaceTextFlags)(DSTF_TOPLEFT | DSTF_BLEND_FUNCS) );
     }
};

int
main( int argc, char *argv[] )
{
     DFBTestFontBlend app;

     try {
          /* Initialize DirectFB command line parsing. */
          DirectFB::Init( &argc, &argv );

          /* Parse remaining arguments and run. */
          if (app.Init( argc, argv ))
               app.Run();
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

