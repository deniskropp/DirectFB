#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <directfb.h>

#include "dvc.h"

/*****************************************************************************/

static char                  *file = NULL;
static DFBSurfacePixelFormat  sf   = DSPF_I420;
static int                    sw   = 480;
static int                    sh   = 360;

/*****************************************************************************/

static struct {
     DFBSurfacePixelFormat  id;
     const char            *name;
} formats[] = {
     { DSPF_RGB332,   "RGB332" },
     { DSPF_ARGB1555, "ARGB1555" },
     { DSPF_RGB16,    "RGB16" },
     { DSPF_RGB24,    "RGB24" },
     { DSPF_RGB32,    "RGB32" },
     { DSPF_ARGB,     "ARGB" },
     { DSPF_YUY2,     "YUY2" },
     { DSPF_UYVY,     "UYVY" },
     { DSPF_I420,     "I420" },
     { DSPF_YV12,     "YV12" },
     { DSPF_NV12,     "NV12" },
     { DSPF_NV21,     "NV21" }
};

#define NUM_FORMATS (sizeof(formats)/sizeof(formats[0]))

static DFBSurfacePixelFormat
format_name_to_id( const char *name )
{
     int i;
     
     for (i = 0; i < NUM_FORMATS; i++) {
          if (!strcmp( name, formats[i].name ))
               return formats[i].id;
     }
     
     return DSPF_UNKNOWN;
}

static const char*
format_id_to_name( DFBSurfacePixelFormat id )
{
     int i;
     
     for (i = 0; i < NUM_FORMATS; i++) {
          if (id == formats[i].id)
               return formats[i].name;
     }
     
     return "Unknown";
}

/*****************************************************************************/

static inline unsigned long 
millisec( void )
{
     return clock()/1000;
}

static unsigned int rand_pool = 0x12345678;
static unsigned int rand_add  = 0x87654321;

static inline unsigned int
myrand( void ) 
{
     rand_pool ^= ((rand_pool << 7) | (rand_pool >> 25));
     rand_pool += rand_add;
     rand_add  += rand_pool;
     
     return rand_pool;
}

/*****************************************************************************/



static void
usage( void )
{
     fprintf( stderr, "Usage: dvc_test [options] image-file\n\n" );
     fprintf( stderr, "Options:\n" );
     fprintf( stderr, "  -h, --help                   Show this help\n" );
     fprintf( stderr, "  -f, --format <format>        Set frame pixel format\n" );
     fprintf( stderr, "  -s, --size <width>x<height>  Set frame size\n" );
     fprintf( stderr, "\n" );
     exit( EXIT_FAILURE );
}

static void
parse_options( int argc, char **argv )
{
     int i;
     
     for (i = 1; i < argc; i++) {
          char *opt = argv[i];
           
          if (*opt != '-') {
               file = opt;
               continue;
          }
          
          if (!strcmp( opt, "-h" ) || !strcmp( opt, "--help" )) {
               usage();
          }
          else if (!strcmp( opt, "-f" ) || !strcmp( opt, "--format" )) {
               opt = argv[++i];
               if (opt) {
                    sf = format_name_to_id( opt );
                    if (!sf) {
                         fprintf( stderr, "Unsupported format '%s'!\n", opt );
                         exit( EXIT_FAILURE );
                    }
               } else {
                    fprintf( stderr, "No format specified!\n" );
                    exit( EXIT_FAILURE );
               }
          }
          else if (!strcmp( opt, "-s" ) || !strcmp( opt, "--size" )) {
               opt = argv[++i];
               if (opt) {
                    if (sscanf( opt, "%ux%u", &sw, &sh ) != 2) {
                         fprintf( stderr, "Invalid size '%s'!\n", opt );
                         exit( EXIT_FAILURE );
                    }
               } else {
                    fprintf( stderr, "No size specified!\n" );
                    exit( EXIT_FAILURE );
               }
          }
     }
     
     if (!file || !*file)
          usage();
}               

static void
init_picture( DVCPicture *picture, IDirectFBSurface *surface )
{
     DFBSurfacePixelFormat format;
     
     surface->GetPixelFormat( surface, &format );
     picture->format = dfb2dvc_pixelformat( format );
     surface->GetSize( surface, &picture->width, &picture->height );
     surface->Lock( surface, DSLF_READ, &picture->base[0], &picture->pitch[0] );
     if (DFB_PLANAR_PIXELFORMAT(format)) {
          switch (format) {
               case DSPF_I420:
                    picture->pitch[1] = 
                    picture->pitch[2] = picture->pitch[0]/2;
                    picture->base[1] = picture->base[0] + 
                                       picture->pitch[0] * picture->height;
                    picture->base[2] = picture->base[1] + 
                                       picture->pitch[1] * picture->height/2;
                    break;
               case DSPF_YV12:
                    picture->pitch[1] = 
                    picture->pitch[2] = picture->pitch[0]/2;
                    picture->base[2] = picture->base[0] + 
                                       picture->pitch[0] * picture->height;
                    picture->base[1] = picture->base[2] + 
                                       picture->pitch[2] * picture->height/2;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
                    picture->pitch[1] = picture->pitch[0] & ~1;
                    picture->base[1] = picture->base[0] + 
                                       picture->pitch[0] * picture->height;
                    break;
               default:
                    break;
          }
     }
     surface->Unlock( surface );
     
     picture->palette = NULL;
     picture->palette_size = 0;
     
     picture->premultiplied = 
     picture->separated = false;
}

int
main( int argc, char **argv )
{
     DFBResult               ret;
     IDirectFB              *dfb;
     IDirectFBSurface       *primary;
     IDirectFBSurface       *source;
     IDirectFBImageProvider *provider;
     IDirectFBEventBuffer   *buffer;
     DFBSurfaceDescription   dsc;
     DVCPicture              picture;
     unsigned long           start, now;
     unsigned long           num;
     int                     dw, dh;
     DFBSurfacePixelFormat   df;
     static char             buf[128];
          
     DirectFBInit( &argc, &argv );
     
     parse_options( argc, argv );
          
     ret = DirectFBCreate( &dfb );
     if (ret)
          DirectFBErrorFatal( "DirectFBCreate()", ret );
          
     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
     
     ret = dfb->CreateImageProvider( dfb, file, &provider );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::CreateImageProvider()", ret );
     
     dsc.flags       = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc.caps        = DSCAPS_SYSTEMONLY;
     dsc.width       = sw;
     dsc.height      = sh;
     dsc.pixelformat = sf;
     
     ret = dfb->CreateSurface( dfb, &dsc, &source );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::CreateSurface( source )", ret );
     provider->RenderTo( provider, source, NULL );
     provider->Release( provider );
     
     init_picture( &picture, source );
     
     dsc.flags = DSDESC_CAPS;
     dsc.caps  = DSCAPS_PRIMARY;
          
     ret = dfb->CreateSurface( dfb, &dsc, &primary );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::CreateSurface( primary )", ret );
     primary->GetSize( primary, &dw, &dh );
     primary->GetPixelFormat( primary, &df );
     primary->Clear( primary, 0, 0, 0, 0 );
     
     {
          IDirectFBFont      *font;
          DFBFontDescription  fdsc;
     
          fdsc.flags  = DFDESC_HEIGHT;
          fdsc.height = 36;
          
          ret = dfb->CreateFont( dfb, "/usr/share/fonts/truetype/decker.ttf", &fdsc, &font );
          if (ret)
               ret = dfb->CreateFont( dfb, NULL, &fdsc, &font );
          if (ret)
               DirectFBErrorFatal( "IDirectFB::CreateFont()", ret );
          primary->SetFont( primary, font );
          font->Release( font );
     }     
     
     ret = dfb->CreateInputEventBuffer( dfb, DICAPS_KEYS, 0, &buffer );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::CreateInputEventBuffer()", ret );
     
     sleep(2);
     
     printf( "** Benchmarking at %dx%d %s->%s **\n", 
             sw, sh, format_id_to_name(sf), format_id_to_name(df) );
     fflush( stdout );
     
     /* copy/convert test */
     start = millisec();
     num   = 0;
     do {
          DFBRectangle rect = { x:myrand()%(dw-sw), y:myrand()%(dh-sh), w:sw, h:sh };
          dvc_scale_to_surface( &picture, primary, &rect, NULL );
          num++;
     } while ((now=millisec()) < (start+3000));
     
     printf( "Copy/Convert    %.3f MPixel/s, %.3f Frames/s, (%.3f sec)\n", 
             (double)sw*sh*num/(double)(now-start)/1000.0, 
             (double)num*1000.0/(double)(now-start), (double)(now-start)/1000.0 );
     fflush( stdout );
     
     snprintf( buf, sizeof(buf), "%.3f MPixel/s, %.3f Frames/s", 
               (double)sw*sh*num/(double)(now-start)/1000.0,
               (double)num*1000.0/(double)(now-start) );
     primary->SetColor( primary, 0xff, 0xff, 0x00, 0xff );
     primary->DrawString( primary, buf, -1, dw/2, dh/2-36, DSTF_CENTER | DSTF_TOP );

     buffer->Reset( buffer );   
     buffer->WaitForEventWithTimeout( buffer, 5, 0 );
     buffer->Reset( buffer );
     
     /* scale/convert test */
     start = millisec();
     num   = 0;
     do {
          dvc_scale_to_surface( &picture, primary, NULL, NULL );
          num++;
     } while ((now=millisec()) < (start+3000));
     
     printf( "Scale/Convert   %.3f MPixel/s, %.3f Frames/s, (%.3f sec)\n", 
             (double)dw*dh*num/(double)(now-start)/1000.0, 
             (double)num*1000.0/(double)(now-start), (double)(now-start)/1000.0 );
     fflush( stdout );
     
     snprintf( buf, sizeof(buf), "%.3f MPixel/s, %.3f Frames/s", 
               (double)dw*dh*num/(double)(now-start)/1000.0,
               (double)num*1000.0/(double)(now-start) );
     primary->SetColor( primary, 0xff, 0xff, 0x00, 0xff );
     primary->DrawString( primary, buf, -1, dw/2, dh/2-36, DSTF_CENTER | DSTF_TOP );
     
     buffer->Reset( buffer );
     buffer->WaitForEventWithTimeout( buffer, 5, 0 );
     buffer->Reset( buffer );
             
     buffer->Release( buffer );
     source->Release( source );
     primary->Release( primary );
     dfb->Release( dfb );
     
     return 0;
}
