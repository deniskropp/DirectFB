#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <directfb.h>
#include <directfb_strings.h>

/**********************************************************************************************************************/

static const DirectFBPixelFormatNames(format_names);
static const DirectFBSurfaceBlittingFlagsNames(blittingflags_names);
static const DirectFBSurfaceDrawingFlagsNames(drawingflags_names);
static const DirectFBAccelerationMaskNames(accelerationmask_names);

/**********************************************************************************************************************/

typedef enum {
     NONE         = 0x00000000,
     CREATE_FILES = 0x00000001
} Options;

typedef struct {
     IDirectFB        *dfb;
     IDirectFBFont    *font;

     Options           options;
     const char       *directory;

     struct {
          FILE *formats;
     }                files;

     bool             device_drawstring;
} Inspector;

/**********************************************************************************************************************/

static DFBResult
Inspector_Init( Inspector *inspector, int argc, char *argv[] )
{
     int                i;
     DFBResult          ret;
     DFBFontDescription desc;

     memset( inspector, 0, sizeof(Inspector) );

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-d" )) {
               if (++i == argc) {
                    D_ERROR( "Inspector/Init: Missing argument to option '-d'!\n" );
                    return DFB_INVARG;
               }

               inspector->directory  = argv[i];
               inspector->options   |= CREATE_FILES;
          }
     }

     ret = DirectFBCreate( &inspector->dfb );
     if (ret) {
          D_DERROR( ret, "Inspector/Init: DirectFBCreate() failed!\n" );
          return ret;
     }

     desc.flags  = DFDESC_HEIGHT;
     desc.height = 24;

     ret = inspector->dfb->CreateFont( inspector->dfb, DATADIR"/decker.ttf", &desc, &inspector->font );
     if (ret) {
          D_DERROR( ret, "Inspector/Init: Could not load font '%s'!\n", DATADIR"/decker.ttf" );
          return ret;
     }

     return DFB_OK;
}

static DFBResult
Inspector_Run( Inspector *inspector )
{
     static const DFBSurfacePixelFormat formats[] = {
          DSPF_LUT8,
          DSPF_ALUT44,
          DSPF_RGB332,
          DSPF_RGB16,
          DSPF_RGB24,
          DSPF_RGB32,
          DSPF_ARGB1555,
          DSPF_RGBA5551,
          DSPF_ARGB2554,
          DSPF_ARGB4444,
          DSPF_ARGB8565,
          DSPF_ARGB,
          DSPF_AiRGB,
          DSPF_A1,
          DSPF_A1_LSB,
          DSPF_A8,
          DSPF_YUY2,
          DSPF_UYVY,
          DSPF_I420,
          DSPF_YV12,
          DSPF_NV12,
          DSPF_NV21,
          DSPF_NV16,
          DSPF_AYUV,
          DSPF_YUV444P,
          DSPF_AVYU,
          DSPF_VYU,
     };

     int                    i, j, n;
     DFBResult              ret;
     DFBSurfaceDescription  desc;
     IDirectFBSurface      *surfaces[D_ARRAY_SIZE(formats)];
     char                   buf[strlen( inspector->directory ? : "" ) + 23];


     if (inspector->options & CREATE_FILES) {
          /* Create the directory. */
          if (mkdir( inspector->directory, 0755 ) < 0 && errno != EEXIST) {
               D_PERROR( "Inspector/Init: Could not create directory '%s'!\n", inspector->directory );
               return DFB_INIT;
          }

          /* Open file for writing supported format conversions (blitting). */
          snprintf( buf, sizeof(buf), "%s/blit.formats", inspector->directory );
          inspector->files.formats = fopen( buf, "w" );
          if (!inspector->files.formats) {
               D_PERROR( "Inspector/Init: Could not open file '%s' for writing!\n", buf );
               return DFB_INIT;
          }
     }

     desc.flags  = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH  |
                                                DSDESC_HEIGHT |
                                                DSDESC_PIXELFORMAT);
     desc.width  = 64;
     desc.height = 64;

     for (i=0; i<D_ARRAY_SIZE(formats); i++) {
          desc.pixelformat = formats[i];

          ret = inspector->dfb->CreateSurface( inspector->dfb, &desc, &surfaces[i] );
          if (ret) {
               D_DERROR( ret, "Inspector/Init: Could not create %s surface!\n",
                         format_names[DFB_PIXELFORMAT_INDEX(formats[i])].name );
               while (i--)
                    surfaces[i]->Release( surfaces[i] );
               return ret;
          }

          surfaces[i]->SetFont( surfaces[i], inspector->font );

          if (inspector->files.formats)
               fprintf( inspector->files.formats, "%s%s", i ? "," : "",
                        format_names[DFB_PIXELFORMAT_INDEX(formats[i])].name );
     }

     if (inspector->files.formats)
          fprintf( inspector->files.formats, "\n" );

     printf("\n");



     printf("source ->");

     for (i=0; i<D_ARRAY_SIZE(formats); i++)
          printf( "%9s", format_names[DFB_PIXELFORMAT_INDEX(formats[i])].name );

     printf("\n");



     printf("dest.\n");

     for (i=0; i<D_ARRAY_SIZE(formats); i++) {
          printf( "%9s", format_names[DFB_PIXELFORMAT_INDEX(formats[i])].name );

          for (j=0; j<D_ARRAY_SIZE(formats); j++) {
               DFBAccelerationMask mask;

               surfaces[i]->GetAccelerationMask( surfaces[i], surfaces[j], &mask );

               if (mask & DFXL_DRAWSTRING)
                    inspector->device_drawstring = true;

               printf( "%9s", (mask & DFXL_BLIT) ? "X" : "" );

               if (inspector->files.formats)
                    fputc( (mask & DFXL_BLIT) ? '1' : '0', inspector->files.formats );
          }

          printf( "  %s", format_names[DFB_PIXELFORMAT_INDEX(formats[i])].name );

          printf("\n");

          if (inspector->files.formats)
               fprintf( inspector->files.formats, "\n" );
     }

     for (i=0; i<D_ARRAY_SIZE(formats); i++)
          surfaces[i]->Release( surfaces[i] );

     if (inspector->options & CREATE_FILES) {
          FILE                         *f;
          DFBGraphicsDeviceDescription  desc;

          if (inspector->files.formats)
               fclose( inspector->files.formats );

          /* Query device and driver information. */
          inspector->dfb->GetDeviceDescription( inspector->dfb, &desc );

          if (inspector->device_drawstring)
               desc.acceleration_mask = (DFBAccelerationMask)(desc.acceleration_mask | DFXL_DRAWSTRING);

          /* Write device info to a file. */
          snprintf( buf, sizeof(buf), "%s/device.info", inspector->directory );
          f = fopen( buf, "w" );
          if (!f) {
               D_PERROR( "Inspector/Init: Could not open file '%s' for writing!\n", buf );
               return DFB_FAILURE;
          }

          fprintf( f, "name = %s\n", desc.name );
          fprintf( f, "vendor = %s\n", desc.vendor );

          fprintf( f, "acceleration_mask = " );
          for (i=0, n=0; accelerationmask_names[i].mask; i++) {
               if (desc.acceleration_mask & accelerationmask_names[i].mask)
                    fprintf( f, "%s%s", n++ ? "," : "", accelerationmask_names[i].name );
          }
          fprintf( f, "\n" );

          fprintf( f, "blitting_flags = " );
          for (i=0, n=0; blittingflags_names[i].flag; i++) {
               if (desc.blitting_flags & blittingflags_names[i].flag)
                    fprintf( f, "%s%s", n++ ? "," : "", blittingflags_names[i].name );
          }
          fprintf( f, "\n" );

          fprintf( f, "drawing_flags = " );
          for (i=0, n=0; drawingflags_names[i].flag; i++) {
               if (desc.drawing_flags & drawingflags_names[i].flag)
                    fprintf( f, "%s%s", n++ ? "," : "", drawingflags_names[i].name );
          }
          fprintf( f, "\n" );

          fclose( f );


          /* Write driver info to a file. */
          snprintf( buf, sizeof(buf), "%s/driver.info", inspector->directory );
          f = fopen( buf, "w" );
          if (!f) {
               D_PERROR( "Inspector/Init: Could not open file '%s' for writing!\n", buf );
               return DFB_FAILURE;
          }

          fprintf( f, "name = %s\n", desc.driver.name );
          fprintf( f, "vendor = %s\n", desc.driver.vendor );
          fprintf( f, "version = %d.%d\n", desc.driver.major, desc.driver.minor );

          fclose( f );
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;
     Inspector inspector;

     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "Inspector/Init: DirectFBInit() failed!\n" );
          return ret;
     }

     ret = Inspector_Init( &inspector, argc, argv );
     if (ret)
          return ret;
          
     return Inspector_Run( &inspector );
}

