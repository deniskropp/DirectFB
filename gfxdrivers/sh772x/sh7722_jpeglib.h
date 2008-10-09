#ifndef __SH7722__SH7722_JPEGLIB_H__
#define __SH7722__SH7722_JPEGLIB_H__

#include <jpeglib.h>

typedef struct {
     DirectStream                  *stream;

     int                            width;
     int                            height;
     bool                           mode420;
     bool                           mode444;

     struct jpeg_decompress_struct  cinfo;
} SH7722_JPEG_context;


DirectResult SH7722_JPEG_Initialize();

DirectResult SH7722_JPEG_Shutdown();

DirectResult SH7722_JPEG_Open  ( DirectStream          *stream,
                                 SH7722_JPEG_context   *context );

DirectResult SH7722_JPEG_Decode( SH7722_JPEG_context   *context,
                                 const DFBRectangle    *rect,
                                 const DFBRegion       *clip,
                                 DFBSurfacePixelFormat  format,
                                 unsigned long          phys,
                                 void                  *addr,
                                 int                    pitch,
                                 unsigned int           width,
                                 unsigned int           height );

DirectResult SH7722_JPEG_Close ( SH7722_JPEG_context   *context );

DirectResult SH7722_JPEG_Encode( const char            *filename,
                                 const DFBRectangle    *srcrect,
                                 DFBSurfacePixelFormat  srcformat,
                                 unsigned long          srcphys,
                                 int                    srcpitch,
                                 unsigned int           width,
                                 unsigned int           height,
                                 unsigned int           tmpphys );


#endif
