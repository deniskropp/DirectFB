/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <stdio.h>
#include <unistd.h>

#include <sys/file.h>

#include <pthread.h>

#include <directfb.h>

#include <core/system.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/lock.h>
#include <fusion/property.h>
#include <fusion/reactor.h>
#include <fusion/ref.h>

#include <misc/util.h>


static long long    t1, t2;
static unsigned int loops;

#define BENCH_START()    do { sync(); usleep(100000); sync(); t1 = dfb_get_millis(); loops = 0; } while (0)
#define BENCH_STOP()     do { t2 = dfb_get_millis(); } while (0)

#define BENCH_LOOP()     while ((loops++ & 0x3fff) || (dfb_get_millis() - t1 < 1000))

#define BENCH_RESULT()   (loops / (float)(t2 - t1))


static ReactionResult
react (const void    *msg_data,
       void          *ctx)
{
     return RS_OK;
}

static void
bench_reactor()
{
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
     BENCH_START();

     BENCH_LOOP() {
          fusion_reactor_attach( reactor, react, NULL, &reaction );
          fusion_reactor_detach( reactor, &reaction );
     }

     BENCH_STOP();

     printf( "reactor attach/detach                 -> %8.2f k/sec\n", BENCH_RESULT() );


     /* reactor attach/detach (2nd) */
     fusion_reactor_attach( reactor, react, NULL, &reaction );

     BENCH_START();

     BENCH_LOOP() {
          fusion_reactor_attach( reactor, react, NULL, &reaction2 );
          fusion_reactor_detach( reactor, &reaction2 );
     }

     BENCH_STOP();

     fusion_reactor_detach( reactor, &reaction );

     printf( "reactor attach/detach (2nd)           -> %8.2f k/sec\n", BENCH_RESULT() );


     /* reactor attach/detach (global) */
     fusion_reactor_attach( reactor, react, NULL, &reaction );

     BENCH_START();

     BENCH_LOOP() {
          fusion_reactor_attach_global( reactor, 0, NULL, &global_reaction );
          fusion_reactor_detach_global( reactor, &global_reaction );
     }

     BENCH_STOP();

     fusion_reactor_detach( reactor, &reaction );

     printf( "reactor attach/detach (global)        -> %8.2f k/sec\n", BENCH_RESULT() );


     /* reactor dispatch */
     fusion_reactor_attach( reactor, react, NULL, &reaction );

     BENCH_START();

     BENCH_LOOP() {
          char msg[16];

          fusion_reactor_dispatch( reactor, msg, true, NULL );
     }

     BENCH_STOP();

     printf( "reactor dispatch                      -> %8.2f k/sec\n", BENCH_RESULT() );


     fusion_reactor_detach( reactor, &reaction );


     fusion_reactor_free( reactor );

     printf( "\n" );
}

static void
bench_ref()
{
     FusionResult ret;
     FusionRef    ref;

     ret = fusion_ref_init( &ref );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }


     /* ref up/down (local) */
     BENCH_START();

     BENCH_LOOP() {
          fusion_ref_up( &ref, false );
          fusion_ref_down( &ref, false );
     }

     BENCH_STOP();

     printf( "ref up/down (local)                   -> %8.2f k/sec\n", BENCH_RESULT() );


     /* ref up/down (global) */
     BENCH_START();

     BENCH_LOOP() {
          fusion_ref_up( &ref, true );
          fusion_ref_down( &ref, true );
     }

     BENCH_STOP();

     printf( "ref up/down (global)                  -> %8.2f k/sec\n", BENCH_RESULT() );


     fusion_ref_destroy( &ref );

     printf( "\n" );
}

static void
bench_property()
{
     FusionResult   ret;
     FusionProperty property;

     ret = fusion_property_init( &property );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }


     /* property lease/cede */
     BENCH_START();

     BENCH_LOOP() {
          fusion_property_lease( &property );
          fusion_property_cede( &property );
     }

     BENCH_STOP();

     printf( "property lease/cede                   -> %8.2f k/sec\n", BENCH_RESULT() );


     fusion_property_destroy( &property );

     printf( "\n" );
}

static void
bench_skirmish()
{
     FusionResult   ret;
     FusionSkirmish skirmish;

     ret = fusion_skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }


     /* skirmish prevail/dismiss */
     BENCH_START();

     BENCH_LOOP() {
          fusion_skirmish_prevail( &skirmish );
          fusion_skirmish_dismiss( &skirmish );
     }

     BENCH_STOP();

     printf( "skirmish prevail/dismiss              -> %8.2f k/sec\n", BENCH_RESULT() );


     fusion_skirmish_destroy( &skirmish );

     printf( "\n" );
}

static void *
prevail_dismiss_loop( void *arg )
{
     FusionSkirmish *skirmish = (FusionSkirmish *) arg;

     BENCH_LOOP() {
          fusion_skirmish_prevail( skirmish );
          fusion_skirmish_dismiss( skirmish );
     }

     return NULL;
}

static void
bench_skirmish_threaded()
{
     int            i;
     FusionResult   ret;
     FusionSkirmish skirmish;

     ret = fusion_skirmish_init( &skirmish );
     if (ret) {
          fprintf( stderr, "Fusion Error %d\n", ret );
          return;
     }


     /* skirmish prevail/dismiss (2-5 threads) */
     for (i=2; i<=5; i++) {
          int       t;
          pthread_t threads[i];

          BENCH_START();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, prevail_dismiss_loop, &skirmish );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          BENCH_STOP();

          printf( "skirmish prevail/dismiss (%d threads)  -> %8.2f k/sec\n", i, BENCH_RESULT() );
     }


     fusion_skirmish_destroy( &skirmish );

     printf( "\n" );
}

static void *
mutex_lock_unlock_loop( void *arg )
{
     pthread_mutex_t *lock = (pthread_mutex_t *) arg;

     BENCH_LOOP() {
          pthread_mutex_lock( lock );
          pthread_mutex_unlock( lock );
     }

     return NULL;
}

static void
bench_mutex_threaded()
{
     int             i;
     pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


     /* mutex lock/unlock (2-5 threads) */
     for (i=2; i<=5; i++) {
          int       t;
          pthread_t threads[i];

          BENCH_START();

          for (t=0; t<i; t++)
               pthread_create( &threads[t], NULL, mutex_lock_unlock_loop, &lock );

          for (t=0; t<i; t++)
               pthread_join( threads[t], NULL );

          BENCH_STOP();

          printf( "mutex lock/unlock (rec., %d threads)   -> %8.2f k/sec\n", i, BENCH_RESULT() );
     }


     pthread_mutex_destroy( &lock );

     printf( "\n" );
}

static void
bench_mutex()
{
     pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
     pthread_mutex_t rmutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


     /* pthread_mutex lock/unlock */
     BENCH_START();

     BENCH_LOOP() {
          pthread_mutex_lock( &mutex );
          pthread_mutex_unlock( &mutex );
     }

     BENCH_STOP();

     printf( "mutex lock/unlock                     -> %8.2f k/sec\n", BENCH_RESULT() );


     /* pthread_mutex lock/unlock */
     BENCH_START();

     BENCH_LOOP() {
          pthread_mutex_lock( &rmutex );
          pthread_mutex_unlock( &rmutex );
     }

     BENCH_STOP();

     printf( "mutex lock/unlock (recursive)         -> %8.2f k/sec\n", BENCH_RESULT() );


     pthread_mutex_destroy( &mutex );
     pthread_mutex_destroy( &rmutex );

     printf( "\n" );
}

static void
bench_flock()
{
     int   fd;
     FILE *tmp;

     tmp = tmpfile();
     if (!tmp) {
          perror( "tmpfile()" );
          return;
     }

     fd = fileno( tmp );
     if (fd < 0) {
          perror( "fileno()" );
          fclose( tmp );
          return;
     }

     BENCH_START();

     BENCH_LOOP() {
          flock( fd, LOCK_EX );
          flock( fd, LOCK_UN );
     }

     BENCH_STOP();

     printf( "flock lock/unlock                     -> %8.2f k/sec\n", BENCH_RESULT() );
     printf( "\n" );

     fclose( tmp );
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret)
          return DirectFBError( "DirectFBInit", ret );

     dfb_system_lookup();

     if (fusion_init( -1, 0, NULL ) < 0)
          return -1;

     printf( "\n" );

#if FUSION_BUILD_MULTI
     printf( "Fusion Benchmark (Multi Application Core)\n" );
#else
     printf( "Fusion Benchmark (Single Application Core)\n" );
#endif

     printf( "\n" );


     bench_flock();

     bench_mutex();
     bench_mutex_threaded();

     bench_skirmish();
     bench_skirmish_threaded();

     //bench_spinlock_threaded();

     bench_property();

     bench_ref();

     bench_reactor();

     fusion_exit();

     return ret;
}

