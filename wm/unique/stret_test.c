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

#include <config.h>

#include <fusion/fusion.h>
#include <fusion/shm/pool.h>

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
     DirectResult         ret;
     FusionWorld         *world;
     StretRegion         *root;
     StretRegion         *child[16];
     int                  child_num = 0;
     StretIteration       iteration;
     DFBRegion            clip;
     FusionSHMPoolShared *pool;

     ret = fusion_enter( -1, 0, FER_ANY, &world );
     if (ret)
          return -1;

     ret = fusion_shm_pool_create( world, "StReT Test Pool", 0x10000, direct_config->debug, &pool );
     if (ret) {
          fusion_exit( world, false );
          return -1;
     }

     D_INFO( "StReT/Test: Starting...\n" );


     ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 2, 0, 0, 1000, 1000, NULL, 0, pool, &root );
     if (ret) {
          D_DERROR( ret, "StReT/Test: Could not create root region!\n" );
          goto error_root;
     }
     else {
          ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 10, 10, 100, 100, root, 1, pool, &child[child_num++] );
          if (ret) {
               D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
               goto error_child;
          }
          else {
               ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 50, 50, 30, 30, child[0], 0, pool, &child[child_num++] );
               if (ret) {
                    D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                    goto error_child;
               }

               ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 20, 20, 30, 30, child[0], 0, pool, &child[child_num++] );
               if (ret) {
                    D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                    goto error_child;
               }
               else {
                    ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 10, 10, 10, 10, child[2], 0, pool, &child[child_num++] );
                    if (ret) {
                         D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                         goto error_child;
                    }

                    ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 20, 20, 10, 10, child[2], 0, pool, &child[child_num++] );
                    if (ret) {
                         D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
                         goto error_child;
                    }
               }
          }

          ret = stret_region_create( 0, NULL, 0, SRF_ACTIVE, 1, 200, 10, 200, 200, root, 0, pool, &child[child_num++] );
          if (ret) {
               D_DERROR( ret, "StReT/Test: Could not create child region!\n" );
               goto error_child;
          }
     }




     stret_iteration_init( &iteration, root, NULL );

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
     fusion_shm_pool_destroy( world, pool );

     fusion_exit( world, false );

     return 0;
}

