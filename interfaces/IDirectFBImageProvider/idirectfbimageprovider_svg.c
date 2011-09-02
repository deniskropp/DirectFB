/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_ZLIB
# include <zlib.h>
#endif

#include <directfb.h>

#include <idirectfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>

#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/mem.h>

#include <misc/util.h>

#include <cairo.h>
#include <cairo-features.h>
/*
 * Disable cairo DirectFB backend until it becomes stable enough.
 */
#undef CAIRO_HAS_DIRECTFB_SURFACE

#ifdef CAIRO_HAS_DIRECTFB_SURFACE
# include <cairo-directfb.h>
#endif

#include <svg-cairo.h>


static DFBResult 
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, SVG )

typedef struct {
     IDirectFBImageProvider_data   base;

     svg_cairo_t      *svg_cairo;

     int               width;
     int               height;
     
     DIRenderCallback  render_callback;
     void             *render_callback_ctx;
} IDirectFBImageProvider_SVG_data;


static DFBResult
svgstatus2result( svg_cairo_status_t status )
{
     switch (status) {
          case SVG_CAIRO_STATUS_SUCCESS:
               return DFB_OK;
          case SVG_CAIRO_STATUS_NO_MEMORY:
               return DFB_NOSYSTEMMEMORY;
          case SVG_CAIRO_STATUS_IO_ERROR:
               return DFB_IO;
          case SVG_CAIRO_STATUS_FILE_NOT_FOUND:
               return DFB_FILENOTFOUND;
          case SVG_CAIRO_STATUS_INVALID_VALUE:
               return DFB_INVARG;
          case SVG_CAIRO_STATUS_INVALID_CALL:
               return DFB_UNSUPPORTED;
          case SVG_CAIRO_STATUS_PARSE_ERROR:
               return DFB_FAILURE;
          default:
               break;
     }

     return DFB_FAILURE;
}


#ifdef USE_ZLIB
static DFBResult
check_gzip_header( IDirectFBDataBuffer *source )
{
     int i, flags;
     
     /* Check/Skip gzip header. */

#define GETC( buffer ) ({\
     DFBResult     ret;\
     unsigned char byte;\
     ret = buffer->WaitForData( buffer, 1 );\
     if (ret)\
          return ret;\
     ret = buffer->GetData( buffer, 1, &byte, NULL );\
     if (ret)\
          return ret;\
     byte;\
}) 
     
     /* Magic header. */
     if (GETC(source) != 0x1f || GETC(source) != 0x8b)
          return DFB_UNSUPPORTED;
    
     /* Compression method. */     
     if (GETC(source) != 8)
          return DFB_UNSUPPORTED;

     /* Flags. */     
     flags = GETC(source);
     
     /* Modification timestamp + Extra flags + OS type. */
     for (i = 0; i < 6; i++)
          GETC(source);
     
     /* Optional part number. */
     if (flags & (1 << 1)) {
          GETC(source);
          GETC(source);
     }
     
     /* Optional extra field. */
     if (flags & (1 << 2)) {
          int len = GETC(source) | (GETC(source) << 8);
          while (len--)
               GETC(source);
     }
     
     /* Optional original filename. */
     if (flags & (1 << 3)) {
          while (GETC(source) != '\0');
     }
     
     /* Optional file comment. */
     if (flags & (1 << 4)) {
          while (GETC(source) != '\0');
     }
     
     /* Optional encryption header. */
     if (flags & (1 << 5)) {
          for (i = 0; i < 12; i++)
               GETC(source);
     }
 
#undef GETC
               
     return DFB_OK;
}
#endif /* USE_ZLIB */


static void
IDirectFBImageProvider_SVG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_SVG_data *data = thiz->priv;

     if (data->svg_cairo)
          svg_cairo_destroy( data->svg_cairo );

}


#ifdef CAIRO_HAS_DIRECTFB_SURFACE

static DFBResult
IDirectFBImageProvider_SVG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *dest,
                                     const DFBRectangle     *dest_rect )
{
     DFBResult         ret;
     DFBRectangle      rect;
     IDirectFBSurface *tmp;
     cairo_t          *cairo;
     cairo_surface_t  *cairo_surface;
          
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_SVG );
     
     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;

     rect.x = 0;
     rect.y = 0;
     dest->GetSize( dest, &rect.w, &rect.h );

     if (dest_rect && !dfb_rectangle_intersect( &rect, dest_rect ))
          return DFB_OK;

     ret = dest->GetSubSurface( dest, &rect, &tmp );
     if (ret)
          return ret;
     
     tmp->Clear( tmp, 0x00, 0x00, 0x00, 0x00 );

     cairo_surface = cairo_directfb_surface_create( idirectfb_singleton , tmp );
     if (!cairo_surface) {
          tmp->Release( tmp );
          return DFB_FAILURE;
     }
     
     cairo = cairo_create( cairo_surface );
     if (!cairo) {
          cairo_surface_destroy( cairo_surface );
          tmp->Release( tmp );
          return DFB_FAILURE;
     }
     
     if (data->width != rect.w || data->height != rect.h) {
          cairo_scale( cairo, (double)rect.w / (double)data->width,
                              (double)rect.h / (double)data->height );
     }

     ret = svgstatus2result( svg_cairo_render( data->svg_cairo, cairo ) );

     if (data->render_callback && ret == DFB_OK) {
          rect.x = 0;
          rect.y = 0;
          rect.w = data->width;
          rect.h = data->height;
          
          data->render_callback( &rect, data->render_callback_ctx );
     }
     
     cairo_destroy( cairo  );
     cairo_surface_destroy( cairo_surface );

     tmp->Release( tmp );
     
     return ret;
}

#else /* !CAIRO_HAS_DIRECTFB_SURFACE */

static DFBResult
IDirectFBImageProvider_SVG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *dest,
                                     const DFBRectangle     *dest_rect )
{
     DFBResult               ret;
     DFBRectangle            rect;
     IDirectFBSurface       *tmp;
     DFBSurfacePixelFormat   format;
     cairo_t                *cairo;
     cairo_surface_t        *cairo_surface;  
     cairo_format_t          cairo_format;
     void                   *ptr;
     int                     pitch;
     bool                    need_conversion = false;
          
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_SVG );

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;

     rect.x = 0;
     rect.y = 0;
     dest->GetSize( dest, &rect.w, &rect.h );

     if (dest_rect && !dfb_rectangle_intersect( &rect, dest_rect ))
          return DFB_OK;

     dest->GetPixelFormat( dest, &format );
     switch (format) {
          case DSPF_A1:
               cairo_format = CAIRO_FORMAT_A1;
               break;
          case DSPF_A8:
               cairo_format = CAIRO_FORMAT_A8;
               break;
          case DSPF_RGB32:
               cairo_format = CAIRO_FORMAT_RGB24;
               break;
          case DSPF_ARGB:
               cairo_format = CAIRO_FORMAT_ARGB32;
               break;
          default:
               cairo_format = CAIRO_FORMAT_ARGB32;
               need_conversion = true;
               break;
     }

     if (need_conversion) {
          DFBSurfaceDescription dsc;

          dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc.width       = rect.w;
          dsc.height      = rect.h;
          dsc.pixelformat = DSPF_ARGB;

          ret = idirectfb_singleton->CreateSurface( idirectfb_singleton, &dsc, &tmp );
     }
     else {
          ret = dest->GetSubSurface( dest, &rect, &tmp );
     }

     if (ret)
          return ret;

     tmp->Clear( tmp, 0x00, 0x00, 0x00, 0x00 );
     
     ret = tmp->Lock( tmp, DSLF_READ | DSLF_WRITE, &ptr, &pitch );
     if (ret) {
          tmp->Release( tmp );
          return ret;
     }
     
     cairo_surface = 
          cairo_image_surface_create_for_data( ptr, cairo_format,
                                               rect.w, rect.h, pitch );
     if (!cairo_surface) {
          tmp->Unlock( tmp );
          tmp->Release( tmp );
          return DFB_FAILURE;
     }
     
     cairo = cairo_create( cairo_surface );
     if (!cairo) {
          cairo_surface_destroy( cairo_surface );
          tmp->Unlock( tmp );
          tmp->Release( tmp );
          return DFB_FAILURE;
     }
     
     if (data->width != rect.w || data->height != rect.h) {
          cairo_scale( cairo, (double)rect.w / (double)data->width,
                              (double)rect.h / (double)data->height );
     }

     ret = svgstatus2result( svg_cairo_render( data->svg_cairo, cairo ) );
     
     tmp->Unlock( tmp );
     
     if (need_conversion && ret == DFB_OK)
          ret = dest->Blit( dest, tmp, NULL, rect.x, rect.y );

     if (data->render_callback && ret == DFB_OK) {
          rect.x = 0;
          rect.y = 0;
          rect.w = data->width;
          rect.h = data->height;
          
          data->render_callback( &rect, data->render_callback_ctx );
     }
     
     cairo_destroy( cairo  );
     cairo_surface_destroy( cairo_surface );
     
     tmp->Release( tmp );
     
     return ret;
}

#endif /* !CAIRO_HAS_DIRECTFB_SURFACE */

static DFBResult
IDirectFBImageProvider_SVG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_SVG );

     data->render_callback     = callback;
     data->render_callback_ctx = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SVG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_SVG );

     if (!desc)
          return DFB_INVARG;

     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = DSPF_ARGB;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SVG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_SVG );

     if (!desc)
          return DFB_INVARG;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     int i;
     
     if (ctx->filename) {
          char *ext = strrchr( ctx->filename, '.' );
          
          if (ext) {
               if (!strcasecmp( ext, ".svg" ))
                    return DFB_OK;
#ifdef USE_ZLIB
               if (!strcasecmp( ext, ".svgz" ))
                    return DFB_OK;
#endif
          }
     }
     
     for (i = 0; i < sizeof(ctx->header)-5; i++) {
          if (!memcmp( &ctx->header[i], "<?xml", 5))
               return DFB_OK;
     }
     
     return DFB_UNSUPPORTED;
}
     
static DFBResult
Construct( IDirectFBImageProvider *thiz, IDirectFBDataBuffer *buffer )
{
     DFBResult           ret;
     svg_cairo_status_t  status;
#ifdef USE_ZLIB
     z_stream           *z = NULL;
#endif
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_SVG );

     data->base.ref = 1;
 
     status = svg_cairo_create( &data->svg_cairo );
     if (status != SVG_CAIRO_STATUS_SUCCESS) {
          ret = svgstatus2result( status );
          D_ERROR( "IDirectFBImageProvider_SVG: "
                   "svg_cairo_create() -> %s\n", DirectFBErrorString(ret) );
          IDirectFBImageProvider_SVG_Destruct( thiz );
          return ret;
     }

     status = svg_cairo_parse_chunk_begin( data->svg_cairo );
     if (status != SVG_CAIRO_STATUS_SUCCESS) {
          ret = svgstatus2result( status );
          D_ERROR( "IDirectFBImageProvider_SVG: "
                   "svg_cairo_parse_chunk_begin() -> %s\n",
                   DirectFBErrorString(ret) );
          IDirectFBImageProvider_SVG_Destruct( thiz );
          return ret;
     }
     
     buffer->AddRef( buffer );
     
#ifdef USE_ZLIB
     if (((IDirectFBDataBuffer_data*)buffer->priv)->filename) {
          char *filename = ((IDirectFBDataBuffer_data*)buffer->priv)->filename;
          
          if (!strcasecmp( strrchr( filename, '.' ) ? : "", ".svgz" )) {
               ret = check_gzip_header( buffer );
               if (ret) {
                    D_ERROR( "IDirectFBImageProvider_SVG: Invalid GZIP header!\n" );
                    IDirectFBImageProvider_SVG_Destruct( thiz );
                    buffer->Release( buffer );
                    return ret;
               }
                    
               z = alloca( sizeof(z_stream) );
               memset( z, 0, sizeof(z_stream) );
               inflateInit2( z, -MAX_WBITS );
          }
     }
#endif

     while (1) {
          unsigned char in[1024];
          unsigned int  len = 0;
              
          buffer->WaitForData( buffer, sizeof(in) );
          
          ret = buffer->GetData( buffer, sizeof(in), in, &len );
          if (ret) {
               if (ret == DFB_EOF)
                    break;
               IDirectFBImageProvider_SVG_Destruct( thiz );
               buffer->Release( buffer );
               return ret;
          }
          
          if (len) {
#ifdef USE_ZLIB
               if (z) {
                    unsigned char out[4096];
                    
                    z->next_in  = &in[0];
                    z->avail_in = len;
                    
                    do {
                         z->next_out  = &out[0];
                         z->avail_out = sizeof(out);
                         ret = inflate( z, Z_SYNC_FLUSH );
                         if (!ret) {                              
                              status = svg_cairo_parse_chunk( data->svg_cairo, 
                                                              out, sizeof(out)-z->avail_out );
                         }
                    }
                    while (ret == 0 && status == SVG_CAIRO_STATUS_SUCCESS);
               }
               else 
#endif
                    status = svg_cairo_parse_chunk( data->svg_cairo, in, len );
               
               if (status != SVG_CAIRO_STATUS_SUCCESS) {
                    ret = svgstatus2result( status );
                    D_ERROR( "IDirectFBImageProvider_SVG: "
                             "svg_cairo_parse_chunk() -> %s\n", 
                             DirectFBErrorString(ret) );
                    IDirectFBImageProvider_SVG_Destruct( thiz );
                    buffer->Release( buffer );
                    return ret;
               }
          }
     }

     buffer->Release( buffer );

#ifdef USE_ZLIB
     if (z)
          inflateEnd( z );
#endif

     status = svg_cairo_parse_chunk_end( data->svg_cairo );
     if (status != SVG_CAIRO_STATUS_SUCCESS) {
          ret = svgstatus2result( status );
          D_ERROR( "IDirectFBImageProvider_SVG: "
                   "svg_cairo_parse_chunk_end() -> %s\n", 
                   DirectFBErrorString(ret) );
          IDirectFBImageProvider_SVG_Destruct( thiz );
          return ret;
     }

     svg_cairo_get_size( data->svg_cairo, &data->width, &data->height );
     if (data->width < 1)
          data->width = 200;
     if (data->height < 1)
          data->height = 200;

     data->base.Destruct = IDirectFBImageProvider_SVG_Destruct;

     thiz->RenderTo              = IDirectFBImageProvider_SVG_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_SVG_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_SVG_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_SVG_GetSurfaceDescription;

     return DFB_OK;
}

