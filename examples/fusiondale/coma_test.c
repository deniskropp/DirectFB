#include <config.h>

#include <unistd.h>

#include <direct/list.h>
#include <direct/log.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <fusiondale.h>


static IComa        *coma;
static unsigned int  calls;

typedef struct {
     IComaComponent   *component;
} ComaTestInstance;

typedef enum {
     COMA_TEST_CALL1
} ComaTestMethods;


typedef struct {
     unsigned char bytes[20];
} DataCall1;



typedef struct {
     DirectLink         link;

     ComaTestInstance  *instance;

     ComaMethodID       method;
     void              *data;
     unsigned int       serial;
} DecoupledCall;


static DirectLink      *decoupled_calls;
static DirectMutex      decoupled_calls_lock;
static DirectWaitQueue  decoupled_calls_wq;


static void
DispatchCall1( ComaTestInstance *instance,
               DataCall1        *data,
               unsigned int      serial )
{
     int             i;
     IComaComponent *component = instance->component;

     for (i=1; i<20; i++) {
          if (data->bytes[i] != data->bytes[i-1]+1)
               D_BUG( "byte %d being %d does not match %d", i, data->bytes[i], data->bytes[i-1]+1 );
     }

     for (i=1; i<20; i++) {
          data->bytes[i] *= 3;
     }

     component->Return( component, DR_OK, serial );
}

static void *
DecoupledCall_thread_main( DirectThread *thread,
                           void         *ctx )
{
     while (true) {
          DecoupledCall *call;

          direct_mutex_lock( &decoupled_calls_lock );

          while (!decoupled_calls)
               direct_waitqueue_wait( &decoupled_calls_wq, &decoupled_calls_lock );

          call = (DecoupledCall*) decoupled_calls;

          direct_list_remove( &decoupled_calls, &call->link );

          direct_mutex_unlock( &decoupled_calls_lock );


          ComaTestInstance *instance  = call->instance;
          IComaComponent   *component = instance->component;

          switch (call->method) {
               case COMA_TEST_CALL1:
                    DispatchCall1( instance, call->data, call->serial );
                    break;

               default:
                    component->Return( component, DR_NOIMPL, call->serial );
                    break;
          }

          D_FREE( call );
     }

     return NULL;
}

static void
DecoupleCall( ComaTestInstance *instance,
              ComaMethodID      method,
              void             *data,
              unsigned int      serial )
{
     DecoupledCall  *call;
     IComaComponent *component = instance->component;

     call = D_CALLOC( 1, sizeof(DecoupledCall) );
     if (!call) {
          D_OOM();
          component->Return( component, DR_NOLOCALMEMORY, call->serial );
          return;
     }

     call->instance = instance;
     call->method   = method;
     call->data     = data;
     call->serial   = serial;

     direct_mutex_lock( &decoupled_calls_lock );

     direct_list_append( &decoupled_calls, &call->link );

     direct_mutex_unlock( &decoupled_calls_lock );

     direct_waitqueue_broadcast( &decoupled_calls_wq );
}

static void
ComaTestMethodFunc( void         *ctx,
                    ComaMethodID  method,
                    void         *arg,
                    unsigned int  serial )
{
     ComaTestInstance *instance = ctx;

     calls++;

     if (calls % 1000 == 0)
          direct_log_printf( NULL, "\rReceived %u calls", calls );

     DecoupleCall( instance, method, arg, serial );
}

static DirectResult
RUN_create( int index )
{
     DirectResult      ret;
     ComaTestInstance *instance;
     char              buf[20];

     snprintf( buf, sizeof(buf), "Tuner_%02d", index );

     instance = D_CALLOC( 1, sizeof(ComaTestInstance) );
     if (!instance)
          return D_OOM();

     ret = coma->CreateComponent( coma, buf, ComaTestMethodFunc,
                                  0,
                                  instance, &instance->component );
     if (ret) {
          D_DERROR( ret, "IComa::CreateComponent( '%s' ) failed!\n", buf );
          return ret;
     }

     instance->component->Activate( instance->component );

     return DR_OK;
}

static DirectResult
RUN_call( int index )
{
     DirectResult    ret;
     int             result;
     IComaComponent *instance;
     char            buf[20];
     void           *ptr;
     int             seed = 0;

     snprintf( buf, sizeof(buf), "Tuner_%02d", index );

     ret = coma->GetComponent( coma, buf, 99999, &instance );
     if (ret) {
          D_DERROR( ret, "IComa::GetComponent( '%s' ) failed!\n", buf );
          return ret;
     }

     while (true) {
          ret = coma->GetLocal( coma, sizeof(DataCall1), &ptr );
          if (ret) {
               D_DERROR( ret, "IComa::GetLocal( %zu ) failed!\n", sizeof(DataCall1) );
          }
          else {
               int        i;
               DataCall1 *data = ptr;

               for (i=0; i<20; i++)
                    data->bytes[i] = seed + i;

               ret = instance->Call( instance, COMA_TEST_CALL1, data, &result );
               if (ret)
                    D_DERROR( ret, "IComaComponent::Call( COMA_TEST_CALL1 ) failed!\n" );

               for (i=0; i<20; i++) {
                    if (data->bytes[i] != (seed + i) * 3)
                         D_BUG( "byte %d being %d does not match %d", i, data->bytes[i], (seed + i) * 3 );
               }
          }
     }

     return DR_OK;
}

static void *
RUN_call_thread_main( DirectThread *thread,
                      void         *arg )
{
     RUN_call( (long) arg );

     return NULL;
}

static DirectResult
RUN_call_threaded( int index,
                   int thread_count )
{
     int           i;
     DirectThread *threads[thread_count];

     for (i=0; i<thread_count; i++) {
          threads[i] = direct_thread_create( DTT_DEFAULT, RUN_call_thread_main, (void*)(long)index, "Test" );
     }

     for (i=0; i<thread_count; i++) {
          direct_thread_join( threads[i] );
          direct_thread_destroy( threads[i] );
     }

     return DR_OK;
}

int
main( int argc, char *argv[] )
{
     DirectResult  ret;
     IFusionDale  *dale;
     int           i;

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

     ret = dale->EnterComa( dale, "Coma Test", &coma );
     if (ret) {
          D_DERROR( ret, "IFusionDale::EnterComa('Coma Test') failed!\n" );
          return -3;
     }

     direct_mutex_init( &decoupled_calls_lock );
     direct_waitqueue_init( &decoupled_calls_wq );

     for (i=1; i<argc; i++) {
          if (strncmp( argv[i], "create", 6 ) == 0) {
               int n, num;
               int threads = 1;

               if (sscanf( argv[i]+6, "%d,%d", &num, &threads ) >= 1) {
                    for (n=0; n<num; n++) {
                         RUN_create( n );
                    }

                    for (n=0; n<threads; n++) {
                         direct_thread_create( DTT_DEFAULT, DecoupledCall_thread_main, NULL, "Process" );
                    }

                    pause();
               }
               else
                    fprintf( stderr, "Wrong number '%s'!\n", argv[i]+6 );
          }
          else if (strncmp( argv[i], "call", 4 ) == 0) {
               int index, threads = 1;

               if (sscanf( argv[i]+4, "%d,%d", &index, &threads ) >= 1) {
                    RUN_call_threaded( index, threads );
               }
               else
                    fprintf( stderr, "Wrong number '%s'!\n", argv[i]+4 );
          }
     }

     direct_waitqueue_deinit( &decoupled_calls_wq );
     direct_mutex_deinit( &decoupled_calls_lock );

     coma->Release( coma );

     dale->Release( dale );

     return 0;
}

