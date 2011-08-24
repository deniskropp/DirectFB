/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>

#include <directfb.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <idirectfb.h>

#include <core/surface.h>
#include <core/layers.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbvideoprovider.h>

#include <misc/gfx_util.h>

#undef HAVE_STDLIB_H
#include <libmng.h>

static DFBResult Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBVideoProvider *thiz,
                            IDirectFBDataBuffer    *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, MNG )

/*****************************************************************************/

typedef struct {
     int                            ref;      /* reference counter */

     IDirectFBDataBuffer           *buffer;
     DFBBoolean                     seekable;
     
     IDirectFBSurface              *destination;
     IDirectFBSurface_data         *dst_data;
     DFBRectangle                   dst_rect;
     
     mng_handle                     mng_handle;            /* mng handle */
     u32                           *image;
     
     DirectThread                  *thread;
     pthread_mutex_t                lock;
     pthread_cond_t                 cond;
     
     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     double                         speed;
     
     unsigned int                   start_pos;

     char                           image_type[4];
     unsigned int                   width;
     unsigned int                   height;

     mng_uint32                     delay;/* ticks to wait before resuming decode */

     unsigned int                   AspectRatio;
     unsigned int                   delayTime;

     DVFrameCallback                callback;
     void                          *callback_ctx;
} IDirectFBVideoProvider_MNG_data;

#define MNGERRORMSG(x, ...) \
     D_ERROR( "IDirectFBVideoProvider_MNG: " #x "!\n", ## __VA_ARGS__ )

#define MNGDEBUGMSG(x, ...) \
     D_DEBUG( "IDirectFBVideoProvider_MNG: " #x "!\n", ## __VA_ARGS__ )

/*****************************************************************************/

/////////////////////////////////////////////MNG callbacks////////////////////////////////////////////////////////////

static mng_ptr Memalloc( mng_size_t iLen )
{
	//D_INFO("Enter function %s\n", __func__); 

	mng_ptr pResult = D_MALLOC( iLen );   /* get memory from the heap */

	if( pResult )                       /* Added - condition */
		memset( pResult, 0, iLen );

	return pResult;
}

static void Memfree( mng_ptr iPtr, mng_size_t iLen )
{
	//D_INFO("Enter function %s\n", __func__); 

	if(iPtr) 
		D_FREE( iPtr );   /* free the memory */
	(void)iLen;         // Kill compiler warning
}

static mng_bool Openstream( mng_handle hHandle )
{	
	return MNG_TRUE;
}

static mng_bool Closestream( mng_handle hHandle )
{	
	return MNG_TRUE;
}

static mng_bool Readdata ( mng_handle hHandle, mng_ptr pBuf,
                           mng_uint32 iBuflen, mng_uint32 *pRead )
{
    DFBResult                         ret;
	IDirectFBVideoProvider_MNG_data  *data;
    IDirectFBDataBuffer              *buffer;

    //D_INFO("Enter function %s\n", __func__); 

    /* dereference our structure */
    data = (IDirectFBVideoProvider_MNG_data *)mng_get_userdata( hHandle );
    buffer = data->buffer;

    if (buffer->HasData( buffer ) == DFB_OK) {
        ret = buffer->GetData( buffer, iBuflen, pBuf, pRead );
        if(ret) {
            return MNG_FALSE;
        }
    }else{
        return MNG_FALSE;
    }

	return MNG_TRUE;
}

static mng_bool ProcessHeader ( mng_handle hHandle,
                                mng_uint32 iWidth, mng_uint32 iHeight )
{
    IDirectFBVideoProvider_MNG_data  *data;
    DFBSurfacePixelFormat             pixelformat;
    mng_imgtype                       imgtype;
	
    D_INFO("Enter function %s, image width = %d, height = %d\n", __func__, iWidth, iHeight); 

    /* dereference our structure */
    data = (IDirectFBVideoProvider_MNG_data *)mng_get_userdata( hHandle );

    data->width = iWidth;
    data->height = iHeight;

    data->image = (u32 *)D_MALLOC(data->width * data->height * 4);

    imgtype = mng_get_imagetype(hHandle);
    switch(imgtype) {
    case mng_it_png:
        strcpy(data->image_type, "PNG");
        break;
    case mng_it_mng:
        strcpy(data->image_type, "MNG");
        break;
    case mng_it_jng:
        strcpy(data->image_type, "JNG");
        break;
    default:
        break;
    }

    /* tell the mng decoder about our bit-depth choice */
    /* FIXME: is it correct for SXL? */
    pixelformat = dfb_primary_layer_pixelformat();
    switch(pixelformat) {
    case DSPF_ARGB:        
        mng_set_canvasstyle(hHandle, MNG_CANVAS_ARGB8);
        break;

    default:
        break;
    }

	return MNG_TRUE;
}

static mng_ptr GetCanvasLine ( mng_handle hHandle, 
                               mng_uint32 iLinenr )
{
    IDirectFBVideoProvider_MNG_data  *data;
    mng_ptr                           row;

    //D_INFO("Enter function %s, line = %d", __func__, iLinenr); 

    /* dereference our structure */
    data = (IDirectFBVideoProvider_MNG_data *)mng_get_userdata( hHandle );

    /* we assume any necessary locking has happened 
       outside, in the frame level code */
    row = data->image + data->width * iLinenr;

    //D_INFO(", row addr = %p\n", row);

	return (row);
}
 
static mng_bool ImageRefresh ( mng_handle hHandle, 
                            mng_uint32 iX, mng_uint32 iY, 
                            mng_uint32 iWidth, mng_uint32 iHeight )
{
    IDirectFBVideoProvider_MNG_data  *data;
    DFBResult                         ret;
    CoreSurface                      *dst_surface;
    CoreSurfaceBufferLock             lock;
    DFBRectangle                      rect;
    DFBRegion                         clip;

    //return MNG_TRUE;

    D_INFO("Enter function %s : x = %d, y = %d, w = %d, h = %d\n", __func__, iX, iY, iWidth, iHeight); 

    /* dereference our structure */
    data = (IDirectFBVideoProvider_MNG_data *)mng_get_userdata( hHandle );
    dst_surface = data->dst_data->surface;
    D_MAGIC_ASSERT( dst_surface, CoreSurface );

    rect = (data->dst_rect.w == 0)
           ? data->dst_data->area.wanted : data->dst_rect;          
    dfb_region_from_rectangle( &clip, &data->dst_data->area.current );

    if (dfb_rectangle_region_intersects( &rect, &clip )){
        //Fix me: currently update the whole image area, need to fine tune the performance
        ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
        if (ret)
             return MNG_FALSE;
        dfb_scale_linear_32( data->image, data->width, data->height,
                                         lock.addr, lock.pitch, &rect, dst_surface, &clip );
        
        #if 0
        {
            int i,j;
            for(j = 0; j < data->height; j++) {
                for(i = 0; i < data->height; i++) {
                    printf("%x ", *(data->image + j * data->width + i));
                }
                printf("\n");
            }
        }
        #endif
        
        if (data->callback) {
            data->callback( data->callback_ctx );
        }
        dfb_surface_unlock_buffer( dst_surface, &lock );
    }

    return MNG_TRUE;
}

static mng_uint32 GetTickCount( mng_handle hHandle )
{
	mng_uint32 ticks;
	struct timeval tv;
	struct timezone tz;

    //D_INFO("Enter function %s\n", __func__); 

	gettimeofday(&tv, &tz);
	ticks = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

    //D_INFO("tickcount = 0x%x\n", ticks);

	return (ticks);
}

static mng_bool SetTimer( mng_handle hHandle, mng_uint32 iMsecs )
{
	IDirectFBVideoProvider_MNG_data  *data;

    //D_INFO("Enter function %s\n", __func__); 

	/* look up our stream struct */
	data = (IDirectFBVideoProvider_MNG_data *) mng_get_userdata(hHandle);

	/* set the timer for when the decoder wants to be woken */
	data->delay = iMsecs;

    //D_INFO("delay = 0x%x\n", iMsecs);

	return MNG_TRUE;
}

static inline void mdelay(unsigned long msec)
{
    //D_INFO("Enter function %s\n", __func__); 

	usleep(msec * 1000);
}

/*****************************************************************************/

static void
IDirectFBVideoProvider_MNG_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_MNG_data *data = thiz->priv;
     
     thiz->Stop( thiz );

     //move cancel rendering and decoding thread here.
     /////////////////////////////////////////////////
     if (data->thread) {
          direct_thread_cancel( data->thread );
          pthread_mutex_lock( &data->lock );
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
          data->dst_data    = NULL;
     }
     //////////////////////////////////////////////////
     
     if (data->image)
          D_FREE( data->image );
     
     if (data->buffer)
          data->buffer->Release( data->buffer );

     if(data->mng_handle)
         mng_cleanup(&data->mng_handle);
    
     pthread_cond_destroy( &data->cond );
     pthread_mutex_destroy( &data->lock );
          
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_MNG_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBVideoProvider_MNG_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )

     if (--data->ref == 0)
          IDirectFBVideoProvider_MNG_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                            DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_SPEED;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!desc)
          return DFB_INVARG;
          
     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = dfb_primary_layer_pixelformat();
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                 DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!desc)
          return DFB_INVARG;
          
     desc->caps = DVSCAPS_VIDEO;
     
     snprintf( desc->video.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "MNG : %s", data->image_type );
     desc->video.framerate = 0;
     desc->video.aspect    = (double)data->AspectRatio/256.0;
     desc->video.bitrate   = 0;
     
     desc->title[0] = desc->author[0] = 
     desc->album[0] = desc->genre[0] = desc->comment[0] = 0;
     desc->year = 0;
     
     return DFB_OK;
}


static void*
MNGVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_MNG_data *data = arg;
     int retcode = MNG_NOERROR;
     
     pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );

     pthread_mutex_lock( &data->lock );
     retcode = mng_display(data->mng_handle);
     D_INFO("==========After mng_display()===============");
     pthread_mutex_unlock( &data->lock );
     
    /* actual loading and rendering */
    while( !direct_thread_is_canceled( self ) ) {  
    
      pthread_mutex_lock( &data->lock );
    
      if (direct_thread_is_canceled( self )) {
           pthread_mutex_unlock( &data->lock );
           break;
      }
      
      if((data->flags & DVPLAY_LOOPING) && (retcode == MNG_NOERROR)) {
          //This function will "reset" the animation into its pristine state. 
          //Calling mng_display() afterwards will re-display the animation from the first frame.
          D_INFO("mng_display_reset, support flag DVPLAY_LOOPING\n");
          retcode = mng_display_reset(data->mng_handle);
          retcode = mng_display(data->mng_handle);
      }
    
      if(data->delay && data->status == DVSTATE_PLAY) {
          mdelay(data->delay);
    
          retcode = mng_display_resume (data->mng_handle);
    
         if (retcode == MNG_NOERROR)
         {
                D_INFO("mng_dispaly_resume, MNG_NOERROR, display finished\n");
                data->delay = 0;
                if(!(data->flags & DVPLAY_LOOPING)) {
                      D_INFO("\nDVSTATE_FINISHED, don't support flag DVPLAY_LOOPING\n");
                      data->status = DVSTATE_FINISHED;
                      pthread_mutex_unlock( &data->lock );
                      break;
                }
         }
         else if (retcode == MNG_NEEDTIMERWAIT){
                 D_INFO("mng_dispaly_resume, need timer wait\n");
         } else{
                 D_INFO("mng_display_resume() return not good value");
         }
      }
      pthread_mutex_unlock( &data->lock );
    }
                          
     return (void*)0;
}


static DFBResult
IDirectFBVideoProvider_MNG_PlayTo( IDirectFBVideoProvider *thiz,
                                   IDirectFBSurface       *destination,
                                   const DFBRectangle     *dest_rect,
                                   DVFrameCallback         callback,
                                   void                   *ctx )
{
     IDirectFBSurface_data *dst_data;
     DFBRectangle           rect = { 0, 0, 0, 0 };
     DFBResult              ret;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!destination)
          return DFB_INVARG;
          
     dst_data = destination->priv;
     if (!dst_data || !dst_data->surface)
          return DFB_DESTROYED;
          
     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          
          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }          
     
     pthread_mutex_lock( &data->lock );
     
     if (data->status == DVSTATE_FINISHED) {
          ret = data->buffer->SeekTo( data->buffer, data->start_pos );
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
     }
     data->status = DVSTATE_PLAY;
     
     if (data->destination)
          data->destination->Release( data->destination );
     
     destination->AddRef( destination );
     data->destination = destination;
     data->dst_data    = dst_data;
     data->dst_rect    = rect;
     
     data->callback     = callback;
     data->callback_ctx = ctx;
     
     if (!data->thread) {
          data->thread = direct_thread_create( DTT_DEFAULT, MNGVideo,
                                              (void*)data, "MNG Video" );
     }
     
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     #if 0
     if (data->thread) {
          direct_thread_cancel( data->thread );
          pthread_mutex_lock( &data->lock );
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
          data->dst_data    = NULL;
     }
     #endif
     
     data->status = DVSTATE_STOP;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetStatus( IDirectFBVideoProvider *thiz,
                                      DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!status)
          return DFB_INVARG;
          
     *status = data->status;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_SeekTo( IDirectFBVideoProvider *thiz,
                                   double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (seconds < 0.0)
          return DFB_INVARG;

     data->status = DVSTATE_PLAY;

     return DFB_OK;
          
     //return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetPos( IDirectFBVideoProvider *thiz,
                                   double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!seconds)
          return DFB_INVARG;

     /* runtime is the actual number of millisecs since the start of the animation */
     *seconds = (double)mng_get_runtime(data->mng_handle)/(double)1000;
     D_INFO("mng_get_runtime : %f", *seconds);

     return DFB_OK;
          
     //*seconds = 0.0;
     //return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetLength( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!seconds)
          return DFB_INVARG;
          
     /* totalframes, totallayers & totalplaytime are filled after a complete run
       of an animation (eg. at MEND); they are also valid after just reading the MNG */
     
     *seconds = (double)mng_get_totalplaytime(data->mng_handle)/(double)1000;
     D_INFO("mng_get_totalplaytime : %f", *seconds);

     return DFB_OK;
          
     //*seconds = 0.0;
     //return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_MNG_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                             DFBVideoProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (flags & ~DVPLAY_LOOPING)
          return DFB_UNSUPPORTED;
          
     if (flags & DVPLAY_LOOPING && !data->seekable)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_SetSpeed( IDirectFBVideoProvider *thiz,
                                     double                  multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
    
     if (data->speed != multiplier) {
          pthread_mutex_lock( &data->lock ); 
          data->speed = multiplier;
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_MNG_GetSpeed( IDirectFBVideoProvider *thiz,
                                     double                 *multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_MNG )
     
     if (!multiplier)
          return DFB_INVARG;
          
     *multiplier = data->speed;
     
     return DFB_OK;
}

/* exported symbols */
static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
    if ((ctx->header[0] == 0x89 && ctx->header[1] == 0x50 && ctx->header[2] == 0x4e && ctx->header[3] == 0x47) ||  //PNG_SIG 89 50 4e 47
     (ctx->header[0] == 0x8b && ctx->header[1] == 0x4a && ctx->header[2] == 0x4e && ctx->header[3] == 0x47) ||  //JNG_SIG 8b 4a 4e 47
     (ctx->header[0] == 0x8a && ctx->header[1] == 0x4d && ctx->header[2] == 0x4e && ctx->header[3] == 0x47)) {  //MNG_SIG 8a 4d 4e 47
        if (ctx->header[4] == 0x0d && ctx->header[5] == 0x0a && ctx->header[6] == 0x1a && ctx->header[7] == 0x0a) {//POST_SIG 0d 0a 1a 0a
            D_INFO("========== MNG video ============\n");

            printf("ctx->filename = %s\n", ctx->filename);

            //check file name
            if (ctx->filename && strrchr (ctx->filename, '.' ) &&
            (strcasecmp ( strrchr (ctx->filename, '.' ), ".mng" ) == 0 ||
             strcasecmp ( strrchr (ctx->filename, '.' ), ".jng") == 0))
                return DFB_OK;

         }else{
             return DFB_UNSUPPORTED;
         }
    }

    return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_MNG )

     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->buffer = buffer;
     data->speed  = 1.0;
     
     buffer->AddRef( buffer );
     data->seekable = (buffer->SeekTo( buffer, 0 ) == DFB_OK);
          
     data->buffer->GetPosition( data->buffer, &data->start_pos );
     
     if(data->mng_handle) {
         mng_cleanup(&data->mng_handle);
     }
     data->mng_handle = mng_initialize( data, Memalloc, Memfree, NULL );

     if(data->mng_handle == MNG_NULL) {
         D_ERROR("Fail to initilize mng handle");
         goto error;
     }

    /* no need to store chunk-info ! */
    mng_set_storechunks( data->mng_handle, MNG_FALSE );
    
    /* use suspension-buffer */
    mng_set_suspensionmode( data->mng_handle, MNG_FALSE );
    
    /* set all the callbacks */
    if(
    (mng_setcb_openstream   (data->mng_handle, Openstream   ) != MNG_NOERROR) |
    (mng_setcb_closestream  (data->mng_handle, Closestream  ) != MNG_NOERROR) |
    (mng_setcb_readdata     (data->mng_handle, Readdata     ) != MNG_NOERROR) |
    (mng_setcb_processheader(data->mng_handle, ProcessHeader) != MNG_NOERROR) |
    (mng_setcb_getcanvasline(data->mng_handle, GetCanvasLine) != MNG_NOERROR) |
    (mng_setcb_refresh      (data->mng_handle, ImageRefresh ) != MNG_NOERROR) |
    (mng_setcb_gettickcount (data->mng_handle, GetTickCount ) != MNG_NOERROR) |
    (mng_setcb_settimer     (data->mng_handle, SetTimer     ) != MNG_NOERROR)
    )
    {
        D_ERROR("libmng reported an error setting a callback function!");
        mng_cleanup(&data->mng_handle);
        goto error;
    };
    
    mng_read(data->mng_handle);
    
    D_INFO("==========After mng_read()===============");

     direct_util_recursive_pthread_mutex_init( &data->lock );
     pthread_cond_init( &data->cond, NULL );
     
     thiz->AddRef                = IDirectFBVideoProvider_MNG_AddRef;
     thiz->Release               = IDirectFBVideoProvider_MNG_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_MNG_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_MNG_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_MNG_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_MNG_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_MNG_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_MNG_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_MNG_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_MNG_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_MNG_GetLength;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_MNG_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_MNG_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_MNG_GetSpeed;
     
     return DFB_OK;

error:
     buffer->Release( buffer );

     if (data->image)
          D_FREE( data->image );

     DIRECT_DEALLOCATE_INTERFACE(thiz);
     return DFB_FAILURE;
}
