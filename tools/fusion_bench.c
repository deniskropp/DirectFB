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
#define N_DISPATCH  700000
#endif

static void
bench_reactor()
{
     unsigned int    i, react_counter = 0;
     long long       t1, t2;
     FusionReactor  *reactor;
     Reaction        reaction;
     Reaction        reaction2;
     GlobalReaction  global_reaction;

     reactor = fusion_reactor_new( 16 );
     if (!reactor) {
          fprintf( stderr, "Fusion Error\n" );
          return;
     }

     
     /* reactor attach/detach */
     t1 = dfb_get_millis();
     
     for (i=0; i<700000; i++) {
          fusion_reactor_attach( reactor, react, NULL, &reaction );
          fusion_reactor_detach( reactor, &reaction );
     }
     
     t2 = dfb_get_millis();
     
     printf( "reactor attach/detach                 -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     /* reactor attach/detach (2nd) */
     fusion_reactor_attach( reactor, react, NULL, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<700000; i++) {
          fusion_reactor_attach( reactor, react, NULL, &reaction2 );
          fusion_reactor_detach( reactor, &reaction2 );
     }
     
     t2 = dfb_get_millis();
     
     fusion_reactor_detach( reactor, &reaction );
     
     printf( "reactor attach/detach (2nd)           -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     /* reactor attach/detach (global) */
     fusion_reactor_attach( reactor, react, NULL, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<700000; i++) {
          fusion_reactor_attach_global( reactor, 0, NULL, &global_reaction );
          fusion_reactor_detach_global( reactor, &global_reaction );
     }
     
     t2 = dfb_get_millis();
     
     fusion_reactor_detach( reactor, &reaction );
     
     printf( "reactor attach/detach (global)        -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     /* reactor dispatch */
     fusion_reactor_attach( reactor, react, &react_counter, &reaction );
     
     t1 = dfb_get_millis();
     
     for (i=0; i<N_DISPATCH; i++) {
          char msg[16];

          fusion_reactor_dispatch( reactor, msg, true, NULL );
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
     
     fusion_reactor_detach( reactor, &reaction );

     
     fusion_reactor_free( reactor );

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
     
     for (i=0; i<700000; i++) {
          fusion_ref_up( &ref, false );
          fusion_ref_down( &ref, false );
     }
     
     t2 = dfb_get_millis();
     
     printf( "ref up/down (local)                   -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     /* ref up/down (global) */
     t1 = dfb_get_millis();
     
     for (i=0; i<700000; i++) {
          fusion_ref_up( &ref, true );
          fusion_ref_down( &ref, true );
     }
     
     t2 = dfb_get_millis();

     printf( "ref up/down (global)                  -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
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
     
     for (i=0; i<700000; i++) {
          fusion_property_lease( &property );
          fusion_property_cede( &property );
     }
     
     t2 = dfb_get_millis();
     
     printf( "property lease/cede                   -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     fusion_property_destroy( &property );

     printf( "\n" );
}

static void
bench_skirmish()
{
     FusionResult   ret;
     unsigned int   i;
     long long      t1, t2;
     FusionSkirmish skirmish;

     ret = fusion_skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* skirmish prevail/dismiss */
     t1 = dfb_get_millis();
     
     for (i=0; i<700000; i++) {
          fusion_skirmish_prevail( &skirmish );
          fusion_skirmish_dismiss( &skirmish );
     }
     
     t2 = dfb_get_millis();
     
     printf( "skirmish prevail/dismiss              -> %8.2f k/sec\n", 700000 / (float)(t2 - t1) );

     
     fusion_skirmish_destroy( &skirmish );

     printf( "\n" );
}

static void *
prevail_dismiss_loop( void *arg )
{
     unsigned int    i;
     FusionSkirmish *skirmish = (FusionSkirmish *) arg;

     for (i=0; i<200000; i++) {
          fusion_skirmish_prevail( skirmish );
          fusion_skirmish_dismiss( skirmish );
     }

     return NULL;
}

static void
bench_skirmish_threaded()
{
     FusionResult   ret;
     int            i;
     FusionSkirmish skirmish;

     ret = fusion_skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }

     
     /* skirmish prevail/dismiss (2-5 threads) */
     for (i=2; i<=5; i++) {
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
                  i, (double)i * (double)200000 / (double)(t2 - t1) );
     }

     
     fusion_skirmish_destroy( &skirmish );

     printf( "\n" );
}


#if 0
static pthread_spinlock_t spin_lock;

static void *
spin_lock_unlock_loop( void *arg )
{
     int                 i;
     pthread_spinlock_t *lock = &spin_lock;

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

     ret = pthread_spin_init( &spin_lock, true );
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
               pthread_create( &threads[t], NULL, spin_lock_unlock_loop, NULL );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          t2 = dfb_get_millis();

          if (i > 1)
               printf( "spinlock lock/unlock (%d threads)      -> %8.2f k/sec\n",
                       i, (double)i * (double)2000000 / (double)(t2 - t1) );
          else
               printf( "spinlock lock/unlock                  -> %8.2f k/sec\n",
                       (double)i * (double)2000000 / (double)(t2 - t1) );
     }

     
     pthread_spin_destroy( &spin_lock );

     printf( "\n" );
}
#endif

static void *
mutex_lock_unlock_loop( void *arg )
{
     int              i;
     pthread_mutex_t *lock = (pthread_mutex_t *) arg;

     for (i=0; i<1000000; i++) {
          pthread_mutex_lock( lock );
          pthread_mutex_unlock( lock );
     }

     return NULL;
}

static void
bench_mutex_threaded()
{
     int             i;
     pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

     
     /* mutex lock/unlock (2-5 threads) */
     for (i=2; i<=5; i++) {
          int       t;
          long long t1, t2;
          pthread_t threads[i];
          
          t1 = dfb_get_millis();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, mutex_lock_unlock_loop, &lock );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          t2 = dfb_get_millis();

          printf( "mutex lock/unlock (%d threads)         -> %8.2f k/sec\n",
                  i, (double)i * (double)1000000 / (double)(t2 - t1) );
     }

     
     pthread_mutex_destroy( &lock );

     printf( "\n" );
}

static void
bench_mutex()
{
     int             i;
     long long       t1, t2;
     pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
     pthread_mutex_t rmutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

     
     /* pthread_mutex lock/unlock */
     t1 = dfb_get_millis();
     
     for (i=0; i<2000000; i++) {
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
     }
     
     t2 = dfb_get_millis();
     
     printf( "mutex lock/unlock                     -> %8.2f k/sec\n", (double)2000000 / (double)(t2 - t1) );

     
     /* pthread_mutex lock/unlock */
     t1 = dfb_get_millis();
     
     for (i=0; i<2000000; i++) {
          pthread_mutex_lock( &rmutex );
          pthread_mutex_unlock( &rmutex );
     }
     
     t2 = dfb_get_millis();
     
     printf( "mutex lock/unlock (recursive)         -> %8.2f k/sec\n", (double)2000000 / (double)(t2 - t1) );

     
     pthread_mutex_destroy( &mutex );
     pthread_mutex_destroy( &rmutex );

     printf( "\n" );
}

#define RUN(x...)   sync(); sleep(1); x

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     
     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret)
          return DirectFBError( "DirectFBInit", ret );

     if (fusion_init( dfb_config->session, NULL ) < 0)
          return -1;

     printf( "\n" );

#ifdef FUSION_FAKE
     printf( "Fusion Benchmark (Fusion fake mode!)\n" );
#else
     printf( "Fusion Benchmark\n" );
#endif

     printf( "\n" );
     
     
     RUN( bench_mutex() );
     RUN( bench_mutex_threaded() );
     
     RUN( bench_skirmish() );
     RUN( bench_skirmish_threaded() );
     
     //bench_spinlock_threaded();

     RUN( bench_property() );
     
     RUN( bench_ref() );
     
     RUN( bench_reactor() );

     fusion_exit();

     return ret;
}

