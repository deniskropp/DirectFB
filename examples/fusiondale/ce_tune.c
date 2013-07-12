#include <config.h>

#include <stdlib.h>
#include <unistd.h>

#include <direct/log.h>
#include <direct/messages.h>

#include <fusiondale.h>

#include "ce_tuner.h"


int
main( int argc, char *argv[] )
{
     DirectResult        ret;
     int                 result;
     int                 frequency = 2342;
     void               *ptr;
     IFusionDale        *dale;
     IComa              *coma;
     IComaComponent     *tuner;

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


     if (argc > 1) {
          char *end;

          frequency = strtoul( argv[1], &end, 10 );
     }


     ret = coma->GetComponent( coma, "Tuner", 7000, &tuner );
     if (ret) {
          D_DERROR( ret, "IComa::GetComponent('Tuner') failed!\n" );
          return -2;
     }

     /* 
      * Test SetFrequency
      */
     while (1) {
          direct_log_printf( NULL, "AV/Tune: Calling SetFrequency( %d )...\n", frequency );
     
          ret = coma->GetLocal( coma, sizeof(int), &ptr );
          if (ret) {
               D_DERROR( ret, "IComa::GetLocal( %zu ) failed!\n", sizeof(AVTunerSetGainsCtx) );
          }
          else {
               int *freq = ptr;
     
               *freq = frequency;
     
               ret = tuner->Call( tuner, AV_TUNER_SETFREQUENCY, freq, &result );
               if (ret)
                    D_DERROR( ret, "IComaComponent::Call( TUNER_SETFREQUENCY, %d ) failed!\n", frequency );
               else
                    direct_log_printf( NULL, "AV/Tune: ...SetFrequency( %d ) returned %d.\n", frequency, result );
          }

          sleep(5);
     }

     sleep( 2 );

     /* 
      * Test SetGains
      */
     direct_log_printf( NULL, "AV/Tune: Allocating %zu bytes...\n", sizeof(AVTunerSetGainsCtx) );

     ret = coma->GetLocal( coma, sizeof(AVTunerSetGainsCtx), &ptr );
     if (ret) {
          D_DERROR( ret, "IComa::GetLocal( %zu ) failed!\n", sizeof(AVTunerSetGainsCtx) );
     }
     else {
          AVTunerSetGainsCtx *gains = ptr;

          gains->num      = 3;
          gains->gains[0] = 23;
          gains->gains[1] = 24;
          gains->gains[2] = 25;

          direct_log_printf( NULL, "AV/Tune: Calling SetGains( %p )...\n", gains );

          ret = tuner->Call( tuner, AV_TUNER_SETGAINS, gains, &result );
          if (ret)
               D_DERROR( ret, "IComaComponent::Call( TUNER_SETGAINS, %p ) failed!\n", gains );
          else
               direct_log_printf( NULL, "AV/Tune: ...SetGains( %p ) returned %d.\n", gains, result );

          direct_log_printf( NULL, "AV/Tune: Deallocating %zu bytes...\n", sizeof(AVTunerSetGainsCtx) );

          coma->FreeLocal( coma );
     }

     direct_log_printf( NULL, "AV/Tune: Exiting...\n" );

     tuner->Release( tuner );
     coma->Release( coma );
     dale->Release( dale );

     return 0;
}

