#include <config.h>

#include <unistd.h>

#include <direct/log.h>
#include <direct/messages.h>

#include <fusiondale.h>

#include "ce_tuner.h"


static void
OnStationFound( void *ctx, void *arg )
{
     direct_log_printf( NULL, "AV/TuningMonitor: %s( %d ) called!\n", __FUNCTION__, * (int*) arg );
}

static void
OnStationLost( void *ctx, void *arg )
{
     direct_log_printf( NULL, "AV/TuningMonitor: %s( %d ) called!\n", __FUNCTION__, * (int*) arg );
}


int
main( int argc, char *argv[] )
{
     DirectResult   ret;
     IFusionDale      *dale;
     IComa            *coma;
     IComaComponent   *tuner;

     ret = FusionDaleInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "FusionDaleInit() failed!\n" );
          return -1;
     }

     ret = FusionDaleCreate( &dale );
     if (ret) {
          D_DERROR( ret, "FusionDaleCreate() failed!\n" );
          return -2;
     }

     ret = dale->EnterComa( dale, "AV Platform", &coma );
     if (ret) {
          D_DERROR( ret, "IFusionDale::EnterComa('AV Platform') failed!\n" );
          return -1;
     }

     ret = coma->GetComponent( coma, "Tuner", 7000, &tuner );
     if (ret) {
          D_DERROR( ret, "IComa::GetComponent('Tuner') failed!\n" );
          return -2;
     }

     ret = tuner->Listen( tuner, AV_TUNER_ONSTATIONFOUND, OnStationFound, NULL );
     if (ret)
          D_DERROR( ret, "IComaComponent::Listen( 'AV_TUNER_ONSTATIONFOUND' ) failed!\n" );

     ret = tuner->Listen( tuner, AV_TUNER_ONSTATIONLOST, OnStationLost, NULL );
     if (ret)
          D_DERROR( ret, "IComaComponent::Listen( 'AV_TUNER_ONSTATIONLOST' ) failed!\n" );

     pause();

     tuner->Release( tuner );
     coma->Release( coma );
     dale->Release( dale );

     return 0;
}

