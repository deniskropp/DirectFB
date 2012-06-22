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

D_DEBUG_DOMAIN( imageProviderANDROID, "ANDROID/ImageProvider", "Android ImageProvider" );

/*
 * private data struct of IDirectFBImageProvider_ANDROID
 */
typedef struct {
     IDirectFBImageProvider_data base;

     char *path;
     int   width;
     int   height;
     int   alpha;
     int   pitch;
     int   format;
     char *image;

     jbyteArray pixels;
} IDirectFBImageProvider_ANDROID_data;

extern AndroidData *m_data;

static int
check_exception( JNIEnv *env )
{
     if ((*env)->ExceptionCheck(env)) {
          (*env)->ExceptionDescribe( env );
          (*env)->ExceptionClear( env );

          return 1;
     }

     return 0;
}

static DFBResult
readBufferStream( IDirectFBImageProvider_ANDROID_data  *data,
                  char                                **bufferData,
                  int                                  *bufferSize )
{
     IDirectFBDataBuffer *buffer     = data->base.buffer;
     int                  total_size = 0;
     const int            bufsize    = 0x10000;
     char                *buf        = NULL;
     DFBResult            ret;
     int                  len;
     char                *rbuf; 

     while (1) {
          rbuf = realloc( buf, total_size + bufsize );
          if (!rbuf) {
               free( buf );
               return DFB_NOSYSTEMMEMORY;
          }

          buf = rbuf;

          while (buffer->HasData( buffer ) == DFB_OK) {
               D_DEBUG_AT( imageProviderANDROID, "Retrieving data (up to %d )...\n", bufsize );

               ret = buffer->GetData( buffer, bufsize, &buf[total_size], &len );
               if (ret)
                    return ret;

               D_DEBUG_AT( imageProviderANDROID, "  -> got %d bytes\n", len );

               total_size += len;
          }

          D_DEBUG_AT( imageProviderANDROID, "Waiting for data...\n" );

          if (buffer->WaitForData( buffer, 1 ) == DFB_EOF) {
               *bufferData = buf;
               *bufferSize = total_size;
               return DFB_OK;
          }
     }

     free( buf );

     return DFB_INCOMPLETE;
}

static DFBResult
decodeImage( IDirectFBImageProvider_ANDROID_data *data )
{
     DFBResult   ret;
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
     char       *sdata   = 0;
     int         ssize   = 0;

     if (data->image)
          return DFB_OK;

     (*m_data->java_vm)->AttachCurrentThread( m_data->java_vm, &env, NULL );
     if (!env) {
          D_DEBUG_AT( imageProviderANDROID, "decodeImage: Failed to attach current thread to JVM\n" );
          return DFB_INIT;
     }

     clazz = (*env)->FindClass( env, "android/graphics/BitmapFactory" );
     if (check_exception( env ) || !clazz)
          return DFB_INIT;

     if (data->path) {
          method = (*env)->GetStaticMethodID( env, clazz, "decodeFile", "(Ljava/lang/String;)Landroid/graphics/Bitmap;" );
          if (check_exception( env ) || !method)
               return DFB_INIT;

          path = (*env)->NewStringUTF( env, data->path );
          if (check_exception( env ) || !path)
               return DFB_NOSYSTEMMEMORY;

          bitmap = (*env)->CallStaticObjectMethod( env, clazz, method, path );
          if (check_exception( env ) || !bitmap) {
               (*env)->DeleteLocalRef( env, path );
               check_exception( env );
               return DFB_INIT;
          }

          (*env)->DeleteLocalRef( env, path );
          if (check_exception( env ))
               return DFB_INIT;
     }
     else {
          jbyteArray jArray = 0;

          ret = readBufferStream( data, &sdata, &ssize );
          if (ret)
               return ret;

          method = (*env)->GetStaticMethodID( env, clazz, "decodeByteArray", "([BII)Landroid/graphics/Bitmap;" );
          if (check_exception( env ) || !method) {
               free( sdata );
               return DFB_INIT;
          }

          jArray = (*env)->NewByteArray( env, ssize );
          if (check_exception( env ) || !jArray) {
               free( sdata );
               return DFB_NOSYSTEMMEMORY;
          }

          (*env)->SetByteArrayRegion(env, jArray, 0, ssize, sdata);
          if (check_exception( env )) {
               (*env)->DeleteLocalRef( env, jArray );
               check_exception( env );
               free( sdata );
               return DFB_INIT;
          }

          bitmap = (*env)->CallStaticObjectMethod( env, clazz, method, jArray, 0, ssize );
          if (check_exception( env ) || !bitmap) {
               free( sdata );
               (*env)->DeleteLocalRef( env, jArray );
               check_exception( env );
               return DFB_INIT;
          }

          (*env)->DeleteLocalRef( env, jArray );
          if (check_exception( env )) {
               free( sdata );
               return DFB_INIT;
          }

          free( sdata );
     }

     clazz = (*env)->GetObjectClass(env, bitmap);
     if (check_exception( env ) || !clazz)
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz, "getWidth", "()I" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     data->width = (*env)->CallIntMethod( env, bitmap, method );
     if (check_exception( env ))
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz, "getHeight", "()I" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     data->height = (*env)->CallIntMethod( env, bitmap, method );
     if (check_exception( env ))
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz, "hasAlpha", "()Z" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     data->alpha = (*env)->CallBooleanMethod( env, bitmap, method );
     if (check_exception( env ))
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz, "getRowBytes", "()I" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     data->pitch = (*env)->CallIntMethod( env, bitmap, method );
     if (check_exception( env ))
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz, "getConfig", "()Landroid/graphics/Bitmap$Config;" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     config = (*env)->CallObjectMethod( env, bitmap, method );
     if (check_exception( env ) || !config)
          return DFB_INIT;

     clazz2 = (*env)->FindClass( env, "android/graphics/Bitmap$Config" );
     if (check_exception( env ) || !clazz2)
          return DFB_INIT;

     method = (*env)->GetMethodID( env, clazz2, "name", "()Ljava/lang/String;" );
     if (check_exception( env ) || !method)
          return DFB_INIT;

     format = (jstring)(*env)->CallObjectMethod( env, config, method );
     if (check_exception( env ) || !format)
          return DFB_INIT;

     fvalue = (*env)->GetStringUTFChars( env, format, 0 );
     if (check_exception( env ) || !fvalue)
          return DFB_NOSYSTEMMEMORY;

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

     (*env)->ReleaseStringUTFChars( env, format, fvalue );
     if (check_exception( env ))
          return DFB_INIT;

     if (DSPF_ARGB != data->format) {
          const char *nconfig_name  = "ARGB_8888";
          jstring     jconfig_name  = 0;
          jclass      config_clazz  = 0;
          jobject     bitmap_config = 0;

          jconfig_name = (*env)->NewStringUTF( env, nconfig_name );
          if (check_exception( env ) || !jconfig_name)
               return DFB_INIT;

          config_clazz = (*env)->FindClass( env, "android/graphics/Bitmap$Config" );
          if (check_exception( env ) || !config_clazz) {
               (*env)->DeleteLocalRef( env, jconfig_name );
               check_exception( env );
               return DFB_INIT;
          }

          method = (*env)->GetStaticMethodID( env, config_clazz, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;" );
          if (check_exception( env ) || !method) {
               (*env)->DeleteLocalRef( env, jconfig_name );
               check_exception( env );
               return DFB_INIT;
          }

          bitmap_config = (*env)->CallStaticObjectMethod( env, config_clazz, method, jconfig_name );
          if (check_exception( env ) || !bitmap_config) {
               (*env)->DeleteLocalRef( env, jconfig_name );
               check_exception( env );
               return DFB_INIT;
          }

          method = (*env)->GetMethodID( env, clazz, "copy", "(Landroid/graphics/Bitmap$Config;Z)Landroid/graphics/Bitmap;" );
          if (check_exception( env ) || !method) {
               (*env)->DeleteLocalRef( env, jconfig_name );
               check_exception( env );
               return DFB_INIT;
          }

          convert = (*env)->CallObjectMethod( env, bitmap, method,  bitmap_config, 0 );
          if (check_exception( env ) || !convert) {
               (*env)->DeleteLocalRef( env, jconfig_name );
               check_exception( env );
               return DFB_INIT;
          }

          (*env)->DeleteLocalRef( env, jconfig_name );
          if (check_exception( env ))
               return DFB_INIT;

          bitmap = convert;

          data->format = DSPF_ARGB;

          method = (*env)->GetMethodID( env, clazz, "getRowBytes", "()I" );
          if (check_exception( env ) || !method)
               return DFB_INIT;

          data->pitch = (*env)->CallIntMethod( env, bitmap, method );
          if (check_exception( env ))
               return DFB_INIT;
     }

     pixels = (*env)->NewByteArray( env, data->pitch * data->height);
     if (check_exception( env ) || !pixels)
          return DFB_NOSYSTEMMEMORY;

     clazz2 = (*env)->FindClass( env, "java/nio/ByteBuffer" );
     if (check_exception( env ) || !clazz2) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     method = (*env)->GetStaticMethodID( env, clazz2, "wrap", "([B)Ljava/nio/ByteBuffer;" );
     if (check_exception( env ) || !method) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     buffer = (*env)->CallStaticObjectMethod( env, clazz2, method, pixels );
     if (check_exception( env ) || !buffer) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     method = (*env)->GetMethodID( env, clazz, "copyPixelsToBuffer", "(Ljava/nio/Buffer;)V" );
     if (check_exception( env ) || !method) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     (*env)->CallVoidMethod( env, bitmap, method, buffer );
     if (check_exception( env )) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     data->pixels = (*env)->NewGlobalRef( env, pixels );
     if (check_exception( env )) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     data->image = (*env)->GetByteArrayElements( env, pixels, 0 );
     if (check_exception( env ) || !data->image) {
          (*env)->DeleteLocalRef( env, pixels );
          check_exception( env );
          return DFB_INIT;
     }

     return DFB_OK;
}

static void
IDirectFBImageProvider_ANDROID_Destruct( IDirectFBImageProvider *thiz )
{
     JNIEnv *env = 0;

     IDirectFBImageProvider_ANDROID_data *data = (IDirectFBImageProvider_ANDROID_data *)thiz->priv;

     (*m_data->java_vm)->AttachCurrentThread( m_data->java_vm, &env, NULL );
     if (!env) {
          D_DEBUG_AT( imageProviderANDROID, "Destruct: Failed to attach current thread to JVM\n" );
          return;
     }

     if (data->image) {
          (*env)->ReleaseByteArrayElements( env, data->pixels, data->image, JNI_ABORT );
          check_exception( env );
     }

     (*env)->DeleteGlobalRef( env, data->pixels );
     check_exception( env );

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
          //desc->pixelformat = DSPF_ABGR;
     else
          desc->pixelformat = primary_format;

     desc->width  = data->width;
     desc->height = data->height;

     D_DEBUG_AT( imageProviderANDROID, "GetSurfaceDescription: width=%d height=%d pitch=%d has_alpha=%d pixelformat=%s/%s\n",
                 data->width, data->height, data->pitch, data->alpha, dfb_pixelformat_name(data->format), dfb_pixelformat_name(desc->pixelformat) );

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

     D_DEBUG_AT( imageProviderANDROID, "GetImageDescription: width=%d height=%d pitch=%d has_alpha=%d pixelformat=%s\n",
                 data->width, data->height, data->pitch, data->alpha, dfb_pixelformat_name(data->format) );

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
