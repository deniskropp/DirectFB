#ifndef __ImageProvider_includes_h__
#define __ImageProvider_includes_h__

#ifdef __cplusplus
extern "C" {
#endif


#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <core/CoreDFB_includes.h>

#include <directfb.h>


typedef struct {
     FusionCall              call;
} ImageProvider;

typedef struct {
     int                     magic;

     FusionCall              call;

     IDirectFB              *idirectfb;
     IDirectFBDataBuffer    *buffer;
     IDirectFBImageProvider *provider;

     CoreResourceCleanup    *cleanup;
} ImageProviderDispatch;

DFBResult ImageProviderDispatch_Create ( IDirectFB               *idirectfb,
                                         IDirectFBDataBuffer     *buffer,
                                         IDirectFBImageProvider  *provider,
                                         ImageProviderDispatch  **ret_dispatch );

void      ImageProviderDispatch_Destroy( ImageProviderDispatch   *dispatch );


static __inline__ DirectResult
ImageProvider_Call( ImageProvider       *provider,
                    FusionCallExecFlags  flags,
                    int                  call_arg,
                    void                *ptr,
                    unsigned int         length,
                    void                *ret_ptr,
                    unsigned int         ret_size,
                    unsigned int        *ret_length )
{
     return fusion_call_execute3( &provider->call,
                                  (FusionCallExecFlags)(dfb_config->call_nodirect | flags),
                                  call_arg, ptr, length, ret_ptr, ret_size, ret_length );
}


#ifdef __cplusplus
}
#endif


#endif

