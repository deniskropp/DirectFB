/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <misc/mem.h>
#include <misc/util.h>

static DFBResult
Probe( const char *head, const char *filename );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, GIF )


#ifndef NODEBUG
#define GIFERRORMSG(x...)     { fprintf( stderr, "(GIFLOADER) "x ); \
                                fprintf( stderr, "\n" ); }
#else
#define GIFERRORMSG(x...)
#endif

#define MAXCOLORMAPSIZE 256

#define TRUE  1
#define FALSE 0

#define CM_RED   0
#define CM_GREEN 1
#define CM_BLUE  2

#define MAX_LWZ_BITS 12

#define INTERLACE     0x40
#define LOCALCOLORMAP 0x80

#define BitSet(byte, bit) (((byte) & (bit)) == (bit))

#define ReadOK(file,buffer,len) (fread(buffer, len, 1, file) != 0)

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

static int verbose = 1;
static int showComment = 1;
static int ZeroDataBlock = FALSE;

static __u32* ReadGIF( FILE *fd, int imageNumber,
                       int *width, int *height, int *transparency,
                       __u32 *key_rgb, int headeronly);

/*
 * private data struct of IDirectFBImageProvider_GIF
 */
typedef struct {
     int            ref;      /* reference counter */
     char          *filename; /* filename of file to load */
} IDirectFBImageProvider_GIF_data;

static DFBResult
IDirectFBImageProvider_GIF_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_GIF_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_GIF_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination );

static DFBResult
IDirectFBImageProvider_GIF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_GIF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );


static DFBResult
Probe( const char *head, const char *filename )
{
     if (strncmp (head, "GIF8", 4) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           const char             *filename )
{
     IDirectFBImageProvider_GIF_data *data;

     data = (IDirectFBImageProvider_GIF_data*)
          DFBCALLOC( 1, sizeof(IDirectFBImageProvider_GIF_data) );

     thiz->priv = data;

     data->ref = 1;
     data->filename = (char*)DFBMALLOC( strlen(filename)+1 );
     strcpy( data->filename, filename );

     DEBUGMSG( "DirectFB/Media: GIF Provider Construct '%s'\n", filename );

     thiz->AddRef = IDirectFBImageProvider_GIF_AddRef;
     thiz->Release = IDirectFBImageProvider_GIF_Release;
     thiz->RenderTo = IDirectFBImageProvider_GIF_RenderTo;
     thiz->GetImageDescription = IDirectFBImageProvider_GIF_GetImageDescription;
     thiz->GetSurfaceDescription =
                               IDirectFBImageProvider_GIF_GetSurfaceDescription;

     return DFB_OK;
}

static void IDirectFBImageProvider_GIF_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_GIF_data *data =
                                   (IDirectFBImageProvider_GIF_data*)thiz->priv;

     DFBFREE( data->filename );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBImageProvider_GIF_AddRef( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_GIF_Release( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

     if (--data->ref == 0) {
          IDirectFBImageProvider_GIF_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_GIF_RenderTo( IDirectFBImageProvider *thiz,
                                               IDirectFBSurface *destination )
{
     int err;
     void *dst;
     int pitch, width, height, src_width, src_height, transparency;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps;

     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     err = destination->GetCapabilities( destination, &caps );
     if (err)
          return err;

     err = destination->GetSize( destination, &width, &height );
     if (err)
          return err;

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;


     /* actual loading and rendering */
     {
          __u32 *image_data;
          FILE *f;

          f = fopen( data->filename, "rb" );
          if (!f)
               return errno2dfb( errno );

          image_data = ReadGIF( f, 1, &src_width, &src_height,
                                &transparency, NULL, 0 );
          if (image_data) {
               err = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
               if (err) {
                    DFBFREE( image_data );
                    fclose( f );
                    return err;
               }

               dfb_scale_linear_32( dst, image_data,
                                    src_width, src_height, width, height,
                                    pitch - DFB_BYTES_PER_LINE(format, width),
                                    format );

               destination->Unlock( destination );
               DFBFREE(image_data);
          }
          fclose (f);
     }

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_GIF_GetSurfaceDescription(
                                                   IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription *dsc )
{
     FILE *f;

     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     f = fopen( data->filename, "rb" );
     if (!f)
          return errno2dfb( errno );

     {
          int width;
          int height;
          int transparency;

          ReadGIF( f, 1, &width, &height, &transparency, NULL, 1 ); // 1 = read header only

          dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc->width  = width;
          dsc->height = height;
          dsc->pixelformat = dfb_primary_layer_pixelformat();

          fclose (f);
     }

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_GIF_GetImageDescription(
                                                   IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc )
{
     FILE *f;

     INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     f = fopen( data->filename, "rb" );
     if (!f)
          return errno2dfb( errno );

     {
          int   width;
          int   height;
          int   transparency;
          __u32 key_rgb;

          ReadGIF( f, 1, &width, &height, &transparency, &key_rgb, 1 ); // 1 = read header only

          if (transparency) {
               dsc->caps = DICAPS_COLORKEY;

               dsc->colorkey_r = 0;//(key_rgb & 0xff0000) >> 16;
               dsc->colorkey_g = 0xFF;//(key_rgb & 0x00ff00) >>  8;
               dsc->colorkey_b = 0;//(key_rgb & 0x0000ff);
          }
          else
               dsc->caps = DICAPS_NONE;

          fclose (f);
     }

     return DFB_OK;
}


/**********************************
         GIF Loader Code
 **********************************/

static int ReadColorMap( FILE *fd, int number,
                         __u8 buffer[3][MAXCOLORMAPSIZE] )
{
     int     i;
     __u8 rgb[3];

     for (i = 0; i < number; ++i) {
          if (! ReadOK(fd, rgb, sizeof(rgb))) {
               GIFERRORMSG("bad colormap" );
               return TRUE;
          }

          buffer[CM_RED][i] = rgb[0] ;
          buffer[CM_GREEN][i] = rgb[1] ;
          buffer[CM_BLUE][i] = rgb[2] ;
     }
     return FALSE;
}

static int GetDataBlock(FILE *fd, __u8 *buf)
{
     unsigned char count;

     if (! ReadOK(fd,&count,1)) {
          GIFERRORMSG("error in getting DataBlock size" );
          return -1;
     }
     ZeroDataBlock = count == 0;

     if ((count != 0) && (! ReadOK(fd, buf, count))) {
          GIFERRORMSG("error in reading DataBlock" );
          return -1;
     }

     return count;
}

static int GetCode(FILE *fd, int code_size, int flag)
{
     static __u8 buf[280];
     static int curbit, lastbit, done, last_byte;
     int i, j, ret;
     unsigned char count;

     if (flag) {
          curbit = 0;
          lastbit = 0;
          done = FALSE;
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

          if ((count = GetDataBlock(fd, &buf[2])) == 0) {
               done = TRUE;
          }

          last_byte = 2 + count;
          curbit = (curbit - lastbit) + 16;
          lastbit = (2+count)*8 ;
     }

     ret = 0;
     for (i = curbit, j = 0; j < code_size; ++i, ++j) {
          ret |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
     }
     curbit += code_size;

     return ret;
}

static int DoExtension( FILE *fd, int label )
{
     static unsigned char buf[256];
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
               while (GetDataBlock(fd, (__u8*) buf) != 0) {
                    if (showComment)
                         GIFERRORMSG("gif comment: %s", buf );
                    }
               return FALSE;
          case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock(fd, (__u8*) buf);
               Gif89.disposal    = (buf[0] >> 2) & 0x7;
               Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
               Gif89.delayTime   = LM_to_uint(buf[1],buf[2]);
               if ((buf[0] & 0x1) != 0) {
                    Gif89.transparent = buf[3];
               }
               while (GetDataBlock(fd, (__u8*) buf) != 0)
                    ;
               return FALSE;
          default:
               str = buf;
               snprintf(buf, 256, "UNKNOWN (0x%02x)", label);
          break;
     }

     GIFERRORMSG("got a '%s' extension", str );

     while (GetDataBlock(fd, (__u8*) buf) != 0)
          ;

     return FALSE;
}

static int LWZReadByte( FILE *fd, int flag, int input_code_size )
{
     static int fresh = FALSE;
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

          GetCode(fd, 0, TRUE);

          fresh = TRUE;

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
          fresh = FALSE;
          do {
               firstcode = oldcode = GetCode(fd, code_size, FALSE);
          } while (firstcode == clear_code);

          return firstcode;
     }

     if (sp > stack) {
          return *--sp;
     }

     while ((code = GetCode(fd, code_size, FALSE)) >= 0) {
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
               firstcode = oldcode = GetCode(fd, code_size, FALSE);

               return firstcode;
          }
          else if (code == end_code) {
               int count;
               __u8 buf[260];

               if (ZeroDataBlock) {
                    return -2;
               }

               while ((count = GetDataBlock(fd, buf)) > 0)
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


static __u32* ReadImage( FILE *fd, int len, int height,
                         __u8 cmap[3][MAXCOLORMAPSIZE],
                         int interlace, int ignore )
{
     __u8 c;
     int v;
     int xpos = 0, ypos = 0, pass = 0;
     __u32 *image;

     /*
     **  Initialize the decompression routines
     */
     if (! ReadOK(fd,&c,1))
          GIFERRORMSG("EOF / read error on image data" );

     if (LWZReadByte(fd, TRUE, c) < 0)
          GIFERRORMSG("error reading image" );

     /*
     **  If this is an "uninteresting picture" ignore it.
     */
     if (ignore) {
          if (verbose) {
               GIFERRORMSG("skipping image..." );
          }
          while (LWZReadByte(fd, FALSE, c) >= 0)
               ;
          return NULL;
     }

     if ((image = DFBMALLOC(len * height * 4)) == NULL) {
          GIFERRORMSG("couldn't alloc space for image" );
     }

     if (verbose) {
          GIFERRORMSG("reading %d by %d%s GIF image", len, height,
                      interlace ? " interlaced" : "" );
     }

     while ((v = LWZReadByte(fd,FALSE,c)) >= 0 ) {
          __u32 *dst = image + (ypos * len + xpos);

          if (v == Gif89.transparent) {
               *dst++ = 0xFF00FF00;
          }
          else {
               __u32 color = cmap[CM_RED][v]   << 16 |
                             cmap[CM_GREEN][v] << 8  |
                             cmap[CM_BLUE][v];

               /* very ugly quick hack to preserve the colorkey for 16bit,
                  this will be fixed very soon! */
               if ((color & 0x00FC00) == 0x00FC00)
                    *dst++ = (0xFF000000 | (color & 0xFFF8FF));
               else
                    *dst++ = 0xFF000000 | color;
          }

          ++xpos;
          if (xpos == len) {
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

     if (LWZReadByte(fd,FALSE,c)>=0) {
          GIFERRORMSG("too much input data, ignoring extra...");
     }
     return image;
}



static __u32* ReadGIF( FILE *fd, int imageNumber,
                       int *width, int *height, int *transparency,
                       __u32 *key_rgb, int headeronly)
{
     __u8 buf[16];
     __u8 c;
     __u8 localColorMap[3][MAXCOLORMAPSIZE];
     int useGlobalColormap;
     int bitPixel;
     int imageCount = 0;
     char version[4];

     if (! ReadOK(fd,buf,6)) {
          GIFERRORMSG("error reading magic number" );
     }

     if (strncmp((char *)buf,"GIF",3) != 0) {
          GIFERRORMSG("not a GIF file" );
     }

     strncpy(version, (char *)buf + 3, 3);
     version[3] = '\0';

     if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0)) {
          GIFERRORMSG("bad version number, not '87a' or '89a'" );
     }

     if (! ReadOK(fd,buf,7)) {
          GIFERRORMSG("failed to read screen descriptor" );
     }

     GifScreen.Width           = LM_to_uint(buf[0],buf[1]);
     GifScreen.Height          = LM_to_uint(buf[2],buf[3]);
     GifScreen.BitPixel        = 2<<(buf[4]&0x07);
     GifScreen.ColorResolution = (((buf[4]&0x70)>>3)+1);
     GifScreen.Background      = buf[5];
     GifScreen.AspectRatio     = buf[6];

     if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
          if (ReadColorMap(fd,GifScreen.BitPixel,GifScreen.ColorMap)) {
               GIFERRORMSG("error reading global colormap" );
          }
     }

     if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49) {
          float r;
          r = ( (float) GifScreen.AspectRatio + 15.0 ) / 64.0;
          GIFERRORMSG("warning - non-square pixels");
     }

     Gif89.transparent = -1;
     Gif89.delayTime   = -1;
     Gif89.inputFlag   = -1;
     Gif89.disposal    = 0;

     for (;;) {
          if (! ReadOK(fd,&c,1)) {
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
               if (! ReadOK(fd,&c,1)) {
                    GIFERRORMSG("OF / read error on extention function code");
               }
               DoExtension(fd, c);
            continue;
          }

          if (c != ',') {         /* Not a valid start character */
               GIFERRORMSG("bogus character 0x%02x, ignoring", (int) c );
               continue;
          }

          ++imageCount;

          if (! ReadOK(fd,buf,9)) {
               GIFERRORMSG("couldn't read left/top/width/height");
          }

          *width = LM_to_uint(buf[4],buf[5]);
          *height = LM_to_uint(buf[6],buf[7]);
          *transparency = (Gif89.transparent != -1);

          if (headeronly && (!key_rgb || !*transparency))
               return NULL;

          useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);

          bitPixel = 1<<((buf[8]&0x07)+1);

          if (! useGlobalColormap) {
               if (ReadColorMap(fd, bitPixel, localColorMap)) {
                    GIFERRORMSG("error reading local colormap" );
               }

               if (key_rgb && *transparency)
                    *key_rgb =
                         (localColorMap[CM_RED][Gif89.transparent] << 16) |
                         (localColorMap[CM_GREEN][Gif89.transparent] << 8) |
                         (localColorMap[CM_BLUE][Gif89.transparent]);

               if (headeronly)
                    return NULL;

               return ReadImage( fd, LM_to_uint(buf[4],buf[5]),
                                 LM_to_uint(buf[6],buf[7]), localColorMap,
                                 BitSet(buf[8], INTERLACE),
                                 imageCount != imageNumber);
          }
          else {
               if (key_rgb && *transparency)
                    *key_rgb =
                         (GifScreen.ColorMap[CM_RED][Gif89.transparent] << 16) |
                         (GifScreen.ColorMap[CM_GREEN][Gif89.transparent] << 8)|
                         (GifScreen.ColorMap[CM_BLUE][Gif89.transparent]);

               if (headeronly)
                    return NULL;

               return ReadImage( fd, LM_to_uint(buf[4],buf[5]),
                                 LM_to_uint(buf[6],buf[7]), GifScreen.ColorMap,
                                 BitSet(buf[8], INTERLACE),
                                 imageCount != imageNumber);
          }
     }
}

