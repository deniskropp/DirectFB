#include <config.h>

#include <stdio.h>

#include <pthread.h>

#include <directfb.h>

#include <core/fusion/lock.h>
#include <core/fusion/reactor.h>
#include <core/fusion/ref.h>

#include <misc/util.h>


static IDirectFB *dfb = NULL;

static DFBResult
init_directfb( int *argc, char **argv[] )
{
     DFBResult ret;
     
     /* Initialize DirectFB. */
     ret = DirectFBInit( argc, argv );
     if (ret)
          return DirectFBError( "DirectFBInit", ret );

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret)
          return DirectFBError( "DirectFBCreate", ret );

     return DFB_OK;
}

static void
deinit_directfb()
{
     if (dfb)
          dfb->Release( dfb );
}

static ReactionResult
react (const void    *msg_data,
       void          *ctx)
{
     if (ctx)
          (*(unsigned int *) ctx)++;

     return RS_OK;
}

#ifdef FUSION_FAKE
#define N_DISPATCH  1000000
#else
#define N_DISPATCH  40000
#endif

static void
bench_reactor()
{
     unsigned int   i, react_counter = 0;
     long long      t1, t2;
     FusionReactor *reactor;
     Reaction       reaction;
     Reaction       reaction2;

     reactor = reactor_new( 16 );
     if (!reactor) {
          fprintf( stderr, "Fusion Error\n" );
          return;
     }

     
     /* reactor attach/detach */
     t1 = dfb_get_millis();
     
     for (i=0; i<10000; i++) {
          reactor_attach( reactor, react, NULL, &reaction );
          reactor_detach( reactor, &reaction );
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor attach/detach        -> %8.2f k/sec\n", 10000 / (float)(t2 - t1) );

     
     /* reactor attach/detach (2nd) */
     reactor_attach( reactor, react, NULL, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<1000000; i++) {
          reactor_attach( reactor, react, NULL, &reaction2 );
          reactor_detach( reactor, &reaction2 );
     }
     
     t2 = dfb_get_millis();
     
     reactor_detach( reactor, &reaction );
     
     printf( "reactor attach/detach (2nd)  -> %8.2f k/sec\n", 1000000 / (float)(t2 - t1) );

     
     /* reactor dispatch */
     reactor_attach( reactor, react, &react_counter, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<N_DISPATCH; i++) {
          char msg[16];

          reactor_dispatch( reactor, msg, true );
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor dispatch             -> %8.2f k/sec  (%d%% arrived)\n",
             N_DISPATCH / (float)(t2 - t1), react_counter / (N_DISPATCH/100) );

#if 0
     while (react_counter < N_DISPATCH) {
          int old_counter = react_counter;

          sched_yield();

          if (old_counter == react_counter)
               break;
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor dispatch             -> %8.2f k/sec  (%d%% arrived)\n",
             N_DISPATCH / (float)(t2 - t1), react_counter / (N_DISPATCH/100) );
#endif     
     
     reactor_detach( reactor, &reaction );

     
     reactor_free( reactor );

     printf( "\n" );
}

static void
bench_ref()
{
     FusionResult ret;
     unsigned int i;
     long long    t1, t2;
     FusionRef    ref;

     ret = fusion_ref_init( &ref );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* ref up/down (local) */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          fusion_ref_up( &ref, false );
          fusion_ref_down( &ref, false );
     }
     
     t2 = dfb_get_millis();
     
     printf( "ref up/down (local)          -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     /* ref up/down (global) */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          fusion_ref_up( &ref, true );
          fusion_ref_down( &ref, true );
     }
     
     t2 = dfb_get_millis();

     printf( "ref up/down (global)         -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     fusion_ref_destroy( &ref );

     printf( "\n" );
}

static void
bench_skirmish()
{
     FusionResult   ret;
     unsigned int   i;
     long long      t1, t2;
     FusionSkirmish skirmish;

     ret = skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* skirmish prevail/dismiss */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          skirmish_prevail( &skirmish );
          skirmish_dismiss( &skirmish );
     }
     
     t2 = dfb_get_millis();
     
     printf( "skirmish prevail/dismiss     -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     skirmish_destroy( &skirmish );

     printf( "\n" );
}

static void
bench_pthread_mutex()
{
     unsigned int    i;
     long long       t1, t2;
     pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

     
     /* pthread_mutex lock/unlock */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
     }
     
     t2 = dfb_get_millis();
     
     printf( "pthread_mutex lock/unlock    -> %8.2f k/sec\n", (double)20000000 / (double)(t2 - t1) );

     
     pthread_mutex_destroy( &mutex );

     printf( "\n" );
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* DirectFB initialization. */
     ret = init_directfb( &argc, &argv );
     if (ret)
          goto out;

     printf( "\n" );

#ifdef FUSION_FAKE
     printf( "Fusion Benchmark (Fusion fake mode!)\n" );
#else
#ifdef LINUX_FUSION
     printf( "Fusion Benchmark (with Fusion Kernel Device)\n" );
#else
     printf( "Fusion Benchmark (without Fusion Kernel Device)\n" );
#endif
#endif

     printf( "\n" );
     
     
     bench_pthread_mutex();
     
     bench_skirmish();
     
     bench_ref();
     
     bench_reactor();


out:
     /* DirectFB deinitialization. */
     deinit_directfb();

     return ret;
}

