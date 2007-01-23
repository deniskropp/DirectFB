/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <misc/util.h>

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, GIF )


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

/*
 * private data struct of IDirectFBImageProvider_GIF
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBDataBuffer *buffer;

     u32                 *image;
     int                  image_width;
     int                  image_height;
     bool                 image_transparency;
     u32                  image_colorkey;

     unsigned int  Width;
     unsigned int  Height;
     u8            ColorMap[3][MAXCOLORMAPSIZE];
     unsigned int  BitPixel;
     unsigned int  ColorResolution;
     u32           Background;
     unsigned int  AspectRatio;


     int GrayScale;
     int transparent;
     int delayTime;
     int inputFlag;
     int disposal;


     u8 buf[280];
     int curbit, lastbit, done, last_byte;


     int fresh;
     int code_size, set_code_size;
     int max_code, max_code_size;
     int firstcode, oldcode;
     int clear_code, end_code;
     int table[2][(1<< MAX_LWZ_BITS)];
     int stack[(1<<(MAX_LWZ_BITS))*2], *sp;

     DIRenderCallback  render_callback;
     void             *render_callback_ctx;

     CoreDFB *core;
} IDirectFBImageProvider_GIF_data;

static bool verbose       = false;
static bool showComment   = false;
static bool ZeroDataBlock = false;

static u32* ReadGIF( IDirectFBImageProvider_GIF_data *data, int imageNumber,
                       int *width, int *height, bool *transparency,
                       u32 *key_rgb, bool alpha, bool headeronly);

static bool ReadOK( IDirectFBDataBuffer *buffer, void *data, unsigned int len );


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
     if (strncmp ((char*) ctx->header, "GIF8", 4) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_GIF)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );
     
     data->ref = 1;

     data->GrayScale   = -1;
     data->transparent = -1;
     data->delayTime   = -1;

     data->core = core;
     data->buffer = buffer;
     buffer->AddRef( buffer );

     data->image = ReadGIF( data, 1, &data->image_width, &data->image_height,
                            &data->image_transparency, &data->image_colorkey,
                            true, false );

     buffer->Release( buffer );
     data->buffer = NULL;
     
     if (!data->image) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

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

     if (data->image)
          D_FREE( data->image );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_GIF_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_GIF)

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
     DFBRegion              clip;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     int err;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          rect = *dest_rect; 
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else {
          rect = dst_data->area.wanted;
     }

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;

     /* actual loading and rendering */
     if (dfb_rectangle_region_intersects( &rect, &clip )) {
          void  *dst;
          int    pitch;

          err = dfb_surface_soft_lock( data->core, dst_surface, DSLF_WRITE, &dst, &pitch, 0 );
          if (err)
               return err;

          dfb_scale_linear_32( data->image, data->image_width, data->image_height,
                               dst, pitch, &rect, dst_surface, &clip );

          dfb_surface_unlock( dst_surface, 0 );

          if (data->render_callback) {
               DIRenderCallbackResult r;

               rect.x = 0;
               rect.y = 0;
               rect.w = data->image_width;
               rect.h = data->image_height;

               r = data->render_callback( &rect, data->render_callback_ctx );

               if (r != DIRCR_OK)
                       return DFB_INTERRUPTED;
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     data->render_callback     = callback;
     data->render_callback_ctx = context;
     
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     dsc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width       = data->image_width;
     dsc->height      = data->image_height;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_GIF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_GIF)

     if (data->image_transparency) {
          dsc->caps = DICAPS_COLORKEY;

          dsc->colorkey_r = (data->image_colorkey & 0xff0000) >> 16;
          dsc->colorkey_g = (data->image_colorkey & 0x00ff00) >>  8;
          dsc->colorkey_b = (data->image_colorkey & 0x0000ff);
     }
     else
          dsc->caps = DICAPS_NONE;

     return DFB_OK;
}


/**********************************
         GIF Loader Code
 **********************************/

static int ReadColorMap( IDirectFBDataBuffer *buffer, int number,
                         u8 buf[3][MAXCOLORMAPSIZE] )
{
     int     i;
     u8 rgb[3];

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

static int GetDataBlock(IDirectFBDataBuffer *buffer, u8 *buf)
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

static int GetCode(IDirectFBImageProvider_GIF_data *data, int code_size, int flag)
{
     int i, j, ret;
     unsigned char count;

     if (flag) {
          data->curbit = 0;
          data->lastbit = 0;
          data->done = false;
          return 0;
     }

     if ( (data->curbit+code_size) >= data->lastbit) {
          if (data->done) {
               if (data->curbit >= data->lastbit) {
                    GIFERRORMSG("ran off the end of my bits" );
               }
             return -1;
          }
          data->buf[0] = data->buf[data->last_byte-2];
          data->buf[1] = data->buf[data->last_byte-1];

          if ((count = GetDataBlock( data->buffer, &data->buf[2] )) == 0) {
               data->done = true;
          }

          data->last_byte = 2 + count;
          data->curbit = (data->curbit - data->lastbit) + 16;
          data->lastbit = (2+count) * 8;
     }

     ret = 0;
     for (i = data->curbit, j = 0; j < code_size; ++i, ++j) {
          ret |= ((data->buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
     }
     data->curbit += code_size;

     return ret;
}

static int DoExtension( IDirectFBImageProvider_GIF_data *data, int label )
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
               while (GetDataBlock( data->buffer, (u8*) buf ) != 0) {
                    if (showComment)
                         GIFERRORMSG("gif comment: %s", buf );
                    }
               return false;
          case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock( data->buffer, (u8*) buf );
               data->disposal    = (buf[0] >> 2) & 0x7;
               data->inputFlag   = (buf[0] >> 1) & 0x1;
               data->delayTime   = LM_to_uint( buf[1], buf[2] );
               if ((buf[0] & 0x1) != 0) {
                    data->transparent = buf[3];
               }
               while (GetDataBlock( data->buffer, (u8*) buf ) != 0)
                    ;
               return false;
          default:
               str = (char*) buf;
               snprintf(str, 256, "UNKNOWN (0x%02x)", label);
          break;
     }

     if (verbose)
          GIFERRORMSG("got a '%s' extension", str );

     while (GetDataBlock( data->buffer, (u8*) buf ) != 0)
          ;

     return false;
}

static int LWZReadByte( IDirectFBImageProvider_GIF_data *data, int flag, int input_code_size )
{
     int code, incode;
     int i;

     if (flag) {
          data->set_code_size = input_code_size;
          data->code_size = data->set_code_size+1;
          data->clear_code = 1 << data->set_code_size ;
          data->end_code = data->clear_code + 1;
          data->max_code_size = 2*data->clear_code;
          data->max_code = data->clear_code+2;

          GetCode(data, 0, true);

          data->fresh = true;

          for (i = 0; i < data->clear_code; ++i) {
               data->table[0][i] = 0;
               data->table[1][i] = i;
          }
          for (; i < (1<<MAX_LWZ_BITS); ++i) {
               data->table[0][i] = data->table[1][0] = 0;
          }
          data->sp = data->stack;

          return 0;
     }
     else if (data->fresh) {
          data->fresh = false;
          do {
               data->firstcode = data->oldcode = GetCode( data, data->code_size, false );
          } while (data->firstcode == data->clear_code);

          return data->firstcode;
     }

     if (data->sp > data->stack) {
          return *--data->sp;
     }

     while ((code = GetCode( data, data->code_size, false )) >= 0) {
          if (code == data->clear_code) {
               for (i = 0; i < data->clear_code; ++i) {
                    data->table[0][i] = 0;
                    data->table[1][i] = i;
               }
               for (; i < (1<<MAX_LWZ_BITS); ++i) {
                    data->table[0][i] = data->table[1][i] = 0;
               }
               data->code_size = data->set_code_size+1;
               data->max_code_size = 2*data->clear_code;
               data->max_code = data->clear_code+2;
               data->sp = data->stack;
               data->firstcode = data->oldcode = GetCode( data, data->code_size, false );

               return data->firstcode;
          }
          else if (code == data->end_code) {
               int count;
               u8 buf[260];

               if (ZeroDataBlock) {
                    return -2;
               }

               while ((count = GetDataBlock( data->buffer, buf )) > 0)
                    ;

               if (count != 0)
                    GIFERRORMSG("missing EOD in data stream "
                                "(common occurence)");

               return -2;
          }

          incode = code;

          if (code >= data->max_code) {
               *data->sp++ = data->firstcode;
               code = data->oldcode;
          }

          while (code >= data->clear_code) {
               *data->sp++ = data->table[1][code];
               if (code == data->table[0][code]) {
                    GIFERRORMSG("circular table entry BIG ERROR");
               }
               code = data->table[0][code];
          }

          *data->sp++ = data->firstcode = data->table[1][code];

          if ((code = data->max_code) <(1<<MAX_LWZ_BITS)) {
               data->table[0][code] = data->oldcode;
               data->table[1][code] = data->firstcode;
               ++data->max_code;
               if ((data->max_code >= data->max_code_size)
                   && (data->max_code_size < (1<<MAX_LWZ_BITS)))
               {
                    data->max_code_size *= 2;
                    ++data->code_size;
               }
          }

          data->oldcode = incode;

          if (data->sp > data->stack) {
               return *--data->sp;
          }
     }
     return code;
}

static int SortColors (const void *a, const void *b)
{
     return (*((const u8 *) a) - *((const u8 *) b));
}

/*  looks for a color that is not in the colormap and ideally not
    even close to the colors used in the colormap  */
static u32 FindColorKey( int n_colors, u8 cmap[3][MAXCOLORMAPSIZE] )
{
     u32   color = 0xFF000000;
     u8    csort[MAXCOLORMAPSIZE];
     int   i, j, index, d;

     if (n_colors < 1)
          return color;

     D_ASSERT( n_colors <= MAXCOLORMAPSIZE );

     for (i = 0; i < 3; i++) {
          direct_memcpy( csort, cmap[i], n_colors );
          qsort( csort, n_colors, 1, SortColors );

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

static u32* ReadImage( IDirectFBImageProvider_GIF_data *data, int width, int height,
                       u8 cmap[3][MAXCOLORMAPSIZE], u32 key_rgb,
                       bool interlace, bool ignore )
{
     u8 c;
     int v;
     int xpos = 0, ypos = 0, pass = 0;
     u32 *image;

     /*
     **  Initialize the decompression routines
     */
     if (! ReadOK( data->buffer, &c, 1 ))
          GIFERRORMSG("EOF / read error on image data" );

     if (LWZReadByte( data, true, c ) < 0)
          GIFERRORMSG("error reading image" );

     /*
     **  If this is an "uninteresting picture" ignore it.
     */
     if (ignore) {
          if (verbose)
               GIFERRORMSG("skipping image..." );

          while (LWZReadByte( data, false, c ) >= 0)
               ;
          return NULL;
     }
     
     // FIXME: allocates four additional bytes because the scaling functions
     //        in src/misc/gfx_util.c have an off-by-one bug which causes
     //        segfaults on darwin/osx (not on linux)           
     if ((image = D_MALLOC(width * height * 4 + 4)) == NULL) {
          GIFERRORMSG("couldn't alloc space for image" );
     }

     if (verbose) {
          GIFERRORMSG("reading %d by %d%s GIF image", width, height,
                      interlace ? " interlaced" : "" );
     }

     while ((v = LWZReadByte( data, false, c )) >= 0 ) {
          u32 *dst = image + (ypos * width + xpos);

          if (v == data->transparent) {
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

     if (LWZReadByte( data, false, c ) >= 0) {
          GIFERRORMSG("too much input data, ignoring extra...");
     }
     return image;
}


static u32* ReadGIF( IDirectFBImageProvider_GIF_data *data, int imageNumber,
                     int *width, int *height, bool *transparency,
                     u32 *key_rgb, bool alpha, bool headeronly)
{
     u8    buf[16];
     u8    c;
     u8    localColorMap[3][MAXCOLORMAPSIZE];
     u32   colorKey = 0;
     bool  useGlobalColormap;
     int   bitPixel;
     int   imageCount = 0;
     char  version[4];

     if (! ReadOK( data->buffer, buf, 6 )) {
          GIFERRORMSG("error reading magic number" );
     }

     if (strncmp( (char *)buf, "GIF", 3 ) != 0) {
          GIFERRORMSG("not a GIF file" );
     }

     direct_snputs( version, (char *)buf + 3, 4 );

     if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0)) {
          GIFERRORMSG("bad version number, not '87a' or '89a'" );
     }

     if (! ReadOK(data->buffer,buf,7)) {
          GIFERRORMSG("failed to read screen descriptor" );
     }

     data->Width           = LM_to_uint( buf[0], buf[1] );
     data->Height          = LM_to_uint( buf[2], buf[3] );
     data->BitPixel        = 2 << (buf[4] & 0x07);
     data->ColorResolution = (((buf[4] & 0x70) >> 3) + 1);
     data->Background      = buf[5];
     data->AspectRatio     = buf[6];

     if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
          if (ReadColorMap( data->buffer, data->BitPixel, data->ColorMap )) {
               GIFERRORMSG("error reading global colormap" );
          }
     }

     if (data->AspectRatio != 0 && data->AspectRatio != 49) {
          /* float r = ( (float) data->AspectRatio + 15.0 ) / 64.0; */
          GIFERRORMSG("warning - non-square pixels");
     }

     data->transparent = -1;
     data->delayTime   = -1;
     data->inputFlag   = -1;
     data->disposal    = 0;

     for (;;) {
          if (! ReadOK( data->buffer, &c, 1)) {
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
               if (! ReadOK( data->buffer, &c, 1)) {
                    GIFERRORMSG("OF / read error on extention function code");
               }
               DoExtension( data, c );
            continue;
          }

          if (c != ',') {         /* Not a valid start character */
               GIFERRORMSG("bogus character 0x%02x, ignoring", (int) c );
               continue;
          }

          ++imageCount;

          if (! ReadOK( data->buffer, buf, 9 )) {
               GIFERRORMSG("couldn't read left/top/width/height");
          }

          *width  = LM_to_uint( buf[4], buf[5] );
          *height = LM_to_uint( buf[6], buf[7] );
          *transparency = (data->transparent != -1);

          if (headeronly && !(*transparency && key_rgb))
               return NULL;

          useGlobalColormap = ! BitSet( buf[8], LOCALCOLORMAP );

          if (useGlobalColormap) {
               if (*transparency && (key_rgb || !headeronly))
                    colorKey = FindColorKey( data->BitPixel,
                                             data->ColorMap );
          }
          else {
               bitPixel = 2 << (buf[8] & 0x07);
               if (ReadColorMap( data->buffer, bitPixel, localColorMap ))
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

          return ReadImage( data, *width, *height,
                            (useGlobalColormap ?
                             data->ColorMap : localColorMap), colorKey,
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

