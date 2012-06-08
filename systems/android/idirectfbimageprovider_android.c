/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/interface.h>
#include <display/idirectfbsurface.h>
#include <core/system.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>
#include <misc/gfx_util.h>

#include "android_system.h"

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, ANDROID )

/*
 * private data struct of IDirectFBImageProvider_ANDROID
 */
typedef struct {
     IDirectFBImageProvider_data base;

     char *path;
     int   size;
     int   width;
     int   height;
     int   alpha;
     int   pitch;
     int   format;
     char *pixptr;
     
     jobject    buffer;
     jbyteArray pixels;
     jobject    bitmap;
     jobject    config;
} IDirectFBImageProvider_ANDROID_data;

extern AndroidData *m_data;

static int
decodeImage( IDirectFBImageProvider_ANDROID_data *data )
{
     JNIEnv     *env    = 0;
     jclass      clazz  = 0;
     jclass      clazz2 = 0;
     jmethodID   method = 0;
     jstring     path   = 0;
     jobject     buffer = 0;
     jbyteArray  pixels = 0;
     jobject     bitmap = 0;
     jobject     config = 0;
     jstring     format = 0;
     const char *fvalue = 0;

     if (data->pixptr)
          return 1;

     (*m_data->java_vm)->AttachCurrentThread( m_data->java_vm, &env, NULL );
     if (!env)
          return 0;

     clazz = (*env)->FindClass( env, "android/graphics/BitmapFactory" );
     if (!clazz)
          return 0;

     method = (*env)->GetStaticMethodID( env, clazz, "decodeFile", "(Ljava/lang/String;)Landroid/graphics/Bitmap;" );
     if (!method)
          return 0;

     path = (*env)->NewStringUTF( env, data->path );
     if (!path)
          return 0;

     bitmap = (*env)->CallStaticObjectMethod( env, clazz, method, path );
     if (!bitmap) {
          (*env)->DeleteLocalRef( env, path );
          return 0;
     }

     (*env)->DeleteLocalRef( env, path );
     (*env)->NewGlobalRef( env, bitmap );

     clazz = (*env)->GetObjectClass(env, bitmap);
     if (!clazz)
          goto error;

     method = (*env)->GetMethodID( env, clazz, "getWidth", "()I" );
     if (!method)
          goto error;

     data->width = (*env)->CallIntMethod( env, bitmap, method );

     method = (*env)->GetMethodID( env, clazz, "getHeight", "()I" );
     if (!method)
          goto error;

     data->height = (*env)->CallIntMethod( env, bitmap, method );

     method = (*env)->GetMethodID( env, clazz, "hasAlpha", "()Z" );
     if (!method)
          goto error;

     data->alpha = (*env)->CallBooleanMethod( env, bitmap, method );

     method = (*env)->GetMethodID( env, clazz, "getRowBytes", "()I" );
     if (!method)
          goto error;

     data->pitch = (*env)->CallIntMethod( env, bitmap, method );

     method = (*env)->GetMethodID( env, clazz, "getConfig", "()Landroid/graphics/Bitmap/Config;" );
     if (!method)
          goto error;

     config = (*env)->CallObjectMethod( env, bitmap, method );
     if (!config)
          goto error;

     (*env)->NewGlobalRef( env, config );

     clazz2 = (*env)->FindClass( env, "android/graphics/Bitmap/Config" );
     if (!clazz2)
          goto error;

     method = (*env)->GetMethodID( env, clazz2, "name", "()Ljava/lang/String;" );
     if (!method)
          goto error;

     format = (jstring)(*env)->CallObjectMethod( env, config, method );
     if (!format)
          goto error;

     fvalue = (*env)->GetStringUTFChars( env, format, 0 );
     if (!fvalue)
          goto error;

     if (!strcmp( fvalue, "ALPHA_8" )) {
          data->format = DSPF_A8;
     }
     else if (!strcmp( fvalue, "ARGB_4444" )) {
          data->format = DSPF_ARGB4444;
     }
     else if (!strcmp( fvalue, "ARGB_8888" )) {
          data->format = DSPF_ARGB;
     }
     else if (!strcmp( fvalue, "RGB_565" )) {
          data->format = DSPF_RGB555;
     }
     else {
          data->format = DSPF_UNKNOWN;
     }

     pixels = (*env)->NewByteArray( env, data->width * data->height );
     if (!pixels)
          goto error;

     (*env)->NewGlobalRef( env, pixels );

     clazz2 = (*env)->FindClass( env, "java/nio/ByteBuffer" );
     if (!clazz2)
          goto error;

     method = (*env)->GetStaticMethodID( env, clazz2, "wrap", "([B)Ljava/nio/ByteBuffer;" );
     if (!method)
          goto error;

     buffer = (*env)->CallStaticObjectMethod( env, clazz2, method, pixels );
     if (!buffer)
          goto error;

     (*env)->NewGlobalRef( env, buffer );

     method = (*env)->GetMethodID( env, clazz, "copyPixelsToBuffer", "(Ljava/nio/ByteBuffer;)V" );
     if (!method)
          goto error;

     (*env)->CallVoidMethod( env, bitmap, method, buffer );

     data->pixptr = (*env)->GetByteArrayElements( env, pixels, 0 );
     if (!data->pixptr)
          goto error;

     data->buffer = buffer;
     data->pixels = pixels;
     data->bitmap = bitmap;
     data->config = config;

     return 1;

error:
     if (bitmap)
          (*env)->DeleteGlobalRef( env, bitmap );

     if (config)
          (*env)->DeleteGlobalRef( env, config );

     if (pixels)
          (*env)->DeleteGlobalRef( env, pixels );

     if (buffer)
          (*env)->DeleteGlobalRef( env, buffer );

     return 0;
}

static void
IDirectFBImageProvider_ANDROID_Destruct( IDirectFBImageProvider *thiz )
{
     JNIEnv *env = 0;

     IDirectFBImageProvider_ANDROID_data *data = (IDirectFBImageProvider_ANDROID_data *)thiz->priv;

     (*m_data->java_vm)->AttachCurrentThread( m_data->java_vm, &env, NULL );
     if (!env)
          return;

     if (data->bitmap)
          (*env)->DeleteGlobalRef( env, data->bitmap );

     if (data->pixels)
          (*env)->DeleteGlobalRef( env, data->pixels );

     if (data->buffer)
          (*env)->DeleteGlobalRef( env, data->buffer );

     if (data->config)
          (*env)->DeleteGlobalRef( env, data->config );

     if (data->path)
          D_FREE( data->path );
}

static DFBResult
IDirectFBImageProvider_ANDROID_RenderTo( IDirectFBImageProvider *thiz,
                                         IDirectFBSurface       *destination,
                                         const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DFBRectangle           rect;
     DFBRectangle           clipped;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_ANDROID)

     if (!destination)
          return DFB_INVARG;

     if (!decodeImage( data ))
          return DFB_INIT;

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

     if (DFB_RECTANGLE_EQUAL( rect, clipped ) &&
         (unsigned)rect.w == data->width && (unsigned)rect.h == data->height &&
         dst_surface->config.format == data->format)
     {
          ret = dfb_surface_write_buffer( dst_surface, CSBR_BACK, (u8*)data->pixptr, data->pitch, &rect );
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
          desc.preallocated[0].data  = (u8*)data->pixptr;
          desc.preallocated[0].pitch = data->pitch;

          ret = data->base.idirectfb->CreateSurface( data->base.idirectfb, &desc, &source );
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
     
     if (data->base.render_callback) {
          DFBRectangle rect = { 0, 0, clipped.w, clipped.h };
          data->base.render_callback( &rect, data->base.render_callback_context );
     }

     return DFB_OK;
}

/* Loading routines */

static DFBResult
IDirectFBImageProvider_ANDROID_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                      DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_ANDROID)

     if (!desc)
          return DFB_INVARG;

     if (!decodeImage( data ))
          return DFB_INIT;

     desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT;
     desc->width  = data->width;
     desc->height = data->height;
                         
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_ANDROID_GetImageDescription( IDirectFBImageProvider *thiz,
                                                    DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_ANDROID)

     if (!desc)
          return DFB_INVARG;

     if (!decodeImage( data ))
          return DFB_INIT;

     desc->caps = DICAPS_NONE;
        
     if (data->alpha)
          desc->caps |= DICAPS_ALPHACHANNEL;

     return DFB_OK;
}

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (dfb_system_type() == CORE_ANDROID)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult                 ret;
     struct stat               info;
     void                     *ptr;
     IDirectFBDataBuffer_data *buffer_data;
     IDirectFBDataBuffer      *buffer;
     CoreDFB                  *core;
     va_list                   tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_ANDROID)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core   = va_arg( tag, CoreDFB * );
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

     /* Query file size etc. */
     if (stat( buffer_data->filename, &info ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "ImageProvider/ANDROID: Failure during fstat() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     data->base.ref    = 1;
     data->base.core   = core;
     data->base.buffer = buffer;

     buffer->AddRef( buffer );

     data->size = info.st_size;
     data->path = D_STRDUP( buffer_data->filename );

     data->base.Destruct = IDirectFBImageProvider_ANDROID_Destruct;

     thiz->RenderTo              = IDirectFBImageProvider_ANDROID_RenderTo;
     thiz->GetImageDescription   = IDirectFBImageProvider_ANDROID_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_ANDROID_GetSurfaceDescription;

     return DFB_OK;

error:
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}
