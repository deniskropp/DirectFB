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
     char *image;

     jobject    buffer;
     jbyteArray pixels;
     jobject    bitmap;
     jobject    config;
} IDirectFBImageProvider_ANDROID_data;

extern AndroidData *m_data;

#define CHECK_EXCEPTION( env ) {		\
     if ((*env)->ExceptionCheck(env)) {		\
          (*env)->ExceptionDescribe( env );	\
          (*env)->ExceptionClear( env );	\
          return DFB_INIT;			\
     }						\
}

static DFBResult
decodeImage( IDirectFBImageProvider_ANDROID_data *data )
{
     JNIEnv     *env     = 0;
     jclass      clazz   = 0;
     jclass      clazz2  = 0;
     jmethodID   method  = 0;
     jstring     path    = 0;
     jobject     buffer  = 0;
     jbyteArray  pixels  = 0;
     jobject     bitmap  = 0;
     jobject     convert = 0;
     jobject     config  = 0;
     jstring     format  = 0;
     const char *fvalue  = 0;

     if (data->image)
          return DFB_OK;

     //FIXME
     if (!data->path)
          return DFB_UNSUPPORTED;

     (*m_data->java_vm)->AttachCurrentThread( m_data->java_vm, &env, NULL );
     if (!env)
          return DFB_INIT;

     clazz = (*env)->FindClass( env, "android/graphics/BitmapFactory" );
     CHECK_EXCEPTION( env );
     if (!clazz)
          return DFB_INIT;

     method = (*env)->GetStaticMethodID( env, clazz, "decodeFile", "(Ljava/lang/String;)Landroid/graphics/Bitmap;" );
     CHECK_EXCEPTION( env );
     if (!method)
          return DFB_INIT;

     path = (*env)->NewStringUTF( env, data->path );
     CHECK_EXCEPTION( env );
     if (!path)
          return DFB_INIT;

     bitmap = (*env)->CallStaticObjectMethod( env, clazz, method, path );
     CHECK_EXCEPTION( env );
     if (!bitmap) {
          (*env)->DeleteLocalRef( env, path );
          return DFB_INIT;
     }

     (*env)->DeleteLocalRef( env, path );
     CHECK_EXCEPTION( env );
     (*env)->NewGlobalRef( env, bitmap );
     CHECK_EXCEPTION( env );

     clazz = (*env)->GetObjectClass(env, bitmap);
     CHECK_EXCEPTION( env );
     if (!clazz)
          goto error;

     method = (*env)->GetMethodID( env, clazz, "getWidth", "()I" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     data->width = (*env)->CallIntMethod( env, bitmap, method );
     CHECK_EXCEPTION( env );

     method = (*env)->GetMethodID( env, clazz, "getHeight", "()I" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     data->height = (*env)->CallIntMethod( env, bitmap, method );
     CHECK_EXCEPTION( env );

     method = (*env)->GetMethodID( env, clazz, "hasAlpha", "()Z" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     data->alpha = (*env)->CallBooleanMethod( env, bitmap, method );
     CHECK_EXCEPTION( env );

     method = (*env)->GetMethodID( env, clazz, "getRowBytes", "()I" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     data->pitch = (*env)->CallIntMethod( env, bitmap, method );
     CHECK_EXCEPTION( env );

     method = (*env)->GetMethodID( env, clazz, "getConfig", "()Landroid/graphics/Bitmap/Config;" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     config = (*env)->CallObjectMethod( env, bitmap, method );
     CHECK_EXCEPTION( env );
     if (!config)
          goto error;

     (*env)->NewGlobalRef( env, config );
     CHECK_EXCEPTION( env );

     clazz2 = (*env)->FindClass( env, "android/graphics/Bitmap/Config" );
     CHECK_EXCEPTION( env );
     if (!clazz2)
          goto error;

     method = (*env)->GetMethodID( env, clazz2, "name", "()Ljava/lang/String;" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     format = (jstring)(*env)->CallObjectMethod( env, config, method );
     CHECK_EXCEPTION( env );
     if (!format)
          goto error;

     fvalue = (*env)->GetStringUTFChars( env, format, 0 );
     CHECK_EXCEPTION( env );
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
          data->format = DSPF_RGB16;
     }
     else {
          data->format = DSPF_UNKNOWN;
     }

     if (DSPF_ARGB != data->format) {
          const wchar_t nconfig_name[] = L"ARGB_8888";
          jstring       jconfig_name   = (*env)->NewString( env, (const jchar*)nconfig_name, wcslen(nconfig_name) );
          jclass        config_clazz   = (*env)->FindClass( env, "android/graphics/Bitmap$Config" );
          jobject       bitmap_config  = 0;

          method = (*env)->GetStaticMethodID( env, config_clazz, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;" );
          CHECK_EXCEPTION( env );
          if (!method)
               goto error;

          bitmap_config = (*env)->CallStaticObjectMethod( env, config_clazz, method, jconfig_name );
          CHECK_EXCEPTION( env );
          if (!bitmap_config)
               goto error;

          method = (*env)->GetMethodID( env, clazz, "copy", "(Landroid/graphics/Bitmap/Config;Z)Landroid/graphics/Bitmap;" );
          CHECK_EXCEPTION( env );
          if (!method)
               goto error;

          convert = (*env)->CallObjectMethod( env, bitmap, method,  bitmap_config, 0 );
          CHECK_EXCEPTION( env );
          if (!convert)
               goto error;

          (*env)->DeleteGlobalRef( env, bitmap );
          CHECK_EXCEPTION( env );

          bitmap = convert;

          data->format = DSPF_ARGB;
     }

     pixels = (*env)->NewByteArray( env, data->width * data->height );
     CHECK_EXCEPTION( env );
     if (!pixels)
          goto error;

     (*env)->NewGlobalRef( env, pixels );
     CHECK_EXCEPTION( env );

     clazz2 = (*env)->FindClass( env, "java/nio/ByteBuffer" );
     CHECK_EXCEPTION( env );
     if (!clazz2)
          goto error;

     method = (*env)->GetStaticMethodID( env, clazz2, "wrap", "([B)Ljava/nio/ByteBuffer;" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     buffer = (*env)->CallStaticObjectMethod( env, clazz2, method, pixels );
     CHECK_EXCEPTION( env );
     if (!buffer)
          goto error;

     (*env)->NewGlobalRef( env, buffer );
     CHECK_EXCEPTION( env );

     method = (*env)->GetMethodID( env, clazz, "copyPixelsToBuffer", "(Ljava/nio/ByteBuffer;)V" );
     CHECK_EXCEPTION( env );
     if (!method)
          goto error;

     (*env)->CallVoidMethod( env, bitmap, method, buffer );
     CHECK_EXCEPTION( env );

     data->image = (*env)->GetByteArrayElements( env, pixels, 0 );
     CHECK_EXCEPTION( env );
     if (!data->image)
          goto error;

     data->buffer  = buffer;
     data->pixels  = pixels;
     data->bitmap  = bitmap;
     data->config  = config;

     return DFB_OK;

error:
     if (bitmap) {
          (*env)->DeleteGlobalRef( env, bitmap );
          CHECK_EXCEPTION( env );
     }

     if (convert) {
          (*env)->DeleteGlobalRef( env, convert );
          CHECK_EXCEPTION( env );
     }

     if (config) {
          (*env)->DeleteGlobalRef( env, config );
          CHECK_EXCEPTION( env );
     }

     if (pixels) {
          (*env)->DeleteGlobalRef( env, pixels );
          CHECK_EXCEPTION( env );
     }

     if (buffer) {
          (*env)->DeleteGlobalRef( env, buffer );
          CHECK_EXCEPTION( env );
     }

     return DFB_INIT;
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
     bool                   direct = false;
     DFBRegion              clip;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     CoreSurfaceBufferLock  lock;
     DIRenderCallbackResult cb_result = DIRCR_OK;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_ANDROID)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     ret = decodeImage( data );
     if (ret)
          return ret;

     ret = destination->GetPixelFormat( destination, &format );
     if (ret)
          return ret;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;

          if (!dfb_rectangle_region_intersects( &rect, &clip ))
               return DFB_OK;
     }
     else {
          rect = dst_data->area.wanted;
     }

     ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret)
          return ret;

     dfb_scale_linear_32( (u32 *)data->image, data->width, data->height, lock.addr, lock.pitch, &rect, dst_surface, &clip );
     if (data->base.render_callback) {
          DFBRectangle r = { 0, 0, data->width, data->height };
          data->base.render_callback( &r, data->base.render_callback_context );
     }

     dfb_surface_unlock_buffer( dst_surface, &lock );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_ANDROID_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                      DFBSurfaceDescription  *desc )
{
     DFBResult             ret;
     DFBSurfacePixelFormat primary_format = dfb_primary_layer_pixelformat();

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_ANDROID)

     if (!desc)
          return DFB_INVARG;

     ret = decodeImage( data );
     if (ret)
          return ret;

     desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;

     if (data->alpha)
          desc->pixelformat = DFB_PIXELFORMAT_HAS_ALPHA(primary_format) ? primary_format : DSPF_ARGB;
     else
          desc->pixelformat = primary_format;

     desc->width  = data->width;
     desc->height = data->height;
                         
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_ANDROID_GetImageDescription( IDirectFBImageProvider *thiz,
                                                    DFBImageDescription    *desc )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_ANDROID)

     if (!desc)
          return DFB_INVARG;

     ret = decodeImage( data );
     if (ret)
          return ret;

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

     buffer_data = buffer->priv;
     if (!buffer_data) {
          ret = DFB_DEAD;
          goto error;
     }

     if (buffer_data->filename) {
          data->path = D_STRDUP( buffer_data->filename );

          if (stat( buffer_data->filename, &info ) < 0) {
               ret = errno2result( errno );
               D_PERROR( "ImageProvider/ANDROID: Failure during fstat() of '%s'!\n", buffer_data->filename );
               goto error;
          }

          data->size = info.st_size;
     }

     data->base.ref    = 1;
     data->base.core   = core;
     data->base.buffer = buffer;

     buffer->AddRef( buffer );

     data->base.Destruct = IDirectFBImageProvider_ANDROID_Destruct;

     thiz->RenderTo              = IDirectFBImageProvider_ANDROID_RenderTo;
     thiz->GetImageDescription   = IDirectFBImageProvider_ANDROID_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_ANDROID_GetSurfaceDescription;

     return DFB_OK;

error:
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}
