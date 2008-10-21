#ifdef SH7722_DEBUG_JPEG
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/mman.h>

#ifdef STANDALONE
#include "sh7722_jpeglib_standalone.h"
#else
#include <config.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/stream.h>
#include <direct/util.h>

#include <directfb.h>
#endif

#include "sh7722_jpeglib.h"


void
write_ppm( const char    *filename,
           unsigned long  phys,
           int            pitch,
           unsigned int   width,
           unsigned int   height )
{
     int   i;
     int   fd;
     int   size;
     void *mem;
     FILE *file;

     size = direct_page_align( pitch * height );

     fd = open( "/dev/mem", O_RDWR );
     if (fd < 0) {
          D_PERROR( "SH7722/JPEG: Could not open /dev/mem!\n" );
          return;
     }

     mem = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys );
     if (mem == MAP_FAILED) {
          D_PERROR( "SH7722/JPEG: Could not map /dev/mem at 0x%08lx (length %d)!\n", phys, size );
          close( fd );
          return;
     }

     close( fd );

     file = fopen( filename, "wb" );
     if (!file) {
          D_PERROR( "SH7722/JPEG: Could not open '%s' for writing!\n", filename );
          munmap( mem, size );
          return;
     }

     fprintf( file, "P6\n%d %d\n255\n", width, height );

     for (i=0; i<height; i++) {
          fwrite( mem, 3, width, file );

          mem += pitch;
     }

     fclose( file );

     munmap( mem, size );
}


int
main( int argc, char *argv[] )
{
     DirectResult           ret;
     SH7722_JPEG_context    info;
     DFBSurfacePixelFormat  format;
     int                    pitch;
     DirectStream          *stream = NULL;

     if (argc != 2) {
          fprintf( stderr, "Usage: %s <filename>\n", argv[0] );
          return -1;
     }

#ifndef STANDALONE
     direct_initialize();

     direct_config->debug = true;
#endif

     ret = SH7722_JPEG_Initialize();
     if (ret)
          return ret;

     ret = direct_stream_create( argv[1], &stream );
     if (ret)
          goto out;

     ret = SH7722_JPEG_Open( stream, &info );
     if (ret)
          goto out;

     D_INFO( "SH7722/JPEGTool: Opened %dx%d image (4:%s)\n", info.width, info.height,
             info.mode420 ? "2:0" : info.mode444 ? "4:4" : "2:2?" );

     format = DSPF_RGB24;// info.mode444 ? DSPF_NV16 : DSPF_NV12;
     pitch  = (DFB_BYTES_PER_LINE( format, info.width ) + 31) & ~31;

     ret = SH7722_JPEG_Decode( &info, NULL, NULL, format, 0x0f800000, NULL, pitch, info.width, info.height );
     if (ret)
          goto out;


//  Use RGB24 format for this
//     write_ppm( "test.ppm", 0x0f800000, pitch, info.width, info.height );

     ret = SH7722_JPEG_Encode( "test.jpg", NULL, format, 0x0f800000, pitch, info.width, info.height, 0 );
     if (ret)
          goto out;


out:
     if (stream)
          direct_stream_destroy( stream );

     SH7722_JPEG_Shutdown();

     return ret;
}
