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

#include <directfb.h>
#include <core/coredefs.h>
#include <core/layers.h>
#include <misc/gfx_util.h>


DFBResult load_png_argb( FILE *f, __u8 *dst, int width, int height,
                         int pitch, int bpp );


/*
 * private data struct of IDirectFBImageProvider_PNG
 */
typedef struct {
     int            ref;      /* reference counter */
     char          *filename; /* filename of file to load */
} IDirectFBImageProvider_PNG_data;

/*
 * increments reference count of input buffer
 */
DFBResult IDirectFBImageProvider_PNG_AddRef( IDirectFBImageProvider *thiz );

/*
 * decrements reference count, destructs interface data if reference count is 0
 */
DFBResult IDirectFBImageProvider_PNG_Release( IDirectFBImageProvider *thiz );

/*
 * Render the file contents into the destination contents
 * doing automatic scaling and color format conversion.
 */
DFBResult IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                               IDirectFBSurface *destination );

/*
 * Get a surface description that best matches the image
 * contained in the file. For opaque image formats the
 * pixel format of the primary layer is used.
 */
DFBResult IDirectFBImageProvider_PNG_GetSurfaceDescription(
                                               IDirectFBImageProvider *thiz,
                                               DFBSurfaceDescription *dsc );


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
                              malloc( sizeof(IDirectFBImageProvider_PNG_data) );
     memset( data, 0, sizeof(IDirectFBImageProvider_PNG_data) );
     thiz->priv = data;

     data->ref = 1;
     data->filename = (char*)malloc( strlen(filename)+1 );
     strcpy( data->filename, filename );

     DEBUGMSG( "DirectFB/Media: PNG Provider Construct '%s'\n", filename );

     thiz->AddRef = IDirectFBImageProvider_PNG_AddRef;
     thiz->Release = IDirectFBImageProvider_PNG_Release;
     thiz->RenderTo = IDirectFBImageProvider_PNG_RenderTo;
     thiz->GetSurfaceDescription =
                               IDirectFBImageProvider_PNG_GetSurfaceDescription;

     return DFB_OK;
}

void IDirectFBImageProvider_PNG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;

     free( data->filename );
     
     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBImageProvider_PNG_AddRef( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBImageProvider_PNG_Release( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBImageProvider_PNG_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                               IDirectFBSurface *destination )
{
     int err, loader_result = 1;
     void *dst;
     int pitch, width, height;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps;
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

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

DFBResult load_png_argb( FILE *f, __u8 *dst, int width, int height,
                         int pitch, int format )
{
     png_structp png_ptr;
     png_infop info_ptr;

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

     {
          png_uint_32 png_width, png_height;
          int png_bpp, png_type, number_of_passes;

          png_get_IHDR( png_ptr, info_ptr, &png_width, &png_height, &png_bpp,
                        &png_type, NULL /* interlace_type */, NULL, NULL );

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

          number_of_passes = png_set_interlace_handling(png_ptr);

          if (width == png_width
                &&  height == png_height && BYTES_PER_PIXEL(format) == 4)
          {
               while (number_of_passes--) {
                    int h = png_height;
                    png_bytep dest = dst;
                    while (h--) {
                         png_read_row( png_ptr, dest, NULL );
                         dest += pitch;
                    }
               }
          }
          else {
               int i;
               int png_rowbytes;
               png_bytep buffer;
               png_bytep bptr;

               /* stupid libpng returns only 3 if we use the filler */
               png_rowbytes = png_width*4;

               buffer = malloc( png_rowbytes * png_height );

               while (number_of_passes--) {
                    bptr = buffer;
                    for (i=0; i<png_height; i++)  {
                         png_read_row( png_ptr, bptr, NULL );
                         bptr += png_rowbytes;
                    }
               }

               scale_linear_32( (__u32*)dst, (__u32*)buffer, png_width,
                                png_height, width, height,
                                pitch-width*BYTES_PER_PIXEL(format), format );

               free( buffer );
          }
     }

     png_destroy_read_struct( &png_ptr, &info_ptr, NULL );

     return DFB_OK;
}

DFBResult IDirectFBImageProvider_PNG_GetSurfaceDescription(
                                              IDirectFBImageProvider *thiz,
                                              DFBSurfaceDescription *dsc )
{
     IDirectFBImageProvider_PNG_data *data =
                                   (IDirectFBImageProvider_PNG_data*)thiz->priv;
     FILE *f;

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

          memset( dsc, 0, sizeof(DFBSurfaceDescription) );
          dsc->flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc->width = png_width;
          dsc->height = png_height;

          if (png_type & PNG_COLOR_MASK_ALPHA) 
               dsc->pixelformat = DSPF_ARGB;
          else 
               dsc->pixelformat= layers->surface->format;               

          png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
          fclose( f );
     }

     return DFB_OK;
}

