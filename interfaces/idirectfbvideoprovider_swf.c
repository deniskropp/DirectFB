/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de> and
              Joachim Steiger <roh@convergence.de>.


   The contents of this software are proprietary and confidential to
   convergence integrated media GmbH. Use of this information is
   to be in accordance with the terms of the license agreement you
   entered into with convergence integrated media GmbH. Individuals
   having access to this software are responsible for maintaining the
   confidentiality of the content and for keeping the software secure
   when not in use. Transfer to any party is strictly forbidden other
   than as expressly permitted in writing by convergence integrated
   media GmbH.  Unauthorized transfer to or possession by any
   unauthorized party may be a criminal offense.

   convergence integrated media GmbH MAKES NO WARRANTIES EITHER
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
   OR NON-INFRINGEMENT.
   convergence integrated media GmbH SHALL NOT BE LIABLE FOR ANY
   DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
   DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.

   convergence integrated media GmbH
   Rosenthaler Str.51
   D-10178 Berlin / Germany
*/

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include <pthread.h>

#include <flash.h>

#include <directfb.h>

#include "misc/util.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/layers.h"
#include "core/gfxcard.h"

#include "display/idirectfbsurface.h"

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                  ref;       /* reference counter */
     FlashHandle          flashHandle;
     struct FlashInfo     flashInfo;
     struct FlashDisplay  flashDisplay;
     pthread_t            thread;

     IDirectFBSurface    *destination;
     DFBRectangle         dest_rect;

     DVFrameCallback      callback;
     void                *ctx;

     CardState            state;
     CoreSurface         *source;
} IDirectFBVideoProvider_Swf_data;


//------------------------------------
int
readFile (const char *filename, char **buffer, long *size)
{
    FILE *in;
    char *buf;
    long length;
    
    in = fopen (filename, "r");
    if (in == 0)
    {
      perror (filename);
      return -1;
    }        
    fseek (in, 0, SEEK_END);
    length = ftell (in);
    rewind (in);
    (int *) buf = malloc (length);
    fread (buf, length, 1, in);
    fclose (in);
    *size = length;
    *buffer = buf;
    return length;
}

void
showUrl (char *url, char *target, void *client_data)
{
  printf ("SWF GetURL : %s\n", url);
}

void
getSwf (char *url, int level, void *client_data)
{
  FlashHandle flashHandle;
  char *buffer;
  long size;
  
  flashHandle = (FlashHandle) client_data;
  printf ("SWF LoadMovie: %s @ %d\n", url, level);
  if (readFile (url, &buffer, &size) > 0)
  {
    FlashParse (flashHandle, level, buffer, size);
  }
}


static void* FrameThread( void *ctx )
{
  IDirectFBVideoProvider_Swf_data *data = (IDirectFBVideoProvider_Swf_data*)ctx;
  struct timeval wd2,now,tv;
  long           cmd;
  long           wakeUp;
  long delay = 0;
   
      cmd = FLASH_WAKEUP;
      wakeUp = FlashExec (data->flashHandle, cmd, 0, &wd2);
   
  while (1) {

    pthread_testcancel();


    gettimeofday (&now, 0);
    delay = (wd2.tv_sec - now.tv_sec) * 1000 + (wd2.tv_usec - now.tv_usec) / 1000;

    if (delay < 0)
      delay = 20;
    
    if (data->flashDisplay.flash_refresh)
    { 
      DFBRectangle   rect, drect;

      rect.x=0;
      rect.y=0;
      rect.w=(int) data->flashInfo.frameWidth / 20;
      rect.h=(int) data->flashInfo.frameHeight / 20;

      drect = data->dest_rect;

      gfxcard_stretchblit( &rect, &drect, &data->state );
      data->flashDisplay.flash_refresh = 0;
      
      if (data->callback)
        data->callback (data->ctx);
    }
    
  if (wakeUp)
    {
      tv.tv_sec = 0;
      tv.tv_usec = delay * 1000;
      select( 0, 0, 0, 0, &tv );
      
      cmd = FLASH_WAKEUP;
      wakeUp = FlashExec (data->flashHandle, cmd, 0, &wd2);
    }
    else
      return NULL;
  }
}

//------------------------------------------
static 
void IDirectFBVideoProvider_Swf_Destruct(IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swf_data *data;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;
     
     if (data->thread != -1)
     {
       pthread_cancel( data->thread );
       pthread_join( data->thread, NULL );
       data->thread = -1;
     }
     
     FlashClose(data->flashHandle);
    
     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

static DFBResult IDirectFBVideoProvider_Swf_AddRef(IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_Release(IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBVideoProvider_Swf_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_GetSurfaceDescription(
                                          IDirectFBVideoProvider *thiz,
                                          DFBSurfaceDescription  *desc )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz || !desc)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     memset( desc, 0, sizeof(DFBSurfaceDescription) );
     desc->flags = (DFBSurfaceDescriptionFlags)
       (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);

     desc->width  = (int) data->flashInfo.frameWidth / 20;
     desc->height = (int) data->flashInfo.frameHeight / 20;
     desc->pixelformat = layers->surface->format;

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Swf_PlayTo(
                                           IDirectFBVideoProvider *thiz,
                                           IDirectFBSurface       *destination,
                                           DFBRectangle           *dstrect,
                                           DVFrameCallback         callback,
                                           void                   *ctx )
{
     DFBRectangle                         rect;
     IDirectFBVideoProvider_Swf_data     *data;
     IDirectFBSurface_data               *dst_data;

     if (!thiz || !destination)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;
     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!data || !dst_data)
          return DFB_DEAD;


     /* build the destination rectangle */
     if (dstrect) {
          if (dstrect->w < 1  ||  dstrect->h < 1)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->req_rect.x;
          rect.y += dst_data->req_rect.y;
     }
     else
          rect = dst_data->req_rect;

     /* save for later blitting operation */
     data->dest_rect = rect;


     /* build the clip rectangle */
     if (!rectangle_intersect( &rect, &dst_data->clip_rect ))
          return DFB_INVARG;

     /* put the destination clip into the state */
     data->state.clip.x1 = rect.x;
     data->state.clip.y1 = rect.y;
     data->state.clip.x2 = rect.x + rect.w - 1;
     data->state.clip.y2 = rect.y + rect.h - 1;
     data->state.destination = dst_data->surface;
     data->state.modified = (StateModificationFlags)
                            (data->state.modified | SMF_CLIP | SMF_DESTINATION);


     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }
     
     destination->AddRef( destination );
     data->destination = destination;   /* FIXME: install listener */


     data->callback = callback;
     data->ctx = ctx;


     if (data->thread == -1)
       pthread_create( &data->thread, NULL, FrameThread, data );
     
     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Swf_Stop(IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->thread != -1)
     {
       pthread_cancel( data->thread );
       pthread_join( data->thread, NULL );
       data->thread = -1;
     }
       
     
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_SeekTo(
                                              IDirectFBVideoProvider *thiz,
                                              double                  seconds )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_GetPos(
     IDirectFBVideoProvider *thiz,
     double                 *seconds )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_GetLength(
     IDirectFBVideoProvider *thiz,
     double                 *seconds )
{
     IDirectFBVideoProvider_Swf_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}


/* exported symbols */

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "Swf";
}

DFBResult Probe( const char *filename )
{
    if (strstr( filename, ".swf" ) ||
        strstr( filename, ".SWF" ))
      return DFB_OK;
     
    return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     IDirectFBVideoProvider_Swf_data *data;
     char *buffer;
     long size;
     int status;

     data = (IDirectFBVideoProvider_Swf_data*)
     
     malloc( sizeof(IDirectFBVideoProvider_Swf_data) );
     memset( data, 0, sizeof(IDirectFBVideoProvider_Swf_data) );
     thiz->priv = data;

     data->ref = 1;

     if (readFile (filename, &buffer, &size) < 0)
     {
       printf( "DirectFB/Swf: Loading Swf file failed.\n");
       free( data );
       return DFB_FAILURE;
     }
     
     data->flashHandle = FlashNew();
     if (data->flashHandle == 0)
     {
       printf( "DirectFB/Swf: Creation of Swfplayer failed.\n");
       free( data );
       return DFB_FAILURE;
     }

     do
     {
       status = FlashParse (data->flashHandle, 0, buffer, size);
     }
     while (status & FLASH_PARSE_NEED_DATA);
     free(buffer);

     FlashGetInfo (data->flashHandle, &data->flashInfo);
     
     surface_create( (int) data->flashInfo.frameWidth  / 20,
                     (int) data->flashInfo.frameHeight / 20,
                     DSPF_RGB16,
                     CSP_SYSTEMONLY,
                     DSCAPS_SYSTEMONLY,
                     &(data->source));

     data->flashDisplay.pixels = data->source->back_buffer->system.addr;      //ptr
     data->flashDisplay.bpl    = data->source->back_buffer->system.pitch; //pitch
     data->flashDisplay.width = data->source->width;
     data->flashDisplay.height = data->source->height;
     data->flashDisplay.depth = 16;
     data->flashDisplay.bpp = 2;
    
     data->thread = -1;
     
/*
     pthread_mutex_init( &data->source.front_lock, NULL );
     pthread_mutex_init( &data->source.back_lock, NULL );
     pthread_mutex_init( &data->source.listeners_mutex, NULL );
*/     
     data->state.source   = data->source;
     data->state.modified = SMF_ALL;
     
     FlashGraphicInit (data->flashHandle, &data->flashDisplay);
//     FlashSoundInit(data->flashHandle, "/dev/dsp");
     FlashSetGetUrlMethod (data->flashHandle, showUrl, 0);
     FlashSetGetSwfMethod (data->flashHandle, getSwf, (void *) data->flashHandle);
     

     thiz->AddRef                = IDirectFBVideoProvider_Swf_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Swf_Release;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_Swf_GetSurfaceDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_Swf_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Swf_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_Swf_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Swf_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Swf_GetLength;

     return DFB_OK;
}
