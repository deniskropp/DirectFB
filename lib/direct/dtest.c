#include <direct/cpu_accel.h>
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>

int
main( int argc, char *argv[] )
{
     D_MALLOC( 1351 );

     D_INFO( "Direct/Test: Application starting...\n" );

     direct_initialize();

     direct_find_best_memcpy();

     D_DEBUG( "Direct/Test: direct_mm_accel() returns 0x%08x\n", direct_mm_accel() );

     direct_print_memleaks();

     direct_shutdown();

     return 0;
}

