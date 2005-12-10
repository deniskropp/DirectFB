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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <directfb.h>
#include <directfb_strings.h>

#include <direct/clock.h>
#include <direct/debug.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/object.h>
#include <fusion/ref.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/shm_internal.h>

#include <core/core.h>
#include <core/layer_control.h>
#include <core/layer_context.h>
#include <core/layers.h>
#include <core/layers_internal.h>
#include <core/surfaces.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/windows_internal.h>
#include <core/wm.h>

static DirectFBPixelFormatNames( format_names );

typedef struct {
     int video;
     int system;
     int presys;
} MemoryUsage;

static IDirectFB *dfb = NULL;

static MemoryUsage mem = { 0, 0 };


static DFBResult
init_directfb( int *argc, char **argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB. */
     ret = DirectFBInit( argc, argv );
     if (ret)
          return DirectFBError( "DirectFBInit", ret );

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret)
          return DirectFBError( "DirectFBCreate", ret );

     return DFB_OK;
}

static void
deinit_directfb()
{
     if (dfb)
          dfb->Release( dfb );
}

static inline int
buffer_size( CoreSurface *surface, SurfaceBuffer *buffer, bool video )
{
     return video ?
          (buffer->video.health == CSH_INVALID ?
           0 : buffer->video.pitch * DFB_PLANE_MULTIPLY( surface->format,
                                                         surface->height )) :
          (buffer->system.health == CSH_INVALID ?
           0 : buffer->system.pitch * DFB_PLANE_MULTIPLY( surface->format,
                                                          surface->height ));
}

static int
buffer_sizes( CoreSurface *surface, bool video )
{
     int mem = buffer_size( surface, surface->front_buffer, video );

     if (surface->caps & DSCAPS_FLIPPING)
          mem += buffer_size( surface, surface->back_buffer, video );

     if (surface->caps & DSCAPS_TRIPLE)
          mem += buffer_size( surface, surface->idle_buffer, video );

     return mem;
}

static int
buffer_locks( CoreSurface *surface, bool video )
{
     SurfaceBuffer *front = surface->front_buffer;
     SurfaceBuffer *back  = surface->back_buffer;
     SurfaceBuffer *idle  = surface->idle_buffer;

     int locks = video ? front->video.locked : front->system.locked;

     if (surface->caps & DSCAPS_FLIPPING)
          locks += video ? back->video.locked : back->system.locked;

     if (surface->caps & DSCAPS_TRIPLE)
          locks += video ? idle->video.locked : idle->system.locked;

     return locks;
}

static bool
surface_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     DirectResult ret;
     int          i;
     int          refs;
     CoreSurface *surface = (CoreSurface*) object;
     MemoryUsage *mem     = ctx;
     int          vmem;
     int          smem;

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

     printf( "%3d   ", refs );

     printf( "%4d x %4d   ", surface->width, surface->height );

     for (i=0; format_names[i].format; i++) {
          if (surface->format == format_names[i].format)
               printf( "%8s ", format_names[i].name );
     }

     vmem = buffer_sizes( surface, true );
     smem = buffer_sizes( surface, false );

     mem->video += vmem;

     /* FIXME: assumes all buffers have this flag (or none) */
     if (surface->front_buffer->flags & SBF_FOREIGN_SYSTEM)
          mem->presys += smem;
     else
          mem->system += smem;

     if (vmem && vmem < 1024)
          vmem = 1024;

     if (smem && smem < 1024)
          smem = 1024;

     printf( "%5dk%c  ", vmem >> 10, buffer_locks( surface, true ) ? '°' : ' ' );
     printf( "%5dk%c  ", smem >> 10, buffer_locks( surface, false ) ? '°' : ' ' );

     if (surface->caps & DSCAPS_SYSTEMONLY)
          printf( "system only  " );

     if (surface->caps & DSCAPS_VIDEOONLY)
          printf( "video only   " );

     if (surface->caps & DSCAPS_DOUBLE)
          printf( "double       " );

     if (surface->caps & DSCAPS_TRIPLE)
          printf( "triple       " );

     if (surface->caps & DSCAPS_INTERLACED)
          printf( "interlaced   " );

     if (surface->caps & DSCAPS_PREMULTIPLIED)
          printf( "premultiplied" );

     printf( "\n" );

     return true;
}

static void
dump_surfaces()
{
     printf( "\n"
             "-----------------------------[ Surfaces ]-------------------------------\n" );
     printf( "Reference  . Refs  Width Height  Format     Video   System  Capabilities\n" );
     printf( "------------------------------------------------------------------------\n" );

     dfb_core_enum_surfaces( NULL, surface_callback, &mem );

     printf( "                                          ------   ------\n" );
     printf( "                                         %6dk  %6dk   -> %dk total\n",
             mem.video >> 10, (mem.system + mem.presys) >> 10,
             (mem.video + mem.system + mem.presys) >> 10);
}

static bool
context_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     DirectResult       ret;
     int                i;
     int                refs;
     int                level;
     CoreLayerContext  *context = (CoreLayerContext*) object;
     CoreLayer         *layer   = (CoreLayer*) ctx;

     if (object->state != FOS_ACTIVE)
          return true;

     if (context->layer_id != dfb_layer_id( layer ))
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

     printf( "%3d   ", refs );

     printf( "%4d x %4d  ", context->config.width, context->config.height );

     for (i=0; format_names[i].format; i++) {
          if (context->config.pixelformat == format_names[i].format) {
               printf( "%-8s ", format_names[i].name );
               break;
          }
     }

     if (!format_names[i].format)
          printf( "unknown  " );

     printf( "%.1f, %.1f -> %.1f, %.1f   ",
             context->screen.location.x,  context->screen.location.y,
             context->screen.location.x + context->screen.location.w,
             context->screen.location.y + context->screen.location.h );

     printf( "%2d     ", fusion_vector_size( &context->regions ) );

     printf( context->active ? "(*)    " : "       " );

     if (context == layer->shared->contexts.primary)
          printf( "SHARED   " );
     else
          printf( "PRIVATE  " );

     if (dfb_layer_get_level( layer, &level ))
          printf( "N/A" );
     else
          printf( "%3d", level );

     printf( "\n" );

     return true;
}

static void
dump_contexts( CoreLayer *layer )
{
     if (fusion_vector_size( &layer->shared->contexts.stack ) == 0)
          return;

     printf( "\n"
             "----------------------------------[ Contexts of Layer %d ]-----------------------------------\n", dfb_layer_id( layer ));
     printf( "Reference  . Refs  Width Height Format   Location on screen  Regions  Active  Info    Level\n" );
     printf( "--------------------------------------------------------------------------------------------\n" );

     dfb_core_enum_layer_contexts( NULL, context_callback, layer );
}

static DFBEnumerationResult
window_callback( CoreWindow *window,
                 void       *ctx )
{
     DirectResult      ret;
     int               refs;
     CoreWindowConfig *config = &window->config;
     DFBRectangle     *bounds = &config->bounds;

     ret = fusion_ref_stat( &window->object.ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return DFENUM_OK;
     }

#if FUSION_BUILD_MULTI
     printf( "0x%08x : ", window->object.ref.multi.id );
#else
     printf( "N/A        : " );
#endif

     printf( "%3d   ", refs );

     printf( "%4d, %4d   ", bounds->x, bounds->y );

     printf( "%4d x %4d    ", bounds->w, bounds->h );

     printf( "0x%02x ", config->opacity );

     printf( "%5d  ", window->id );

     if (window->caps & DWHC_TOPMOST) {
          printf( "*  " );
     }
     else {
          switch (config->stacking) {
               case DWSC_UPPER:
                    printf( "^  " );
                    break;
               case DWSC_MIDDLE:
                    printf( "-  " );
                    break;
               case DWSC_LOWER:
                    printf( "v  " );
                    break;
               default:
                    printf( "?  " );
                    break;
          }
     }

     if (window->caps & DWCAPS_ALPHACHANNEL)
          printf( "alphachannel   " );

     if (window->caps & DWCAPS_INPUTONLY)
          printf( "input only     " );

     if (window->caps & DWCAPS_DOUBLEBUFFER)
          printf( "double buffer  " );

     if (config->options & DWOP_GHOST)
          printf( "GHOST          " );

     if (DFB_WINDOW_FOCUSED( window ))
          printf( "FOCUSED        " );

     if (DFB_WINDOW_DESTROYED( window ))
          printf( "DESTROYED      " );

     printf( "\n" );

     return DFENUM_OK;
}

static void
dump_windows( CoreLayer *layer )
{
     DFBResult         ret;
     CoreLayerShared  *shared;
     CoreLayerContext *context;
     CoreWindowStack  *stack;

     shared = layer->shared;

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret) {
          D_DERROR( ret, "DirectFB/Dump: Could not lock the shared layer data!\n" );
          return;
     }

     context = layer->shared->contexts.primary;
     if (!context) {
          fusion_skirmish_dismiss( &shared->lock );
          return;
     }

     stack = dfb_layer_context_windowstack( context );
     if (!stack) {
          fusion_skirmish_dismiss( &shared->lock );
          return;
     }

     dfb_windowstack_lock( stack );

     if (stack->num) {
          printf( "\n"
                  "-----------------------------------[ Windows of Layer %d ]-----------------------------------\n", dfb_layer_id( layer ) );
          printf( "Reference  . Refs     X     Y   Width Height Opacity   ID     Capabilities   State & Options\n" );
          printf( "--------------------------------------------------------------------------------------------\n" );

          dfb_wm_enum_windows( stack, window_callback, NULL );
     }

     dfb_windowstack_unlock( stack );

     fusion_skirmish_dismiss( &shared->lock );
}

static DFBEnumerationResult
layer_callback( CoreLayer *layer,
                void      *ctx)
{
     dump_windows( layer );
     dump_contexts( layer );

     return DFENUM_OK;
}

static void
dump_layers()
{
     dfb_layers_enumerate( layer_callback, NULL );
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     long long millis;
     long int  seconds, minutes, hours, days;

     char *buffer = malloc( 0x10000 );

     setvbuf( stdout, buffer, _IOFBF, 0x10000 );

     /* DirectFB initialization. */
     ret = init_directfb( &argc, &argv );
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
               printf( "\nDirectFB uptime: %02ld:%02ld:%02ld\n",
                       hours, minutes, seconds );
               break;

          case 1:
               printf( "\nDirectFB uptime: %ld day, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;

          default:
               printf( "\nDirectFB uptime: %ld days, %02ld:%02ld:%02ld\n",
                       days, hours, minutes, seconds );
               break;
     }

     dump_surfaces();
     dump_layers();

#if FUSION_BUILD_MULTI
     if (argc > 1 && !strcmp( argv[1], "-s" )) {
          SHMemDesc           *desc;
          unsigned int         total = 0;
          FusionSHMPoolShared *pool  = dfb_core_shmpool(NULL);

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
     /* DirectFB deinitialization. */
     deinit_directfb();

     return ret;
}

