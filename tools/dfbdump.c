#include <stdio.h>

#include <directfb.h>

#include <core/fusion/ref.h>
#include <core/fusion/object.h>

#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/windows.h>


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

static bool
surface_callback( FusionObjectPool *pool,
                  FusionObject     *object,
                  void             *ctx )
{
     FusionResult ret;
     int          refs;
     CoreSurface *surface = (CoreSurface*) object;
     int         *total   = (int*) ctx;
     int          mem;

     ret = fusion_ref_stat( &object->ref, &refs );
     if (ret) {
          printf( "Fusion error %d!\n", ret );
          return false;
     }

     printf( "%2d  ", refs );
     
     printf( "%4d x %4d   ", surface->width, surface->height );

     switch (surface->format) {
          case DSPF_A8:
               printf( "A8      " );
               break;

          case DSPF_ARGB:
               printf( "ARGB    " );
               break;

          case DSPF_I420:
               printf( "I420    " );
               break;

          case DSPF_LUT8:
               printf( "LUT8    " );
               break;

          case DSPF_RGB15:
               printf( "RGB15   " );
               break;

          case DSPF_RGB16:
               printf( "RGB16   " );
               break;

          case DSPF_RGB24:
               printf( "RGB24   " );
               break;

          case DSPF_RGB32:
               printf( "RGB32   " );
               break;

          case DSPF_RGB332:
               printf( "RGB332  " );
               break;

          case DSPF_UYVY:
               printf( "UYVY    " );
               break;

          case DSPF_YUY2:
               printf( "YUY2    " );
               break;

          case DSPF_YV12:
               printf( "YV12    " );
               break;

          default:
               printf( "unknown!" );
               break;
     }

     mem = DFB_BYTES_PER_LINE( surface->format, surface->width ) *
           surface->height * ((surface->caps & DSCAPS_FLIPPING) ? 2 : 1);

     if (mem < 1024)
          mem = 1024;

     printf( "%4dk  ", mem >> 10 );

     *total += mem;

     if (surface->caps & DSCAPS_SYSTEMONLY)
          printf( "system only  " );

     if (surface->caps & DSCAPS_VIDEOONLY)
          printf( "video only   " );

     if (surface->caps & DSCAPS_FLIPPING)
          printf( "flipping     " );

     if (surface->caps & DSCAPS_INTERLACED)
          printf( "interlaced   " );

     printf( "\n" );

     return true;
}

static void
dump_surfaces()
{
     int total = 0;

     printf( "\nSurfaces\n" );
     printf( "---------\n\n" );

     fusion_object_pool_enum( dfb_gfxcard_surface_pool(),
                              surface_callback, &total );

     printf( "                         ------\n" );
     printf( "                         %4dk\n", total >> 10 );
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

     printf( "%4d x %4d   ", window->width, window->height );
     
     printf( "%4d, %4d   ", window->x, window->y );

     printf( "0x%02x  ", window->opacity );

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

     if (stack->focused_window == window)
          printf( "FOCUSED        " );

     if (window->caps & DWCAPS_ALPHACHANNEL)
          printf( "alphachannel   " );

     if (window->caps & DWCAPS_INPUTONLY)
          printf( "input only     " );

     if (window->caps & DWCAPS_DOUBLEBUFFER)
          printf( "double buffer  " );

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

     skirmish_prevail( &stack->lock );
     
     for (i=stack->num_windows - 1; i>=0; i--) {
          if (!window_callback( stack->windows[i], stack ))
               break;
     }

     skirmish_dismiss( &stack->lock );
     
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

     /* DirectFB initialization. */
     ret = init_directfb( &argc, &argv );
     if (ret)
          goto out;

     dump_surfaces();
     dump_windows();

out:
     /* DirectFB deinitialization. */
     deinit_directfb();

     return ret;
}
