/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <direct/messages.h>

#include <fusion/call.h>
#include <fusion/fusion.h>
#include <fusion/shm/pool.h>

#define MAX_NUM_BLOCKS 10000

#define SIZE_ALIGNMASK   0x3
#define ALIGN_SIZE(s)    (((s) + SIZE_ALIGNMASK) & ~SIZE_ALIGNMASK)

/**********************************************************************************************************************/

static int parse_cmdline  ( int    argc,
                            char  *argv[] );
static int show_usage     ();

/**********************************************************************************************************************/

static inline unsigned long
get_millis()
{
     struct timeval tv;

     gettimeofday( &tv, NULL );

     return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**********************************************************************************************************************/

static unsigned int   bit_rate;
static bool           run_busy;
static bool           do_fork;
static bool           do_thread;

/**********************************************************************************************************************/

static long           block_size = 184;
static long           num_blocks = 16;

/**********************************************************************************************************************/

static int fuser, fnice, fsystem, fidle, ftotal;
static int cuser, cnice, csystem, cidle, ctotal;
static int puser, pnice, psystem, pidle, ptotal;
static int duser, dnice, dsystem, didle, dtotal;

static int
read_stat()
{
     char  dummy[4];
     int   wa = 0, hi = 0, si = 0;
     FILE *file;

     puser   = cuser;
     pnice   = cnice;
     psystem = csystem;
     pidle   = cidle;
     ptotal  = ctotal;

     file = fopen( "/proc/stat", "r" );
     if (!file) {
          perror( "Could not open '/proc/stat'" );
          return 0;
     }

     if (fscanf( file, "%3s %d %d %d %d %d %d %d", dummy, &cuser, &cnice, &csystem, &cidle, &wa, &hi, &si ) < 4) {
          fprintf( stderr, "Parsing '/proc/stat' failed!\n" );
          return 0;
     }

     fclose( file );

     /* Compatibility with 2.6 split up idle times. */
     cidle += wa + hi + si;

     /* Count nice as idle. */
     cidle += cnice;
     cnice  = 0;

     ctotal  = cuser + cnice + csystem + cidle;

     duser   = cuser - puser;
     dnice   = cnice - pnice;
     dsystem = csystem - psystem;
     didle   = cidle - pidle;
     dtotal  = ctotal - ptotal;

     if (!ftotal) {
          fuser   = cuser;
          fnice   = cnice;
          fsystem = csystem;
          fidle   = cidle;
          ftotal  = ctotal;
     }

     return 1;
}

/**********************************************************************************************************************/

static pthread_t       busy_thread;
static pthread_mutex_t busy_lock  = PTHREAD_MUTEX_INITIALIZER;
static unsigned int    busy_alive = 1;
static unsigned int    busy_count;

static void *
busy_loop( void *arg )
{
     setpriority( PRIO_PROCESS, 0, 19 );

     while (busy_alive) {
          int i;

          for (i=0; i<100000; i++);

          pthread_mutex_lock( &busy_lock );

          busy_count++;

          pthread_mutex_unlock( &busy_lock );
     }

     return NULL;
}

/**********************************************************************************************************************/

static FusionCallHandlerResult
call_handler( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     static u32 checksum = 0;

     int        i;
     const u32 *values = call_ptr;


     for (i=0; i<block_size/4; i++)
          checksum += values[i];

     *ret_val = checksum;

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult         ret;
     FusionWorld         *world;
     FusionCall           call = {0};
     FusionSHMPoolShared *pool;
     void                *buffer;
     int           i, max_busy = 0, active = 1;
     long long     t1 = 0, t2;
     long long     start       = 0;
     long long     bytes       = 0;
     long long     last_bytes  = 0;
     unsigned long blocks      = 0;
     unsigned long last_blocks = 0;
     unsigned long last_busy   = 0;
     u32           checksum    = 0;
     bool          produce     = true;
     int           delay       = 66000;

     if (parse_cmdline( argc, argv ))
          return -1;

     if (bit_rate) {
          int blocks_per_sec = (bit_rate * 1024 / 8) / block_size;

          if (blocks_per_sec < 100)
               delay = 900000 / blocks_per_sec - 2000;
          else
               delay = 300000 * 100 / blocks_per_sec;

          num_blocks = bit_rate * 1024 / 8 / block_size * delay / 900000;

          if (num_blocks > MAX_NUM_BLOCKS)
               num_blocks = MAX_NUM_BLOCKS;

          if (!num_blocks) {
               num_blocks = 1;
               delay = 970 * block_size / (bit_rate * 1024 / 8) * 1000 - 2000;
          }
     }

     sync();

     if (run_busy) {
          pthread_create( &busy_thread, NULL, busy_loop, NULL );

          printf( "Calibrating...\n" );
     
          pthread_mutex_lock( &busy_lock );
     
          for (i=0; i<7; i++) {
               int busy_rate;
     
               busy_count = 0;
     
               t1 = get_millis();
               pthread_mutex_unlock( &busy_lock );
     
               usleep( 300000 );
     
               pthread_mutex_lock( &busy_lock );
               t2 = get_millis();
     
               busy_rate = busy_count * 1000 / (t2 - t1);
     
               if (busy_rate > max_busy)
                    max_busy = busy_rate;
          }
     
          printf( "Calibrating done. (%d busy counts/sec)\n", max_busy );
     }

     ret = fusion_enter( -1, 23, FER_MASTER, &world );
     if (ret)
          return ret;

     ret = fusion_call_init( &call, call_handler, NULL, world );
     if (ret)
          return ret;

     ret = fusion_shm_pool_create( world, "Stream Buffer", block_size + 8192, false, &pool );
     if (ret)
          return ret;

     ret = fusion_shm_pool_allocate( pool, block_size, false, true, &buffer );
     if (ret)
          return ret;



     /*
      * Do the fork() magic!
      */
     if (do_fork) {
          fusion_world_set_fork_action( world, FFA_FORK );

          switch (fork()) {
               case -1:
                    D_PERROR( "fork() failed!\n" );
                    return -1;

               case 0:
                    /* child continues as the producer */
                    run_busy = false;
                    break;

               default:
                    /* parent is the consumer (callback in Fusion Dispatch thread) */
                    produce = false;

                    usleep( 50000 );
          }
     }


     start = t1 = get_millis();

     if (run_busy) {
          busy_count = 0;
          pthread_mutex_unlock( &busy_lock );
     }

#ifdef LINUX_2_4
     delay -= 10000;
#endif

     do {
          if (bit_rate || !produce) {
               if (delay > 10)
                    usleep( delay );
          }

          if (produce) {
               for (i=0; i<num_blocks; i++) {
                    int  n;
                    u32  retsum;
                    u32 *values = buffer;

                    for (n=0; n<block_size/4; n++) {
                         values[n] = n;
                         checksum += n;
                    }

                    bytes += block_size;

                    fusion_call_execute( &call, do_thread ? FCEF_NODIRECT : FCEF_NONE,
                                         0, buffer, (int*) &retsum );

                    if (retsum != checksum)
                         D_ERROR( "Checksum returned by consumer (0x%08x) does not match 0x%08x!\n", retsum, checksum );
               }

               blocks += num_blocks;
          }


          t2 = get_millis();
          if (t2 - t1 > 2000) {
               if (produce) {
                    long long kbits = 0, avgkbits, total_time, diff_time, diff_bytes;

                    printf( "\n\n\n" );

                    total_time = t2 - start;
                    diff_time  = t2 - t1;
                    diff_bytes = bytes - last_bytes;

                    avgkbits   = (long long)bytes * 8LL * 1000LL / (long long)total_time / 1024LL;

                    if (diff_time)
                         kbits = (long long)diff_bytes * 8LL * 1000LL / (long long)diff_time / 1024LL;

                    printf( "Total Time:       %7lld ms\n", total_time );
                    printf( "Stream Size:      %7lld kb\n", bytes / 1024 );
                    printf( "Stream Rate:      %7lld kb/sec -> %lld.%03lld MBit (avg. %lld.%03lld MBit)\n",
                            kbits / 8,
                            (kbits * 1000 / 1024) / 1000, (kbits * 1000 / 1024) % 1000,
                            (avgkbits * 1000 / 1024) / 1000, (avgkbits * 1000 / 1024) % 1000 );
                    printf( "\n" );


                    if (last_bytes && bit_rate) {
                         long long diff_bytes = (bytes - last_bytes) * 1000 / (t2 - t1);
                         long long need_bytes = bit_rate * 1024 / 8;

                         if (diff_bytes) {
                              int new_blocks = (num_blocks * need_bytes + diff_bytes/2) / diff_bytes;

                              num_blocks = (new_blocks + num_blocks + 1) / 2;

                              if (num_blocks > MAX_NUM_BLOCKS)
                                   num_blocks = MAX_NUM_BLOCKS;
                         }
                    }


                    read_stat();

                    if (ftotal != ctotal && dtotal) {
                         int load, aload;

                         load  = 1000 - didle * 1000 / dtotal;
                         aload = 1000 - (cidle - fidle) * 1000 / (ctotal - ftotal);

                         printf( "Overall Stats\n" );
                         printf( "  Total Time:      %7lld ms\n", t2 - start );
                         printf( "  Block Size:      %7ld\n", block_size );
                         printf( "  Blocks/cycle:    %7ld\n", num_blocks );
                         printf( "  Blocks/second:   %7lld\n", (blocks - last_blocks) * 1000 / diff_time );
                         printf( "  Delay:           %7d\n", delay );
                         printf( "  CPU Load:        %5d.%d %% (avg. %d.%d %%)\n",
                                 load / 10, load % 10, aload / 10, aload % 10 );
                    }


                    last_bytes  = bytes;
                    last_blocks = blocks;
               }

               if (run_busy) {
                    pthread_mutex_lock( &busy_lock );

                    if (last_busy) {
                         int busy_diff;
                         int busy_rate, busy_load;
                         int abusy_rate, abusy_load;

                         busy_diff = busy_count - last_busy;
                         busy_rate = max_busy - (busy_diff * 1000 / (t2 - t1));
                         busy_load = busy_rate * 1000 / max_busy;
                         abusy_rate = max_busy - (busy_count * 1000 / (t2 - start));
                         abusy_load = abusy_rate * 1000 / max_busy;

                         printf( "  Real CPU Load:   %5d.%d %% (avg. %d.%d %%)\n",
                                 busy_load / 10, busy_load % 10,
                                 abusy_load / 10, abusy_load % 10 );
                    }

                    last_busy = busy_count;

                    pthread_mutex_unlock( &busy_lock );
               }

               t1 = t2;
          }
     } while (active > 0);


     if (run_busy) {
          busy_alive = 0;

          pthread_join( busy_thread, NULL );
     }

     return -1;
}

/**********************************************************************************************************************/

static int
parse_cmdline( int argc, char *argv[] )
{
     int   i;
     char *end;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-s" )) {
               if (++i < argc) {
                    block_size = strtoul( argv[i], &end, 10 );

                    if (end && *end) {
                         D_ERROR( "Parse error in number '%s'!\n", argv[i] );
                         return -1;
                    }

                    if (block_size < 1)
                         return show_usage();
               }
               else
                    return show_usage();
          }
          else if (!strcmp( argv[i], "-b" )) {
               if (++i < argc) {
                    bit_rate = strtoul( argv[i], &end, 10 );

                    if (end && *end) {
                         D_ERROR( "Parse error in number '%s'!\n", argv[i] );
                         return -1;
                    }
               }
               else
                    return show_usage();
          }
          else if (!strcmp( argv[i], "-B" )) {
               if (++i < argc) {
                    bit_rate = strtoul( argv[i], &end, 10 ) * 1024;

                    if (end && *end) {
                         D_ERROR( "Parse error in number '%s'!\n", argv[i] );
                         return -1;
                    }
               }
               else
                    return show_usage();
          }
          else if (!strcmp( argv[i], "-c" )) {
               run_busy = 1;
          }
          else if (!strcmp( argv[i], "-f" )) {
               do_fork = 1;
          }
          else if (!strcmp( argv[i], "-t" )) {
               do_thread = 1;
          }
          else
               return show_usage();
     }

     return 0;
}

static int
show_usage()
{
     fprintf( stderr, "\n"
                      "Usage:\n"
                      "   fusion_stream [options]\n"
                      "\n"
                      "Options:\n"
                      "   -s <1-n>  Size of each block of data\n"
                      "   -b <0-n>  Designated bit rate in kbit (0 = unlimited)\n"
                      "   -B <0-n>  Designated bit rate in Mbit (0 = unlimited)\n"
                      "   -c        Run busy loop counting spare CPU cycles to get real CPU load\n"
                      "   -f        Fork to have the producer in a separate process\n"
                      "   -t        Force calls to be handled in a separate thread\n"
                      "\n"
              );

     return -1;
}

