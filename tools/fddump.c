/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fusiondale.h>
#include <ifusiondale.h>

#include <direct/clock.h>
#include <direct/debug.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/hash.h>
#include <fusion/object.h>
#include <fusion/ref.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/shm_internal.h>

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>


static IFusionDale *dale = NULL;


static DFBResult
init_fusiondale( int *argc, char **argv[] )
{
     DFBResult ret;

     /* Initialize FusionDale. */
     ret = FusionDaleInit( argc, argv );
     if (ret)
          return FusionDaleError( "FusionDaleInit", ret );

     /* Create the super interface. */
     ret = FusionDaleCreate( &dale );
     if (ret)
          return FusionDaleError( "FusionDaleCreate", ret );

     return DFB_OK;
}

static void
deinit_fusiondale()
{
     if (dale)
          dale->Release( dale );
}

static bool
messenger_callback( FusionObjectPool *pool,
                    FusionObject     *object,
                    void             *ctx )
{
     DirectResult   ret;
     int            refs;
     CoreMessenger *messenger = (CoreMessenger*) object;

     if (object->state != FOS_ACTIVE)
          return true;

     ret = fusion_ref_stat( &object->ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return false;
     }

#if FUSION_BUILD_MULTI
     printf( "0x%08x : ", object->ref.multi.id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d ", refs );

     printf( "%3lu", object->id );

     printf( "  %4u ", fusion_hash_size( messenger->hash ) );




     printf( "\n" );

     return true;
}

static void
dump_messengers( CoreDale *core )
{
     printf( "\n"
             "--------------------------[ Messengers ]--------------------------\n" );
     printf( "Reference  . Refs ID Events\n" );
     printf( "------------------------------------------------------------------\n" );

     fd_core_enum_messengers( core, messenger_callback, NULL );
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     long long millis;
     long int  seconds, minutes, hours, days;
     IFusionDale_data *data;

     char *buffer = malloc( 0x10000 );

     setvbuf( stdout, buffer, _IOFBF, 0x10000 );

     /* FusionDale initialization. */
     ret = init_fusiondale( &argc, &argv );
     if (ret)
          goto out;

     millis = direct_clock_get_millis();

     seconds  = millis / 1000;
     millis  %= 1000;

     minutes  = seconds / 60;
     seconds %= 60;

     hours    = minutes / 60;
     minutes %= 60;

     days     = hours / 24;
     hours   %= 24;

     switch (days) {
          case 0:
               printf( "\nFusionDale uptime: %02ld:%02ld:%02ld\n",
                       hours, minutes, seconds );
               break;

          case 1:
               printf( "\nFusionDale uptime: %ld day, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;

          default:
               printf( "\nFusionDale uptime: %ld days, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;
     }

     data = ifusiondale_singleton->priv;

     dump_messengers( data->core );

#if FUSION_BUILD_MULTI
     if (argc > 1 && !strcmp( argv[1], "-s" )) {
          SHMemDesc           *desc;
          unsigned int         total = 0;
          FusionSHMPoolShared *pool  = fd_core_shmpool( data->core );

          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret) {
               D_DERROR( ret, "Could not lock shared memory pool!\n" );
               goto out;
          }
     
          if (pool->allocs) {
               printf( "\nShared memory allocations (%d): \n",
                       direct_list_count_elements_EXPENSIVE( pool->allocs ) );
     
               direct_list_foreach (desc, pool->allocs) {
                    printf( " %9d bytes at %p allocated in %-30s (%s: %u)\n",
                         desc->bytes, desc->mem, desc->func, desc->file, desc->line );

                    total += desc->bytes;
               }

               printf( "   -------\n  %7dk total\n", total >> 10 );
          }
     
          printf( "\nShared memory file size: %dk\n", pool->heap->size >> 10 );

          fusion_skirmish_dismiss( &pool->lock );
     }
#endif

     printf( "\n" );

out:
     /* FusionDale deinitialization. */
     deinit_fusiondale();

     return ret;
}

