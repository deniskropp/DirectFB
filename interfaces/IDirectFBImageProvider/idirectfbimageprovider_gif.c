/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <malloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <misc/mem.h>
#include <misc/util.h>

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, GIF )


#ifndef NODEBUG
#define GIFERRORMSG(x...)     { fprintf( stderr, "(GIFLOADER) "x ); \
                                fprintf( stderr, "\n" ); }
#else
#define GIFERRORMSG(x...)
#endif

#define MAXCOLORMAPSIZE 256

#define CM_RED   0
#define CM_GREEN 1
#define CM_BLUE  2

#define MAX_LWZ_BITS 12

#define INTERLACE     0x40
#define LOCALCOLORMAP 0x80

#define BitSet(byte, bit) (((byte) & (bit)) == (bit))

#define LM_to_uint(a,b) (((b)<<8)|(a))

static struct {
     unsigned int  Width;
     unsigned int  Height;
     __u8          ColorMap[3][MAXCOLORMAPSIZE];
     unsigned int  BitPixel;
     unsigned int  ColorResolution;
     __u32         Background;
     unsigned int  AspectRatio;
     int GrayScale;
} GifScreen;

static struct {
     int transparent;
     int delayTime;
     int inputFlag;
     int disposal;
} Gif89 = { -1, -1, -1, 0 };

static bool verbose       = false;
static bool showComment   = true;
static bool ZeroDataBlock = false;

static __u32* ReadGIF( IDirectFBDataBuffer *buffer, int imageNumber,
                       int *width, int *height, bool *transparency,
                       __u32 *key_rgb, bool alpha, bool headeronly);

static bool ReadOK( IDirectFBDataBuffer *buffer, void *data, unsigned int len );

/*
 * private data struct of IDirectFBImageProvider_GIF
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBDataBuffer *buffer;
} IDirectFBImageProvider_GIF_data;

static DFBResult
IDirectFBImageProvider_GIF_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_GIF_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_GIF_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_GIF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context );

static DFBResult
IDirectFBImageProvider_GIF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_GIF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (strncmp (ctx->header, "GIF8", 4) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_GIF)

     data->ref    = 1;
     data->buffer = buffer;

     buffer->AddRef( buffer );

     thiz->AddRef = IDirectFBImageProvider_GIF_AddRef;
     thiz->Release = IDirectFBImageProvider_GIF_Release;
     thiz->RenderTo = IDirectFBImageProvider_GIF_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_GIF_SetRenderCallback;
     thiz->GetImageDescription = IDirectFBImageProvider_GIF_GetImageDescription;
     thiz->GetSurfaceDescription =
                               IDirectFBImageProvider_GIF_GetSurfaceDescription;

     return DFB_OK;
}

static void
IDirectFBImageProvider_GIF_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_GIF_data *data =
                                   (IDirectFBImageProvider_GIF_data*)thiz->priv;

     data->buffer->Release( data->buffer );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_GIF_AddRef( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_Release( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

     if (--data->ref == 0) {
          IDirectFBImageProvider_GIF_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     DFBRectangle           rect = { 0, 0, 0, 0 };
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     int err;

     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     err = destination->GetSize( destination, &rect.w, &rect.h );
     if (err)
          return err;

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;

     /* actual loading and rendering */
     if (dest_rect == NULL || dfb_rectangle_intersect ( &rect, dest_rect )) {
          __u32 *image_data;
          bool  transparency;
          void  *dst;
          int    pitch, src_width, src_height;

          image_data = ReadGIF( data->buffer, 1, &src_width, &src_height,
                                &transparency, NULL,
                                DFB_PIXELFORMAT_HAS_ALPHA (format),
                                false );

          if (image_data) {
               err = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
               if (err) {
                    DFBFREE( image_data );
                    return err;
               }

               dst += rect.x * DFB_BYTES_PER_PIXEL(format) + rect.y * pitch;

               dfb_scale_linear_32( dst, image_data,
                                    src_width, src_height, rect.w, rect.h,
                                    pitch, format, dst_surface->palette );

               destination->Unlock( destination );
               DFBFREE(image_data);
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_GIF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc )
{
     int  width;
     int  height;
     bool transparency;
     
     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)
          
     ReadGIF( data->buffer, 1, &width, &height,
              &transparency, NULL, false, true );

     dsc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width       = width;
     dsc->height      = height;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc )
{
     int   width;
     int   height;
     bool  transparency;
     __u32 key_rgb;
     
     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     ReadGIF( data->buffer, 1, &width, &height,
              &transparency, &key_rgb, false, true );

     if (transparency) {
          dsc->caps = DICAPS_COLORKEY;

          dsc->colorkey_r = (key_rgb & 0xff0000) >> 16;
          dsc->colorkey_g = (key_rgb & 0x00ff00) >>  8;
          dsc->colorkey_b = (key_rgb & 0x0000ff);
     }
     else
          dsc->caps = DICAPS_NONE;

     return DFB_OK;
}


/**********************************
         GIF Loader Code
 **********************************/

static int ReadColorMap( IDirectFBDataBuffer *buffer, int number,
                         __u8 buf[3][MAXCOLORMAPSIZE] )
{
     int     i;
     __u8 rgb[3];

     for (i = 0; i < number; ++i) {
          if (! ReadOK( buffer, rgb, sizeof(rgb) )) {
               GIFERRORMSG("bad colormap" );
               return true;
          }

          buf[CM_RED][i]   = rgb[0] ;
          buf[CM_GREEN][i] = rgb[1] ;
          buf[CM_BLUE][i]  = rgb[2] ;
     }
     return false;
}

static int GetDataBlock(IDirectFBDataBuffer *buffer, __u8 *buf)
{
     unsigned char count;

     if (! ReadOK( buffer, &count, 1 )) {
          GIFERRORMSG("error in getting DataBlock size" );
          return -1;
     }
     ZeroDataBlock = count == 0;

     if ((count != 0) && (! ReadOK( buffer, buf, count ))) {
          GIFERRORMSG("error in reading DataBlock" );
          return -1;
     }

     return count;
}

static int GetCode(IDirectFBDataBuffer *buffer, int code_size, int flag)
{
     static __u8 buf[280];
     static int curbit, lastbit, done, last_byte;
     int i, j, ret;
     unsigned char count;

     if (flag) {
          curbit = 0;
          lastbit = 0;
          done = false;
          return 0;
     }

     if ( (curbit+code_size) >= lastbit) {
          if (done) {
               if (curbit >= lastbit) {
                    GIFERRORMSG("ran off the end of my bits" );
               }
             return -1;
          }
          buf[0] = buf[last_byte-2];
          buf[1] = buf[last_byte-1];

          if ((count = GetDataBlock( buffer, &buf[2] )) == 0) {
               done = true;
          }

          last_byte = 2 + count;
          curbit = (curbit - lastbit) + 16;
          lastbit = (2+count) * 8;
     }

     ret = 0;
     for (i = curbit, j = 0; j < code_size; ++i, ++j) {
          ret |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
     }
     curbit += code_size;

     return ret;
}

static int DoExtension( IDirectFBDataBuffer *buffer, int label )
{
     unsigned char buf[256] = { 0 };
     char *str;

     switch (label) {
          case 0x01:              /* Plain Text Extension */
               str = "Plain Text Extension";
               break;
          case 0xff:              /* Application Extension */
               str = "Application Extension";
               break;
          case 0xfe:              /* Comment Extension */
               str = "Comment Extension";
               while (GetDataBlock( buffer, (__u8*) buf ) != 0) {
                    if (showComment)
                         GIFERRORMSG("gif comment: %s", buf );
                    }
               return false;
          case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock( buffer, (__u8*) buf );
               Gif89.disposal    = (buf[0] >> 2) & 0x7;
               Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
               Gif89.delayTime   = LM_to_uint( buf[1], buf[2] );
               if ((buf[0] & 0x1) != 0) {
                    Gif89.transparent = buf[3];
               }
               while (GetDataBlock( buffer, (__u8*) buf ) != 0)
                    ;
               return false;
          default:
               str = buf;
               snprintf(buf, 256, "UNKNOWN (0x%02x)", label);
          break;
     }

     if (verbose)
          GIFERRORMSG("got a '%s' extension", str );

     while (GetDataBlock( buffer, (__u8*) buf ) != 0)
          ;

     return false;
}

static int LWZReadByte( IDirectFBDataBuffer *buffer, int flag, int input_code_size )
{
     static int fresh = false;
     int code, incode;
     static int code_size, set_code_size;
     static int max_code, max_code_size;
     static int firstcode, oldcode;
     static int clear_code, end_code;
     static int table[2][(1<< MAX_LWZ_BITS)];
     static int stack[(1<<(MAX_LWZ_BITS))*2], *sp;
     int i;

     if (flag) {
          set_code_size = input_code_size;
          code_size = set_code_size+1;
          clear_code = 1 << set_code_size ;
          end_code = clear_code + 1;
          max_code_size = 2*clear_code;
          max_code = clear_code+2;

          GetCode(buffer, 0, true);

          fresh = true;

          for (i = 0; i < clear_code; ++i) {
               table[0][i] = 0;
               table[1][i] = i;
          }
          for (; i < (1<<MAX_LWZ_BITS); ++i) {
               table[0][i] = table[1][0] = 0;
          }
          sp = stack;

          return 0;
     }
     else if (fresh) {
          fresh = false;
          do {
               firstcode = oldcode = GetCode( buffer, code_size, false );
          } while (firstcode == clear_code);

          return firstcode;
     }

     if (sp > stack) {
          return *--sp;
     }

     while ((code = GetCode( buffer, code_size, false )) >= 0) {
          if (code == clear_code) {
               for (i = 0; i < clear_code; ++i) {
                    table[0][i] = 0;
                    table[1][i] = i;
               }
               for (; i < (1<<MAX_LWZ_BITS); ++i) {
                    table[0][i] = table[1][i] = 0;
               }
               code_size = set_code_size+1;
               max_code_size = 2*clear_code;
               max_code = clear_code+2;
               sp = stack;
               firstcode = oldcode = GetCode( buffer, code_size, false );

               return firstcode;
          }
          else if (code == end_code) {
               int count;
               __u8 buf[260];

               if (ZeroDataBlock) {
                    return -2;
               }

               while ((count = GetDataBlock( buffer, buf )) > 0)
                    ;

               if (count != 0)
                    GIFERRORMSG("missing EOD in data stream "
                                "(common occurence)");

               return -2;
          }

          incode = code;

          if (code >= max_code) {
               *sp++ = firstcode;
               code = oldcode;
          }

          while (code >= clear_code) {
               *sp++ = table[1][code];
               if (code == table[0][code]) {
                    GIFERRORMSG("circular table entry BIG ERROR");
               }
               code = table[0][code];
          }

          *sp++ = firstcode = table[1][code];

          if ((code = max_code) <(1<<MAX_LWZ_BITS)) {
               table[0][code] = oldcode;
               table[1][code] = firstcode;
               ++max_code;
               if ((max_code >= max_code_size)
                   && (max_code_size < (1<<MAX_LWZ_BITS)))
               {
                    max_code_size *= 2;
                    ++code_size;
               }
          }

          oldcode = incode;

          if (sp > stack) {
               return *--sp;
          }
     }
     return code;
}

static int SortColors (const void *a, const void *b)
{
     return (*((__u8 *) a) - *((__u8 *) b));
}

/*  looks for a color that is not in the colormap and ideally not
    even close to the colors used in the colormap  */
static __u32 FindColorKey( int n_colors, __u8 cmap[3][MAXCOLORMAPSIZE] )
{
     __u32 color = 0xFF000000;
     __u8  csort[MAXCOLORMAPSIZE];
     int   i, j, index, d;
     
     if (n_colors < 1)
          return color;

     DFB_ASSERT( n_colors <= MAXCOLORMAPSIZE );

     for (i = 0; i < 3; i++) {
          memcpy( csort, cmap[i], n_colors );
          qsort( csort, 1, n_colors, SortColors );
          
          for (j = 1, index = 0, d = 0; j < n_colors; j++) {
               if (csort[j] - csort[j-1] > d) {
                    d = csort[j] - csort[j-1];
                    index = j;
               }
          }
          if ((csort[0] - 0x0) > d) {
               d = csort[0] - 0x0;
               index = n_colors;
          }
          if (0xFF - (csort[n_colors - 1]) > d) {
               index = n_colors + 1;
          }

          if (index < n_colors)
               csort[0] = csort[index] - (d/2);
          else if (index == n_colors)
               csort[0] = 0x0;
          else
               csort[0] = 0xFF;

          color |= (csort[0] << (8 * (2 - i)));
     }

     return color;
}

static __u32* ReadImage( IDirectFBDataBuffer *buffer, int width, int height,
                         __u8 cmap[3][MAXCOLORMAPSIZE], __u32 key_rgb,
                         bool interlace, bool ignore )
{
     __u8 c;
     int v;
     int xpos = 0, ypos = 0, pass = 0;
     __u32 *image;

     /*
     **  Initialize the decompression routines
     */
     if (! ReadOK( buffer, &c, 1 ))
          GIFERRORMSG("EOF / read error on image data" );

     if (LWZReadByte( buffer, true, c ) < 0)
          GIFERRORMSG("error reading image" );

     /*
     **  If this is an "uninteresting picture" ignore it.
     */
     if (ignore) {
          if (verbose)
               GIFERRORMSG("skipping image..." );

          while (LWZReadByte( buffer, false, c ) >= 0)
               ;
          return NULL;
     }

     if ((image = DFBMALLOC(width * height * 4)) == NULL) {
          GIFERRORMSG("couldn't alloc space for image" );
     }

     if (verbose) {
          GIFERRORMSG("reading %d by %d%s GIF image", width, height,
                      interlace ? " interlaced" : "" );
     }

     while ((v = LWZReadByte( buffer, false, c )) >= 0 ) {
          __u32 *dst = image + (ypos * width + xpos);

          if (v == Gif89.transparent) {
               *dst++ = key_rgb;
          }
          else {
               *dst++ = (0xFF000000              |
                         cmap[CM_RED][v]   << 16 |
                         cmap[CM_GREEN][v] << 8  |
                         cmap[CM_BLUE][v]);
          }

          ++xpos;
          if (xpos == width) {
               xpos = 0;
               if (interlace) {
                    switch (pass) {
                         case 0:
                         case 1:
                              ypos += 8;
                              break;
                         case 2:
                              ypos += 4;
                              break;
                         case 3:
                              ypos += 2;
                              break;
                    }

                    if (ypos >= height) {
                         ++pass;
                         switch (pass) {
                              case 1:
                                   ypos = 4;
                                   break;
                              case 2:
                                   ypos = 2;
                                   break;
                              case 3:
                                   ypos = 1;
                              break;
                              default:
                                   goto fini;
                         }
                    }
               }
               else {
                    ++ypos;
               }
          }
          if (ypos >= height) {
               break;
          }
     }

fini:

     if (LWZReadByte( buffer, false, c ) >= 0) {
          GIFERRORMSG("too much input data, ignoring extra...");
     }
     return image;
}


static __u32* ReadGIF( IDirectFBDataBuffer *buffer, int imageNumber,
                       int *width, int *height, bool *transparency,
                       __u32 *key_rgb, bool alpha, bool headeronly)
{
     DFBResult ret;
     __u8      buf[16];
     __u8      c;
     __u8      localColorMap[3][MAXCOLORMAPSIZE];
     __u32     colorKey = 0;
     bool      useGlobalColormap;
     int       bitPixel;
     int       imageCount = 0;
     char      version[4];

     /* FIXME: support streamed buffers */
     ret = buffer->SeekTo( buffer, 0 );
     if (ret) {
          DirectFBError( "(DirectFB/ImageProvider_GIF) Unable to seek", ret );
          return NULL;
     }

     if (! ReadOK( buffer, buf, 6 )) {
          GIFERRORMSG("error reading magic number" );
     }

     if (strncmp( (char *)buf, "GIF", 3 ) != 0) {
          GIFERRORMSG("not a GIF file" );
     }

     strncpy( version, (char *)buf + 3, 3 );
     version[3] = '\0';

     if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0)) {
          GIFERRORMSG("bad version number, not '87a' or '89a'" );
     }

     if (! ReadOK(buffer,buf,7)) {
          GIFERRORMSG("failed to read screen descriptor" );
     }

     GifScreen.Width           = LM_to_uint( buf[0], buf[1] );
     GifScreen.Height          = LM_to_uint( buf[2], buf[3] );
     GifScreen.BitPixel        = 2 << (buf[4] & 0x07);
     GifScreen.ColorResolution = (((buf[4] & 0x70) >> 3) + 1);
     GifScreen.Background      = buf[5];
     GifScreen.AspectRatio     = buf[6];

     if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
          if (ReadColorMap( buffer, GifScreen.BitPixel, GifScreen.ColorMap )) {
               GIFERRORMSG("error reading global colormap" );
          }
     }

     if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49) {
          /* float r = ( (float) GifScreen.AspectRatio + 15.0 ) / 64.0; */
          GIFERRORMSG("warning - non-square pixels");
     }

     Gif89.transparent = -1;
     Gif89.delayTime   = -1;
     Gif89.inputFlag   = -1;
     Gif89.disposal    = 0;

     for (;;) {
          if (! ReadOK( buffer, &c, 1)) {
               GIFERRORMSG("EOF / read error on image data" );
          }

          if (c == ';') {         /* GIF terminator */
               if (imageCount < imageNumber) {
                    GIFERRORMSG("only %d image%s found in file",
                                imageCount, imageCount>1?"s":"" );
               }
               return NULL;
          }

          if (c == '!') {         /* Extension */
               if (! ReadOK( buffer, &c, 1)) {
                    GIFERRORMSG("OF / read error on extention function code");
               }
               DoExtension( buffer, c );
            continue;
          }

          if (c != ',') {         /* Not a valid start character */
               GIFERRORMSG("bogus character 0x%02x, ignoring", (int) c );
               continue;
          }

          ++imageCount;

          if (! ReadOK( buffer, buf, 9 )) {
               GIFERRORMSG("couldn't read left/top/width/height");
          }

          *width  = LM_to_uint( buf[4], buf[5] );
          *height = LM_to_uint( buf[6], buf[7] );
          *transparency = (Gif89.transparent != -1);

          if (headeronly && !(*transparency && key_rgb))
               return NULL;

          useGlobalColormap = ! BitSet( buf[8], LOCALCOLORMAP );

          if (useGlobalColormap) {
               if (*transparency && (key_rgb || !headeronly))
                    colorKey = FindColorKey( GifScreen.BitPixel,
                                             GifScreen.ColorMap );
          }
          else {
               bitPixel = 2 << (buf[8] & 0x07);
               if (ReadColorMap( buffer, bitPixel, localColorMap ))
                    GIFERRORMSG("error reading local colormap" );

               if (*transparency && (key_rgb || !headeronly))
                    colorKey = FindColorKey( bitPixel, localColorMap );
          }

          if (key_rgb)
               *key_rgb = colorKey;

          if (headeronly)
               return NULL;

          if (alpha)
               colorKey &= 0x00FFFFFF;

          return ReadImage( buffer, *width, *height,
                            (useGlobalColormap ?
                             GifScreen.ColorMap : localColorMap), colorKey,
                            BitSet( buf[8], INTERLACE ),
                            imageCount != imageNumber);
     }
}

static bool
ReadOK( IDirectFBDataBuffer *buffer, void *data, unsigned int len )
{
     DFBResult ret;

     ret = buffer->WaitForData( buffer, len );
     if (ret) {
          DirectFBError( "(DirectFB/ImageProvider_GIF) WaitForData failed", ret );
          return false;
     }

     ret = buffer->GetData( buffer, len, data, NULL );
     if (ret) {
          DirectFBError( "(DirectFB/ImageProvider_GIF) GetData failed", ret );
          return false;
     }

     return true;
}

