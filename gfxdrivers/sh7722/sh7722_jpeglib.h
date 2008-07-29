#ifndef __SH7722__SH7722_JPEGLIB_H__
#define __SH7722__SH7722_JPEGLIB_H__


typedef struct {
     int                      width;
     int                      height;
     bool                     mode420;
} SH7722_JPEG_info;


DirectResult SH7722_JPEG_Initialize();

DirectResult SH7722_JPEG_Shutdown();

DirectResult SH7722_JPEG_Open  ( DirectStream          *stream,
                                 SH7722_JPEG_info      *info );

DirectResult SH7722_JPEG_Decode( SH7722_JPEG_info      *info,
                                 DirectStream          *stream,
                                 const DFBRectangle    *rect,
                                 const DFBRegion       *clip,
                                 DFBSurfacePixelFormat  format,
                                 unsigned long          phys,
                                 int                    pitch,
                                 unsigned int           width,
                                 unsigned int           height );

DirectResult SH7722_JPEG_Encode( const char            *filename,
                                 const DFBRectangle    *rect,
                                 const DFBRegion       *clip,
                                 DFBSurfacePixelFormat  format,
                                 unsigned long          phys,
                                 int                    pitch,
                                 unsigned int           width,
                                 unsigned int           height );


#endif
