/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@fusionsound.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@fusionsound.org> and
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fusionsound.h>
#include <ifusionsound.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/object.h>
#include <fusion/ref.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/shm_internal.h>

#include <core/types_sound.h>
#include <core/sound_buffer.h>
#include <core/playback.h>
#include <core/playback_internal.h>


static IFusionSound *fsound = NULL;


static DirectResult
init_fusionsound( int *argc, char **argv[] )
{
     DirectResult ret;

     /* Initialize FusionSound. */
     ret = FusionSoundInit( argc, argv );
     if (ret)
          return FusionSoundError( "FusionSoundInit", ret );

     /* Create the super interface. */
     ret = FusionSoundCreate( &fsound );
     if (ret)
          return FusionSoundError( "FusionSoundCreate", ret );

     return DR_OK;
}

static void
deinit_fusionsound( void )
{
     if (fsound)
          fsound->Release( fsound );
}

static bool
buffer_callback( FusionObjectPool *pool,
                 FusionObject     *object,
                 void             *ctx )
{
     DirectResult     ret;
     int              refs   = -1;
     int              ref_id = object->ref.multi.id;
     CoreSoundBuffer *buffer = (CoreSoundBuffer*) object;

     if (object->state != FOS_ACTIVE)
          return true;

     ret = fusion_ref_stat( &object->ref, &refs );
     if (ret)
          D_DERROR( ret, "FusionSound/Dump: fusion_ref_stat() on 0x%08x (buffer's reference id) failed!\n", ref_id );

#if FUSION_BUILD_MULTI
     printf( "0x%08x : ", ref_id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d   ", refs );

     printf( "%6d  ", buffer->length );
     printf( "%6d  ", buffer->break_pos );
     printf( "%3s   ", buffer->notify ? "YES" : " no" );

     printf( "%2d  ", FS_CHANNELS_FOR_MODE(buffer->mode) );

     switch (buffer->format) {
          case FSSF_U8:
               printf( "   U8   " );
               break;
          case FSSF_S16:
               printf( "  S16   " );
               break;
          case FSSF_S24:
               printf( "  S24   " );
               break;
          case FSSF_S32:
               printf( "  S32   " );
               break;
          case FSSF_FLOAT:
               printf( " FLOAT  " );
               break;
          default:
               printf( "        " );
               break;
     }

     printf( "%8d    ", buffer->rate );

     printf( "%4dk ", buffer->length*buffer->bytes/1024 );

     printf( "\n" );

     return true;
}

static void
dump_buffers( CoreSound *core )
{
     printf( "\n"
             "------------------------------[ Sound Buffers ]-------------------------------\n" );
     printf( "Reference  . Refs  Length   Break  Ntf   Ch  Format  Samplerate   Size\n" );
     printf( "------------------------------------------------------------------------------\n" );

     fs_core_enum_buffers( core, buffer_callback, NULL );
}

static bool
playback_callback( FusionObjectPool *pool,
                   FusionObject     *object,
                   void             *ctx )
{
     DirectResult     ret;
     int              refs     = -1;
     int              ref_id   = object->ref.multi.id;
     CorePlayback    *playback = (CorePlayback*) object;
     CoreSoundBuffer *buffer;

     if (object->state != FOS_ACTIVE)
          return true;

     buffer = playback->buffer;
     if (!buffer)
          D_ERROR( "FusionSound/Dump: Playback with reference id 0x%08x has no buffer!\n", ref_id );

     ret = fusion_ref_stat( &object->ref, &refs );
     if (ret)
          D_DERROR( ret, "FusionSound/Dump: fusion_ref_stat() on 0x%08x (playback's reference id) failed!\n", ref_id );

#if FUSION_BUILD_MULTI
     ref_id = 
     printf( "0x%08x : ", ref_id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d   ", refs );

     printf( "%6d  ", playback->position );
     printf( "%6d  ", playback->stop );

     printf( "%3s ", playback->notify ? "YES" : " no" );
     printf( "%c ", playback->running ? '*' : ' ' );

     printf( "%2d  ", buffer ? FS_CHANNELS_FOR_MODE(buffer->mode) : -1 );

     switch (buffer ? buffer->format : -1) {
          case FSSF_U8:
               printf( "   U8   " );
               break;
          case FSSF_S16:
               printf( "  S16   " );
               break;
          case FSSF_S24:
               printf( "  S24   " );
               break;
          case FSSF_S32:
               printf( "  S32   " );
               break;
          case FSSF_FLOAT:
               printf( " FLOAT  " );
               break;
          case -1:
               printf( "  N/A   " );
               break;
          default:
               printf( "unknown!" );
               break;
     }

     printf( "%8d  ", playback->pitch );

#if FUSION_BUILD_MULTI
     ref_id = buffer->object.ref.multi.id;
     printf( "0x%08x", ref_id );
#else
     printf( "N/A" );
#endif

     printf( "\n" );

     return true;
}

static void
dump_playbacks( CoreSound *core )
{
     printf( "\n"
             "------------------------------[ Sound Playbacks ]----------------------------\n" );
     printf( "Reference  . Refs  Position  Stop  Ntf R Ch  Format  Playrate  Buffer\n" );
     printf( "-----------------------------------------------------------------------------\n" );

     fs_core_enum_playbacks( core, playback_callback, NULL );
}

int
main( int argc, char *argv[] )
{
     DirectResult ret;
     long long millis;
     long int  seconds, minutes, hours, days;
     IFusionSound_data *data;

     char *buffer = malloc( 0x10000 );

     setvbuf( stdout, buffer, _IOFBF, 0x10000 );

     /* FusionSound initialization. */
     ret = init_fusionsound( &argc, &argv );
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
               printf( "\nFusionSound uptime: %02ld:%02ld:%02ld\n",
                       hours, minutes, seconds );
               break;

          case 1:
               printf( "\nFusionSound uptime: %ld day, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;

          default:
               printf( "\nFusionSound uptime: %ld days, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;
     }

     data = ifusionsound_singleton->priv;

     dump_buffers( data->core );
     dump_playbacks( data->core );

#if FUSION_BUILD_MULTI
     if (argc > 1 && !strcmp( argv[1], "-s" )) {
          SHMemDesc           *desc;
          unsigned int         total = 0;
          FusionSHMPoolShared *pool  = fs_core_shmpool( data->core );

          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret) {
               D_DERROR( ret, "Could not lock shared memory pool!\n" );
               goto out;
          }
     
          if (pool->allocs) {
               printf( "\nShared memory allocations (%d): \n",
                       direct_list_count_elements_EXPENSIVE( pool->allocs ) );
     
               direct_list_foreach (desc, pool->allocs) {
                    printf( " %9zu bytes at %p allocated in %-30s (%s: %u)\n",
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
     /* FusionSound deinitialization. */
     deinit_fusionsound();

     return ret;
}

