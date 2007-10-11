/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   All rights reserved.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <idirectfb.h>

#include <display/idirectfbsurface.h>

#include <misc/gfx_util.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>

#include <dfiff.h>

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, DFIFF )


/*
 * private data struct of IDirectFBImageProvider_DFIFF
 */
typedef struct {
     int                  ref;     /* reference counter */

     void                *ptr;     /* pointer to raw file data (mapped) */
     int                  len;     /* data length, i.e. file size */

     DIRenderCallback     render_callback;
     void                *render_callback_context;

     CoreDFB             *core;
} IDirectFBImageProvider_DFIFF_data;





static void
IDirectFBImageProvider_DFIFF_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_DFIFF_data *data = thiz->priv;

     munmap( data->ptr, data->len );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_DFIFF_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_DFIFF)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_DFIFF)

     if (--data->ref == 0) {
          IDirectFBImageProvider_DFIFF_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_RenderTo( IDirectFBImageProvider *thiz,
                                       IDirectFBSurface       *destination,
                                       const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     const DFIFFHeader     *header;
     DFBRectangle           rect;
     DFBRectangle           clipped;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_DFIFF)

     if (!destination)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM (destination, dst_data, IDirectFBSurface);

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DEAD;

     if (dest_rect) {
          rect.x = dest_rect->x + dst_data->area.wanted.x;
          rect.y = dest_rect->y + dst_data->area.wanted.y;
          rect.w = dest_rect->w;
          rect.h = dest_rect->h;
     }
     else
          rect = dst_data->area.wanted;

     if (rect.w < 1 || rect.h < 1)
          return DFB_INVAREA;

     clipped = rect;

     if (!dfb_rectangle_intersect( &clipped, &dst_data->area.current ))
          return DFB_INVAREA;

     header = data->ptr;

     if (DFB_RECTANGLE_EQUAL( rect, clipped ) &&
         rect.w == header->width && rect.h == header->height &&
         dst_surface->config.format == header->format)
     {
          ret = dfb_surface_write_buffer( dst_surface, CSBR_BACK,
                                          data->ptr + sizeof(DFIFFHeader), header->pitch, &rect );
          if (ret)
               return ret;
     }
     else {
          IDirectFBSurface      *source;
          DFBSurfaceDescription  desc;
          DFBSurfaceCapabilities caps;
          DFBRegion              clip = DFB_REGION_INIT_FROM_RECTANGLE( &clipped );
          DFBRegion              old_clip;

          thiz->GetSurfaceDescription( thiz, &desc );

          desc.flags |= DSDESC_PREALLOCATED;   
          desc.preallocated[0].data  = data->ptr + sizeof(DFIFFHeader);
          desc.preallocated[0].pitch = header->pitch;

          ret = idirectfb_singleton->CreateSurface( idirectfb_singleton, &desc, &source );
          if (ret)
               return ret;

          destination->GetCapabilities( destination, &caps );

          if (caps & DSCAPS_PREMULTIPLIED && DFB_PIXELFORMAT_HAS_ALPHA(desc.pixelformat))
               destination->SetBlittingFlags( destination, DSBLIT_SRC_PREMULTIPLY );
          else
               destination->SetBlittingFlags( destination, DSBLIT_NOFX );

          destination->GetClip( destination, &old_clip );
          destination->SetClip( destination, &clip );

          destination->StretchBlit( destination, source, NULL, &rect );

          destination->SetClip( destination, &old_clip );

          destination->SetBlittingFlags( destination, DSBLIT_NOFX );

          destination->ReleaseSource( destination );

          source->Release( source );
     }
     
     if (data->render_callback) {
          DFBRectangle rect = { 0, 0, clipped.w, clipped.h };
          data->render_callback( &rect, data->render_callback_context );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                DIRenderCallback        callback,
                                                void                   *context )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_DFIFF)

     data->render_callback         = callback;
     data->render_callback_context = context;

     return DFB_OK;
}

/* Loading routines */

static DFBResult
IDirectFBImageProvider_DFIFF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                    DFBSurfaceDescription *dsc )
{
     const DFIFFHeader *header;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_DFIFF)

     header = data->ptr;

     dsc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width       = header->width;
     dsc->height      = header->height;
     dsc->pixelformat = header->format;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                  DFBImageDescription    *desc )
{
     const DFIFFHeader *header;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_DFIFF)

     if (!desc)
          return DFB_INVARG;

     header = data->ptr;

     desc->caps = DICAPS_NONE;

     if (DFB_PIXELFORMAT_HAS_ALPHA( header->format ))
          desc->caps |= DICAPS_ALPHACHANNEL;

     return DFB_OK;
}



static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (!strncmp( (const char*) ctx->header, "DFIFF", 5 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult                 ret;
     struct stat               stat;
     void                     *ptr;
     int                       fd = -1;
     IDirectFBDataBuffer_data *buffer_data;

     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_DFIFF)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     D_MAGIC_ASSERT( (IAny*) buffer, DirectInterface );

     /* Get the buffer's private data. */
     buffer_data = buffer->priv;
     if (!buffer_data) {
          ret = DFB_DEAD;
          goto error;
     }

     /* Check for valid filename. */
     if (!buffer_data->filename) {
          ret = DFB_UNSUPPORTED;
          goto error;
     }

     /* Open the file. */
     fd = open( buffer_data->filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "ImageProvider/DFIFF: Failure during open() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     /* Query file size etc. */
     if (fstat( fd, &stat ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "ImageProvider/DFIFF: Failure during fstat() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     /* Memory map the file. */
     ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (ptr == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "ImageProvider/DFIFF: Failure during mmap() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     /* Already close, we still have the map. */
     close( fd );

     data->ref = 1;
     data->ptr = ptr;
     data->len = stat.st_size;
     data->core = core;

     thiz->AddRef                = IDirectFBImageProvider_DFIFF_AddRef;
     thiz->Release               = IDirectFBImageProvider_DFIFF_Release;
     thiz->RenderTo              = IDirectFBImageProvider_DFIFF_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_DFIFF_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_DFIFF_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_DFIFF_GetSurfaceDescription;

     return DFB_OK;

error:
     if (fd != -1)
          close( fd );

     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

