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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <png.h>
#include <string.h>

#include <malloc.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>


static DFBResult load_png_argb( FILE *f, __u8 *dst, int width, int height,
                                int pitch, DFBSurfacePixelFormat format );


/*
 * private data struct of IDirectFBImageProvider_PNG
 */
typedef struct {
     int            ref;      /* reference counter */
     char          *filename; /* filename of file to load */
} IDirectFBImageProvider_PNG_data;

static DFBResult
IDirectFBImageProvider_PNG_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNG_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination );

static DFBResult
IDirectFBImageProvider_PNG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_PNG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );


char *get_type()
{
     return "IDirectFBImageProvider";
}

char *get_implementation()
{
     return "PNG";
}

DFBResult Probe( const char *head )
{
     if (strncmp (head, "\211PNG\r\n\032\n", 8) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBImageProvider *thiz,
                     const char *filename )
{
     IDirectFBImageProvider_PNG_data *data;

     data = (IDirectFBImageProvider_PNG_data*)
          DFBCALLOC( 1, sizeof(IDirectFBImageProvider_PNG_data) );

     thiz->priv = data;

     data->ref = 1;
     data->filename = (char*)DFBMALLOC( strlen(filename)+1 );
     strcpy( data->filename, filename );

     DEBUGMSG( "DirectFB/Media: PNG Provider Construct '%s'\n", filename );

     thiz->AddRef = IDirectFBImageProvider_PNG_AddRef;
     thiz->Release = IDirectFBImageProvider_PNG_Release;
     thiz->RenderTo = IDirectFBImageProvider_PNG_RenderTo;
     thiz->GetImageDescription = IDirectFBImageProvider_PNG_GetImageDescription;
     thiz->GetSurfaceDescription =
                               IDirectFBImageProvider_PNG_GetSurfaceDescription;

     return DFB_OK;
}

static void IDirectFBImageProvider_PNG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;

     DFBFREE( data->filename );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
}

static DFBResult IDirectFBImageProvider_PNG_AddRef( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_PNG_Release( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     if (--data->ref == 0) {
          IDirectFBImageProvider_PNG_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_PNG_RenderTo(
                                               IDirectFBImageProvider *thiz,
                                               IDirectFBSurface *destination )
{
     int err, loader_result = 1;
     void *dst;
     int pitch, width, height;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps;

     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

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
          FILE *f;

          f = fopen( data->filename, "rb" );
          if (!f) {
               destination->Unlock( destination );
               switch (errno) {
                    case EACCES:
                         return DFB_ACCESSDENIED;
                    case EIO:
                         return DFB_IO;
                    case ENOENT:
                         return DFB_FILENOTFOUND;
                    default:
                         return DFB_FAILURE;
               }
          }

          err = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
          if (err) {
               fclose( f );
               return err;
          }

          loader_result = load_png_argb( f, dst, width, height, pitch, format );

          err = destination->Unlock( destination );

          fclose( f );
     }

     if (loader_result)
          return loader_result;

     return err;
}


/* Loading routines */

static DFBResult load_png_argb( FILE *f, __u8 *dst, int width, int height,
                                int pitch, DFBSurfacePixelFormat format )
{
     png_structp png_ptr;
     png_infop   info_ptr;
     png_uint_32 png_width, png_height;
     int         png_bpp, png_type, i;

     png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
     if (!png_ptr)
          return DFB_FAILURE;

     info_ptr = png_create_info_struct( png_ptr );
     if (!info_ptr) {
          png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
          return DFB_FAILURE;
     }

     png_init_io( png_ptr, f );
     png_read_info( png_ptr, info_ptr );

     png_get_IHDR( png_ptr, info_ptr, &png_width, &png_height, &png_bpp,
                   &png_type, NULL, NULL, NULL );

     if (png_type == PNG_COLOR_TYPE_PALETTE)
          png_set_palette_to_rgb( png_ptr );

     if (png_type == PNG_COLOR_TYPE_GRAY
         || png_type == PNG_COLOR_TYPE_GRAY_ALPHA)
          png_set_gray_to_rgb(png_ptr);

     if (png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ))
          png_set_tRNS_to_alpha( png_ptr );

     if (png_bpp == 16)
          png_set_strip_16( png_ptr );

#ifdef __BIG_ENDIAN__
     if (!(png_type & PNG_COLOR_MASK_ALPHA))
          png_set_filler( png_ptr, 0xFF, PNG_FILLER_BEFORE );

     png_set_swap_alpha( png_ptr );
#else
     if (!(png_type & PNG_COLOR_MASK_ALPHA))
          png_set_filler( png_ptr, 0xFF, PNG_FILLER_AFTER );

     png_set_bgr( png_ptr );
#endif

     if (width == png_width && height == png_height && format == DSPF_ARGB) {
          png_bytep bptrs[png_height];

          for (i=0; i<png_height; i++)
               bptrs[i] = dst + pitch * i;

          png_read_image( png_ptr, bptrs );
     }
     else {
          png_bytep bptrs[png_height];

          bptrs[0] = DFBMALLOC( png_height * png_width*4 );

          for (i=1; i<png_height; i++)
               bptrs[i] = bptrs[i-1] + png_width*4;

          png_read_image( png_ptr, bptrs );

          scale_linear_32( (__u32*)dst, (__u32*)bptrs[0], png_width,
                           png_height, width, height,
                           pitch-width*BYTES_PER_PIXEL(format), format );

          DFBFREE( bptrs[0] );
     }

     png_destroy_read_struct( &png_ptr, &info_ptr, NULL );

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_PNG_GetSurfaceDescription(
                                              IDirectFBImageProvider *thiz,
                                              DFBSurfaceDescription *dsc )
{
     FILE *f;

     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     f = fopen( data->filename, "rb" );
     if (!f)
          return errno2dfb( errno );

     {
          png_structp png_ptr;
          png_infop info_ptr;

          png_uint_32 png_width, png_height;
          int png_bpp, png_type;

          png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL,
                                            NULL, NULL );
          if (!png_ptr)
               return DFB_FAILURE;

          info_ptr = png_create_info_struct( png_ptr );
          if (!info_ptr) {
               png_destroy_read_struct( &png_ptr, (png_infopp)NULL,
                                        (png_infopp)NULL );
               return DFB_FAILURE;
          }

          png_init_io( png_ptr, f );
          png_read_info( png_ptr, info_ptr );

          png_get_IHDR( png_ptr, info_ptr, &png_width, &png_height, &png_bpp,
                        &png_type, NULL, NULL, NULL );

          dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc->width  = png_width;
          dsc->height = png_height;

          if (png_type & PNG_COLOR_MASK_ALPHA)
               dsc->pixelformat = DSPF_ARGB;
          else
               dsc->pixelformat= layers->shared->surface->format;

          png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
          fclose( f );
     }

     return DFB_OK;
}

static DFBResult IDirectFBImageProvider_PNG_GetImageDescription(
                                                   IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc )
{
     FILE *f;

     INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

     if (!dsc)
          return DFB_INVARG;

     f = fopen( data->filename, "rb" );
     if (!f)
          return errno2dfb( errno );

     /* FIXME: colorkeyed PNGs are currently converted to alphachannel PNGs */
     {
          png_structp png_ptr;
          png_infop info_ptr;

          int png_type;

          png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL,
                                            NULL, NULL );
          if (!png_ptr) {
               fclose( f );
               return DFB_FAILURE;
          }

          info_ptr = png_create_info_struct( png_ptr );
          if (!info_ptr) {
               png_destroy_read_struct( &png_ptr, (png_infopp)NULL,
                                        (png_infopp)NULL );
               fclose( f );
               return DFB_FAILURE;
          }

          png_init_io( png_ptr, f );
          png_read_info( png_ptr, info_ptr );

          png_get_IHDR( png_ptr, info_ptr, NULL, NULL, NULL,
                        &png_type, NULL, NULL, NULL );

          if (png_type & PNG_COLOR_MASK_ALPHA)
               dsc->caps = DICAPS_ALPHACHANNEL;
          else
               dsc->caps = DICAPS_NONE;

          png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
          fclose( f );
     }

     return DFB_OK;
}

