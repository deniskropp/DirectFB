#define _GNU_SOURCE

#include <config.h>

#include <stdio.h>

#include <pthread.h>

#include <directfb.h>

#include <core/fusion/lock.h>
#include <core/fusion/property.h>
#include <core/fusion/reactor.h>
#include <core/fusion/ref.h>

#include <misc/util.h>


static ReactionResult
react (const void    *msg_data,
       void          *ctx)
{
     if (ctx)
          (*(unsigned int *) ctx)++;

     return RS_OK;
}

#ifdef FUSION_FAKE
#define N_DISPATCH  4000000
#else
#define N_DISPATCH  1000000
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
     
     for (i=0; i<1000000; i++) {
          reactor_attach( reactor, react, NULL, &reaction );
          reactor_detach( reactor, &reaction );
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor attach/detach                 -> %8.2f k/sec\n", 1000000 / (float)(t2 - t1) );

     
     /* reactor attach/detach (2nd) */
     reactor_attach( reactor, react, NULL, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<1000000; i++) {
          reactor_attach( reactor, react, NULL, &reaction2 );
          reactor_detach( reactor, &reaction2 );
     }
     
     t2 = dfb_get_millis();
     
     reactor_detach( reactor, &reaction );
     
     printf( "reactor attach/detach (2nd)           -> %8.2f k/sec\n", 1000000 / (float)(t2 - t1) );

     
     /* reactor dispatch */
     reactor_attach( reactor, react, &react_counter, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<N_DISPATCH; i++) {
          char msg[16];

          reactor_dispatch( reactor, msg, true, NULL );
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor dispatch                      -> %8.2f k/sec  (%d%% arrived)\n",
             N_DISPATCH / (float)(t2 - t1), react_counter / (N_DISPATCH/100) );

#if 1
     if (react_counter < N_DISPATCH) {
          while (react_counter < N_DISPATCH) {
               unsigned int old_counter = react_counter;

               sched_yield();

               if (old_counter == react_counter)
                    break;
          }
          
          t2 = dfb_get_millis();
          
          printf( "  (after waiting for arrival)    -> %8.2f k/sec  (%d%% arrived)\n",
                  N_DISPATCH / (float)(t2 - t1), react_counter / (N_DISPATCH/100) );
     }
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
     
     printf( "ref up/down (local)                   -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     /* ref up/down (global) */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          fusion_ref_up( &ref, true );
          fusion_ref_down( &ref, true );
     }
     
     t2 = dfb_get_millis();

     printf( "ref up/down (global)                  -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     fusion_ref_destroy( &ref );

     printf( "\n" );
}

static void
bench_property()
{
     FusionResult   ret;
     unsigned int   i;
     long long      t1, t2;
     FusionProperty property;

     ret = fusion_property_init( &property );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* property lease/cede */
     t1 = dfb_get_millis();
     
     for (i=0; i<4000000; i++) {
          fusion_property_lease( &property );
          fusion_property_cede( &property );
     }
     
     t2 = dfb_get_millis();
     
     printf( "property lease/cede                   -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     fusion_property_destroy( &property );

     printf( "\n" );
}

#if 0
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
     
     printf( "skirmish prevail/dismiss              -> %8.2f k/sec\n", 4000000 / (float)(t2 - t1) );

     
     skirmish_destroy( &skirmish );

     printf( "\n" );
}
#endif

static void *
prevail_dismiss_loop( void *arg )
{
     unsigned int    i;
     FusionSkirmish *skirmish = (FusionSkirmish *) arg;

     for (i=0; i<700000; i++) {
          skirmish_prevail( skirmish );
          skirmish_dismiss( skirmish );
     }

     return NULL;
}

static void
bench_skirmish_threaded()
{
     FusionResult   ret;
     int            i;
     FusionSkirmish skirmish;

     ret = skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* skirmish prevail/dismiss (1-5 threads) */
     for (i=1; i<=5; i++) {
          int       t;
          long long t1, t2;
          pthread_t threads[i];
          
          t1 = dfb_get_millis();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, prevail_dismiss_loop, &skirmish );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          t2 = dfb_get_millis();

          printf( "skirmish prevail/dismiss (%d threads)  -> %8.2f k/sec\n",
                  i, (double)i * (double)700000 / (double)(t2 - t1) );
     }

     
     skirmish_destroy( &skirmish );

     printf( "\n" );
}

static void *
spin_lock_unlock_loop( void *arg )
{
     int                 i;
     pthread_spinlock_t *lock = (pthread_spinlock_t *) arg;

     for (i=0; i<2000000; i++) {
          pthread_spin_lock( lock );
          pthread_spin_unlock( lock );
     }

     return NULL;
}

static void
bench_spinlock_threaded()
{
     FusionResult       ret;
     int                i;
     pthread_spinlock_t lock;

     ret = pthread_spin_init( &lock, true );
     if (ret) {
          fprintf( stderr, "pthread error %d\n", ret );
          return;
     }

     
     /* spinlock lock/unlock (1-5 threads) */
     for (i=1; i<=5; i++) {
          int       t;
          long long t1, t2;
          pthread_t threads[i];
          
          t1 = dfb_get_millis();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, spin_lock_unlock_loop, (void*)&lock );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          t2 = dfb_get_millis();

          printf( "spinlock lock/unlock (%d threads)      -> %8.2f k/sec\n",
                  i, (double)i * (double)2000000 / (double)(t2 - t1) );
     }

     
     pthread_spin_destroy( &lock );

     printf( "\n" );
}

static void *
mutex_lock_unlock_loop( void *arg )
{
     int              i;
     pthread_mutex_t *lock = (pthread_mutex_t *) arg;

     for (i=0; i<2000000; i++) {
          pthread_mutex_lock( lock );
          pthread_mutex_unlock( lock );
     }

     return NULL;
}

static void
bench_mutex_threaded()
{
     FusionResult    ret;
     int             i;
     pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

     ret = pthread_mutex_init( &lock, NULL );
     if (ret) {
          fprintf( stderr, "pthread error %d\n", ret );
          return;
     }

     
     /* mutex lock/unlock (1-5 threads) */
     for (i=1; i<=5; i++) {
          int       t;
          long long t1, t2;
          pthread_t threads[i];
          
          t1 = dfb_get_millis();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, mutex_lock_unlock_loop, (void*)&lock );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          t2 = dfb_get_millis();

          printf( "mutex lock/unlock (%d threads)         -> %8.2f k/sec\n",
                  i, (double)i * (double)2000000 / (double)(t2 - t1) );
     }

     
     pthread_mutex_destroy( &lock );

     printf( "\n" );
}

#if 0
static void
bench_pthread_mutex()
{
     int             i;
     long long       t1, t2;
     pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

     
     /* pthread_mutex lock/unlock */
     t1 = dfb_get_millis();
     
     for (i=0; i<40000000; i++) {
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
     }
     
     t2 = dfb_get_millis();
     
     printf( "pthread_mutex lock/unlock             -> %8.2f k/sec\n", (double)40000000 / (double)(t2 - t1) );

     
     pthread_mutex_destroy( &mutex );

     printf( "\n" );
}
#endif

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     
     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret)
          return DirectFBError( "DirectFBInit", ret );

     if (fusion_init() < 0)
          return -1;

     printf( "\n" );

#ifdef FUSION_FAKE
     printf( "Fusion Benchmark (Fusion fake mode!)\n" );
#else
     printf( "Fusion Benchmark\n" );
#endif

     printf( "\n" );
     
#if 0
     bench_pthread_mutex();

     bench_skirmish();
#endif
     
     bench_mutex_threaded();
     bench_spinlock_threaded();
     bench_skirmish_threaded();

     bench_property();
     
     bench_ref();
     
     bench_reactor();

     fusion_exit();

     return ret;
}

