#include <config.h>

#include <fusion/fusion.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <misc/util.h>

#include <unique/stret.h>
#include <unique/stret_iteration.h>
#include <unique/internal.h>


int
main( int argc, char *argv[] )
{
     DirectResult    ret;
     int             world;
     StretRegion    *root;
     StretRegion    *child[16];
     int             child_num = 0;
     StretIteration  iteration;
     DFBRegion       clip;

     if (fusion_init( -1, 0, &world ) < 0)
          return -1;


     D_INFO( "StReT/Test: Starting...\n" );


     ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 2, 0, 0, 1000, 1000, NULL, 0, &root );
     if (ret) {
          D_DERROR( ret, "StReT/Test: Could not create root region!\n" );
          goto error_root;
     }
     else {
          ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 10, 10, 100, 100, root, 1, &child[child_num++] );
          if (ret) {
               D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
               goto error_child;
          }
          else {
               ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 50, 50, 30, 30, child[0], 0, &child[child_num++] );
               if (ret) {
                    D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                    goto error_child;
               }

               ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 20, 20, 30, 30, child[0], 0, &child[child_num++] );
               if (ret) {
                    D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                    goto error_child;
               }
               else {
                    ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 10, 10, 10, 10, child[2], 0, &child[child_num++] );
                    if (ret) {
                         D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                         goto error_child;
                    }

                    ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 20, 20, 10, 10, child[2], 0, &child[child_num++] );
                    if (ret) {
                         D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                         goto error_child;
                    }
               }
          }

          ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 200, 10, 200, 200, root, 0, &child[child_num++] );
          if (ret) {
               D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
               goto error_child;
          }
     }




     stret_iteration_init( &iteration, root );

     clip = (DFBRegion) { 50, 50, 200, 59 };

     D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[4] );
     //D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[3] );
     D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[2] );
     //D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[1] );
     D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[0] );
     D_ASSERT( stret_iteration_next( &iteration, &clip ) == child[5] );
     D_ASSERT( stret_iteration_next( &iteration, &clip ) == root );
     D_ASSERT( stret_iteration_next( &iteration, &clip ) == NULL );


     stret_region_destroy( child[child_num-1] );

error_child:
     while (--child_num)
          stret_region_destroy( child[child_num-1] );

     stret_region_destroy( root );

error_root:
     fusion_exit( false );

     return 0;
}

