#include <config.h>

#include <unistd.h>

#include <direct/log.h>
#include <direct/messages.h>

#include <fusiondale.h>

#include "ce_tuner.h"


static IComa *coma;


typedef struct {
     IComaComponent   *component;
     int               frequency;
} AVTuner;

typedef struct {
     int               frequency;
     int               snr;
} OnStationData;


static int
SetFrequency( AVTuner *tuner, int *frequency )
{
     void           *ptr;
     OnStationData  *data;
     IComaComponent *component = tuner->component;

     direct_log_printf( NULL, "AV/Tuner: %s( %d ) called!\n", __FUNCTION__, *frequency );

     tuner->frequency = *frequency * 1000;


     coma->Allocate( coma, sizeof(OnStationData), &ptr );

     data = ptr;
     data->frequency = tuner->frequency;
     data->snr       = 80;

     component->Notify( component, AV_TUNER_ONSTATIONFOUND, data );

     return DR_OK;
}

static int
SetGains( AVTuner *tuner, AVTunerSetGainsCtx *gains )
{
     void           *ptr;
     OnStationData  *data;
     IComaComponent *component = tuner->component;

     direct_log_printf( NULL, "AV/Tuner: %s( %p, %d [%d...] ) called!\n", __FUNCTION__,
                        gains, gains->num, gains->gains[0] );

     coma->Allocate( coma, sizeof(OnStationData), &ptr );

     data = ptr;
     data->frequency = tuner->frequency;
     data->snr       = 30;

     component->Notify( component, AV_TUNER_ONSTATIONLOST, data );

     return DR_OK;
}

static void
AVTunerMethodFunc( void         *ctx,
                   ComaMethodID  method,
                   void         *arg,
                   unsigned int  magic )
{
     int             ret;
     AVTuner        *tuner     = ctx;
     IComaComponent *component = tuner->component;

     switch (method) {
          case AV_TUNER_SETFREQUENCY:
               ret = SetFrequency( ctx, arg );
               break;

          case AV_TUNER_SETGAINS:
               ret = SetGains( ctx, arg );
               break;

          default:
               ret = DR_NOIMPL;
               break;
     }

     component->Return( component, ret, magic );
}

static void
AVTunerOnStationFoundDispatchCallback( void               *ctx,
                                       ComaNotificationID  notification,
                                       void               *arg )
{
//     OnStationData *data = arg;

     direct_log_printf( NULL, "AV/Tuner: %s( %p, %lu, %p ) called!\n",
                        __FUNCTION__, ctx, notification, arg );

//     coma->Deallocate( coma, data );
}

static void
AVTunerOnStationLostDispatchCallback( void               *ctx,
                                      ComaNotificationID  notification,
                                      void               *arg )
{
//     OnStationData *data = arg;

     direct_log_printf( NULL, "AV/Tuner: %s( %p, %lu, %p ) called!\n",
                        __FUNCTION__, ctx, notification, arg );

//     coma->Deallocate( coma, data );
}

int
main( int argc, char *argv[] )
{
     DirectResult  ret;
     AVTuner       tuner;
     IFusionDale  *dale;

     //dfb_config_init( &argc, &argv );

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
          return -3;
     }


     ret = coma->CreateComponent( coma, "Tuner", AVTunerMethodFunc,
                                  _AV_TUNER_NUM_NOTIFICATIONS,
                                  &tuner, &tuner.component );
     if (ret) {
          D_DERROR( ret, "IComa::CreateComponent('Tuner') failed!\n" );
          return -4;
     }


     ret = tuner.component->InitNotification( tuner.component, AV_TUNER_ONSTATIONFOUND,
                                              AVTunerOnStationFoundDispatchCallback, &tuner, CNF_DEALLOC_ARG );
     if (ret) {
          D_DERROR( ret, "IComaComponent::InitNotification(AV_TUNER_ONSTATIONFOUND) failed!\n" );
          return -5;
     }

     ret = tuner.component->InitNotification( tuner.component, AV_TUNER_ONSTATIONLOST,
                                              AVTunerOnStationLostDispatchCallback, &tuner, CNF_DEALLOC_ARG );
     if (ret) {
          D_DERROR( ret, "IComaComponent::InitNotification(AV_TUNER_ONSTATIONFOUND) failed!\n" );
          return -5;
     }

     tuner.component->Activate( tuner.component );


     pause();


     tuner.component->Release( tuner.component );

     coma->Release( coma );
     dale->Release( dale );

     return 0;
}

