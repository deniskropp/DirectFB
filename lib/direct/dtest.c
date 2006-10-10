#include <config.h>

#include <direct/cpu_accel.h>
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>

static int
show_usage()
{
     fprintf( stderr, "Usage: dtest [-f <file>] [-u <host>:<port>]\n" );

     return -1;
}

int
main( int argc, char *argv[] )
{
     int            i;
     DirectResult   ret;
     DirectLogType  log_type  = DLT_STDERR;
     const char    *log_param = NULL;
     DirectLog     *log;


     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-f" )) {
               if (++i < argc) {
                    log_type  = DLT_FILE;
                    log_param = argv[i];
               }
               else
                    return show_usage();
          }
          else if (!strcmp( argv[i], "-u" )) {
               if (++i < argc) {
                    log_type  = DLT_UDP;
                    log_param = argv[i];
               }
               else
                    return show_usage();
          }
          else
               return show_usage();
     }

     /* Initialize logging. */
     ret = direct_log_create( log_type, log_param, &log );
     if (ret)
          return -1;

     /* Set default log to use. */
     direct_log_set_default( log );


     /* Test memory leak detector by not freeing this one. */
     D_MALLOC( 1351 );

     D_INFO( "Direct/Test: Application starting...\n" );


     /* Initialize libdirect. */
     direct_initialize();

     direct_find_best_memcpy();

     D_DEBUG( "Direct/Test: direct_mm_accel() returns 0x%08x\n", direct_mm_accel() );

     /* Shutdown libdirect. */
     direct_shutdown();


     /* Shutdown logging. */
     direct_log_destroy( log );


     direct_print_memleaks();

     return 0;
}

