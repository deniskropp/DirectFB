#include <unistd.h>

#include <fusionsound.h>

int
main( int argc, char *argv[] )
{
     DFBResult     ret;
     IFusionSound *sound;

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          DirectFBErrorFatal( "FusionSoundInit", ret );

     ret = FusionSoundCreate( &sound );
     if (ret)
          DirectFBErrorFatal( "FusionSoundCreate", ret );

     pause();

     sound->Release (sound);

     return 0;
}
