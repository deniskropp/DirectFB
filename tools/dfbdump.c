#include <stdio.h>

#include <directfb.h>

#include <core/fusion/ref.h>
#include <core/fusion/object.h>

#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/windows.h>

typedef struct {
     int video;
     int system;
} MemoryUsage;

static IDirectFB *dfb = NULL;

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
     
     return (mem + 0x3ff) & ~0x3ff;
}

static bool
surface_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     FusionResult ret;
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

     printf( "%2d  ", refs );
     
     printf( "%4d x %4d   ", surface->width, surface->height );

     switch (surface->format) {
          case DSPF_A8:
               printf( "A8       " );
               break;

          case DSPF_ARGB:
               printf( "ARGB     " );
               break;

          case DSPF_ARGB1555:
               printf( "ARGB1555 " );
               break;

          case DSPF_I420:
               printf( "I420     " );
               break;

          case DSPF_LUT8:
               printf( "LUT8     " );
               break;

          case DSPF_ALUT44:
               printf( "ALUT44   " );
               break;

          case DSPF_RGB16:
               printf( "RGB16    " );
               break;

          case DSPF_RGB24:
               printf( "RGB24    " );
               break;

          case DSPF_RGB32:
               printf( "RGB32    " );
               break;

          case DSPF_RGB332:
               printf( "RGB332   " );
               break;

          case DSPF_UYVY:
               printf( "UYVY     " );
               break;

          case DSPF_YUY2:
               printf( "YUY2     " );
               break;

          case DSPF_YV12:
               printf( "YV12     " );
               break;

          default:
               printf( "unknown! " );
               break;
     }

     vmem = buffer_sizes( surface, true );
     smem = buffer_sizes( surface, false );

     printf( "%6dk  ", vmem >> 10 );
     printf( "%6dk  ", smem >> 10 );

     mem->video  += vmem;
     mem->system += smem;

     if (surface->caps & DSCAPS_SYSTEMONLY)
          printf( "system only  " );

     if (surface->caps & DSCAPS_VIDEOONLY)
          printf( "video only   " );

     if (surface->caps & DSCAPS_FLIPPING)
          printf( "flipping     " );

     if (surface->caps & DSCAPS_TRIPLE)
          printf( "triple       " );

     if (surface->caps & DSCAPS_INTERLACED)
          printf( "interlaced   " );

     printf( "\n" );

     return true;
}

static void
dump_surfaces()
{
     MemoryUsage mem = { 0, 0 };

     printf( "\nSurfaces\n" );
     printf( "---------\n\n" );

     fusion_object_pool_enum( dfb_gfxcard_surface_pool(),
                              surface_callback, &mem );

     printf( "                           -------  -------\n" );
     printf( "                           %6dk  %6dk    -> %dk total\n",
             mem.video >> 10, mem.system >> 10, (mem.video + mem.system) >> 10);
}

static bool
window_callback( CoreWindow      *window,
                 CoreWindowStack *stack )
{
     FusionResult ret;
     int          refs;

     ret = fusion_ref_stat( &window->object.ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return false;
     }

     printf( "%2d  ", refs );

     printf( "%4d, %4d   ", window->x, window->y );

     printf( "%4d x %4d   ", window->width, window->height );
     
     printf( "0x%02x  ", window->opacity );

     if (window->caps & DWHC_TOPMOST) {
          printf( "*  " );
     }
     else {
          switch (window->stacking) {
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

     if (stack->focused_window == window)
          printf( "FOCUSED        " );

     printf( "\n" );

     return true;
}

static DFBEnumerationResult
layer_callback( DisplayLayer *layer,
                void         *ctx)
{
     int              i;
     CoreWindowStack *stack = dfb_layer_window_stack( layer );

     if (!stack)
          return DFENUM_OK;
     
     printf( "\nWindows on layer %d\n", dfb_layer_id( layer ) );
     printf( "-------------------\n\n" );

     fusion_skirmish_prevail( &stack->lock );
     
     for (i=stack->num_windows - 1; i>=0; i--) {
          if (!window_callback( stack->windows[i], stack ))
               break;
     }

     fusion_skirmish_dismiss( &stack->lock );
     
     return DFENUM_OK;
}

static void
dump_windows()
{
     dfb_layers_enumerate( layer_callback, NULL );
}

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     long long millis;
     long int  seconds, minutes, hours, days;

     /* DirectFB initialization. */
     ret = init_directfb( &argc, &argv );
     if (ret)
          goto out;

     millis = fusion_get_millis();

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
     dump_windows();

out:
     /* DirectFB deinitialization. */
     deinit_directfb();

     return ret;
}

