/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <dlfcn.h>

#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <idirectfb.h>


#include "android_system.h"

D_DEBUG_DOMAIN( Android_FBO,     "Android/FBO",     "Android FBO Surface Pool" );
D_DEBUG_DOMAIN( Android_FBOLock, "Android/FBOLock", "Android FBO Surface Pool Locks" );

#define CHECK_GL_ERROR() {                             \
     int err = glGetError();                           \
     if (err) {                                        \
          D_ERROR("Android/FBO: GL_ERROR(%d)\n", err); \
          return DFB_INCOMPLETE;                       \
     }                                                 \
}

/**********************************************************************************************************************/

typedef void* buffer_handle_t;

struct hw_module_t;
struct hw_device_t;

typedef struct hw_module_methods_t {
     int (*open)(const struct hw_module_t* module, const char* id, struct hw_device_t** device);
} hw_module_methods_t;

typedef struct hw_module_t {
    /** tag must be initialized to HARDWARE_MODULE_TAG */
    uint32_t tag;

    /** major version number for the module */
    uint16_t version_major;

    /** minor version number of the module */
    uint16_t version_minor;

    /** Identifier of module */
    const char *id;

    /** Name of this module */
    const char *name;

    /** Author/owner/implementor of the module */
    const char *author;

    /** Modules methods */
    struct hw_module_methods_t* methods;

    /** module's dso */
    void* dso;

    /** padding to 128 bytes, reserved for future use */
    uint32_t reserved[32-7];

} hw_module_t;

typedef struct hw_device_t {
    /** tag must be initialized to HARDWARE_DEVICE_TAG */
    uint32_t tag;

    /** version number for hw_device_t */
    uint32_t version;

    /** reference to the module this device belongs to */
    struct hw_module_t* module;

    /** padding reserved for future use */
    uint32_t reserved[12];

    /** Close this device */
    int (*close)(struct hw_device_t* device);

} hw_device_t;

typedef struct alloc_device_t {
    struct hw_device_t common;

    /* 
     * (*alloc)() Allocates a buffer in graphic memory with the requested
     * parameters and returns a buffer_handle_t and the stride in pixels to
     * allow the implementation to satisfy hardware constraints on the width
     * of a pixmap (eg: it may have to be multiple of 8 pixels). 
     * The CALLER TAKES OWNERSHIP of the buffer_handle_t.
     * 
     * Returns 0 on success or -errno on error.
     */

    int (*alloc)(struct alloc_device_t* dev,
            int w, int h, int format, int usage,
            buffer_handle_t* handle, int* stride);

    /* 
     * (*free)() Frees a previously allocated buffer. 
     * Behavior is undefined if the buffer is still mapped in any process,
     * but shall not result in termination of the program or security breaches
     * (allowing a process to get access to another process' buffers).
     * THIS FUNCTION TAKES OWNERSHIP of the buffer_handle_t which becomes
     * invalid after the call.   
     * 
     * Returns 0 on success or -errno on error.
     */
    int (*free)(struct alloc_device_t* dev,
            buffer_handle_t handle);

    /* This hook is OPTIONAL.
     *
     * If non NULL it will be caused by SurfaceFlinger on dumpsys
     */
    void (*dump)(struct alloc_device_t *dev, char *buff, int buff_len);

    void* reserved_proc[7];
} alloc_device_t;

enum {
    /* buffer is never read in software */
    GRALLOC_USAGE_SW_READ_NEVER         = 0x00000000,
    /* buffer is rarely read in software */
    GRALLOC_USAGE_SW_READ_RARELY        = 0x00000002,
    /* buffer is often read in software */
    GRALLOC_USAGE_SW_READ_OFTEN         = 0x00000003,
    /* mask for the software read values */
    GRALLOC_USAGE_SW_READ_MASK          = 0x0000000F,
    
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_NEVER        = 0x00000000,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_RARELY       = 0x00000020,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_OFTEN        = 0x00000030,
    /* mask for the software write values */
    GRALLOC_USAGE_SW_WRITE_MASK         = 0x000000F0,

    /* buffer will be used as an OpenGL ES texture */
    GRALLOC_USAGE_HW_TEXTURE            = 0x00000100,
    /* buffer will be used as an OpenGL ES render target */
    GRALLOC_USAGE_HW_RENDER             = 0x00000200,
    /* buffer will be used by the 2D hardware blitter */
    GRALLOC_USAGE_HW_2D                 = 0x00000400,   
    /* buffer will be used by the HWComposer HAL module */
    GRALLOC_USAGE_HW_COMPOSER           = 0x00000800,
    /* buffer will be used with the framebuffer device */
    GRALLOC_USAGE_HW_FB                 = 0x00001000,
    /* buffer will be used with the HW video encoder */
    GRALLOC_USAGE_HW_VIDEO_ENCODER      = 0x00010000,  
    /* mask for the software usage bit-mask */
    GRALLOC_USAGE_HW_MASK               = 0x00011F00,

    /* buffer should be displayed full-screen on an external display when
     * possible
     */
    GRALLOC_USAGE_EXTERNAL_DISP         = 0x00002000,

    /* Must have a hardware-protected path to external display sink for
     * this buffer.  If a hardware-protected path is not available, then
     * either don't composite only this buffer (preferred) to the
     * external sink, or (less desirable) do not route the entire
     * composition to the external sink.
     */
    GRALLOC_USAGE_PROTECTED             = 0x00004000,

    /* implementation-specific private usage flags */
    GRALLOC_USAGE_PRIVATE_0             = 0x10000000,
    GRALLOC_USAGE_PRIVATE_1             = 0x20000000,
    GRALLOC_USAGE_PRIVATE_2             = 0x40000000,
    GRALLOC_USAGE_PRIVATE_3             = 0x80000000,
    GRALLOC_USAGE_PRIVATE_MASK          = 0xF0000000,
};

enum {
    HAL_PIXEL_FORMAT_RGBA_8888          = 1,
    HAL_PIXEL_FORMAT_RGBX_8888          = 2,
    HAL_PIXEL_FORMAT_RGB_888            = 3,
    HAL_PIXEL_FORMAT_RGB_565            = 4,
    HAL_PIXEL_FORMAT_BGRA_8888          = 5,
    HAL_PIXEL_FORMAT_RGBA_5551          = 6,
    HAL_PIXEL_FORMAT_RGBA_4444          = 7,

    /* 0x8 - 0xFF range unavailable */

    /*
     * 0x100 - 0x1FF
     *
     * This range is reserved for pixel formats that are specific to the HAL
     * implementation.  Implementations can use any value in this range to
     * communicate video pixel formats between their HAL modules.  These formats
     * must not have an alpha channel.  Additionally, an EGLimage created from a
     * gralloc buffer of one of these formats must be supported for use with the
     * GL_OES_EGL_image_external OpenGL ES extension.
     */

    /*
     * Android YUV format:
     *
     * This format is exposed outside of the HAL to software decoders and
     * applications.  EGLImageKHR must support it in conjunction with the
     * OES_EGL_image_external extension.
     *
     * YV12 is a 4:2:0 YCrCb planar format comprised of a WxH Y plane followed
     * by (W/2) x (H/2) Cr and Cb planes.
     *
     * This format assumes
     * - an even width 
     * - an even height
     * - a horizontal stride multiple of 16 pixels
     * - a vertical stride equal to the height
     *
     *   y_size = stride * height
     *   c_size = ALIGN(stride/2, 16) * height/2
     *   size = y_size + c_size * 2
     *   cr_offset = y_size
     *   cb_offset = y_size + c_size
     * 
     */
    HAL_PIXEL_FORMAT_YV12   = 0x32315659, // YCrCb 4:2:0 Planar



    /* Legacy formats (deprecated), used by ImageFormat.java */
    HAL_PIXEL_FORMAT_YCbCr_422_SP       = 0x10, // NV16
    HAL_PIXEL_FORMAT_YCrCb_420_SP       = 0x11, // NV21
    HAL_PIXEL_FORMAT_YCbCr_422_I        = 0x14, // YUY2
};

typedef struct android_native_base_t
{
    /* a magic value defined by the actual EGL native type */
    int magic;

    /* the sizeof() of the actual EGL native type */
    int version;

    void* reserved[4];

    /* reference-counting interface */
    void (*incRef)(struct android_native_base_t* base);
    void (*decRef)(struct android_native_base_t* base);
} android_native_base_t;

typedef struct ANativeWindowBuffer
{
#ifdef __cplusplus
    ANativeWindowBuffer() {
        common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
        common.version = sizeof(ANativeWindowBuffer);
        memset(common.reserved, 0, sizeof(common.reserved));
    }

    // Implement the methods that sp<ANativeWindowBuffer> expects so that it
    // can be used to automatically refcount ANativeWindowBuffer's.
    void incStrong(const void* id) const {
        common.incRef(const_cast<android_native_base_t*>(&common));
    }
    void decStrong(const void* id) const {
        common.decRef(const_cast<android_native_base_t*>(&common));
    }
#endif

    struct android_native_base_t common;

    int width;
    int height;
    int stride;
    int format;
    int usage; 

    void* reserved[2];

    buffer_handle_t handle;

    void* reserved_proc[8];
} ANativeWindowBuffer_t;

typedef struct gralloc_module_t {
    struct hw_module_t common;
    
    /*
     * (*registerBuffer)() must be called before a buffer_handle_t that has not
     * been created with (*alloc_device_t::alloc)() can be used.
     * 
     * This is intended to be used with buffer_handle_t's that have been
     * received in this process through IPC.
     * 
     * This function checks that the handle is indeed a valid one and prepares
     * it for use with (*lock)() and (*unlock)().
     * 
     * It is not necessary to call (*registerBuffer)() on a handle created 
     * with (*alloc_device_t::alloc)().
     * 
     * returns an error if this buffer_handle_t is not valid.
     */
    int (*registerBuffer)(struct gralloc_module_t const* module,
            buffer_handle_t handle);

    /*
     * (*unregisterBuffer)() is called once this handle is no longer needed in
     * this process. After this call, it is an error to call (*lock)(),
     * (*unlock)(), or (*registerBuffer)().
     * 
     * This function doesn't close or free the handle itself; this is done
     * by other means, usually through libcutils's native_handle_close() and
     * native_handle_free(). 
     * 
     * It is an error to call (*unregisterBuffer)() on a buffer that wasn't
     * explicitly registered first.
     */
    int (*unregisterBuffer)(struct gralloc_module_t const* module,
            buffer_handle_t handle);
    
    /*
     * The (*lock)() method is called before a buffer is accessed for the 
     * specified usage. This call may block, for instance if the h/w needs
     * to finish rendering or if CPU caches need to be synchronized.
     * 
     * The caller promises to modify only pixels in the area specified 
     * by (l,t,w,h).
     * 
     * The content of the buffer outside of the specified area is NOT modified
     * by this call.
     *
     * If usage specifies GRALLOC_USAGE_SW_*, vaddr is filled with the address
     * of the buffer in virtual memory.
     *
     * THREADING CONSIDERATIONS:
     *
     * It is legal for several different threads to lock a buffer from 
     * read access, none of the threads are blocked.
     * 
     * However, locking a buffer simultaneously for write or read/write is
     * undefined, but:
     * - shall not result in termination of the process
     * - shall not block the caller
     * It is acceptable to return an error or to leave the buffer's content
     * into an indeterminate state.
     *
     * If the buffer was created with a usage mask incompatible with the
     * requested usage flags here, -EINVAL is returned. 
     * 
     */
    
    int (*lock)(struct gralloc_module_t const* module,
            buffer_handle_t handle, int usage,
            int l, int t, int w, int h,
            void** vaddr);

    
    /*
     * The (*unlock)() method must be called after all changes to the buffer
     * are completed.
     */
    
    int (*unlock)(struct gralloc_module_t const* module,
            buffer_handle_t handle);


    /* reserved for future use */
    int (*perform)(struct gralloc_module_t const* module,
            int operation, ... );

    /* reserved for future use */
    void* reserved_proc[7];
} gralloc_module_t;

static void incRef(struct android_native_base_t* base)
{
}

static void decRef(struct android_native_base_t* base)
{
}

typedef int(*HW_GET_MODULE)( const char *, const hw_module_t **);

#define ANDROID_NATIVE_MAKE_CONSTANT(a,b,c,d) \
    (((unsigned)(a)<<24)|((unsigned)(b)<<16)|((unsigned)(c)<<8)|(unsigned)(d))

#define ANDROID_NATIVE_BUFFER_MAGIC \
    ANDROID_NATIVE_MAKE_CONSTANT('_','b','f','r')

#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2

/**********************************************************************************************************************/

typedef struct {
     int             magic;
} FBOPoolData;

typedef struct {
     int                  magic;

     CoreDFB             *core;

     AndroidData         *data;
} FBOPoolLocalData;

typedef struct {
     int                    magic;

     int                    pitch;
     int                    size;

     EGLImageKHR            image;

     GLuint                 texture;
     GLuint                 fbo;
     GLuint                 color_rb;
     GLuint                 depth_rb;

     ANativeWindowBuffer_t *win_buf;
     const hw_module_t     *hw_mod;
     gralloc_module_t      *gralloc_mod;
     alloc_device_t        *alloc_mod;
} FBOAllocationData;

/**********************************************************************************************************************/

static ANativeWindowBuffer_t *
AndroidAllocNativeBuffer( FBOAllocationData *alloc, int width, int height )
{
     void *hw_handle = dlopen( "/system/lib/libhardware.so", RTLD_LAZY );
     if (!hw_handle) {
          D_ERROR( "DirectFB/EGL: dlopen failed (%d)\n", errno );
          return NULL;
     }

     HW_GET_MODULE hw_get_module = dlsym( hw_handle, "hw_get_module" );
     if (!hw_get_module)  {
          D_ERROR( "DirectFB/EGL: dlsym failed (%d)\n", errno );
          dlclose( hw_handle );
          return NULL;
     }

     dlclose( hw_handle );

     int err = (*hw_get_module)( "gralloc", &alloc->hw_mod );
     if (err || !alloc->hw_mod) {
          D_ERROR( "DirectFB/EGL: hw_get_module failed (%d)\n", err );
          return NULL;
     }

     alloc->hw_mod->methods->open( alloc->hw_mod, "gpu0", (struct hw_device_t**)&alloc->alloc_mod );
     if (!alloc->alloc_mod) {
          D_ERROR( "DirectFB/EGL: open alloc failed\n");
          return NULL;
     }

     buffer_handle_t buf_handle = NULL;
     int stride = 0;
     int usage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN;
     int format = HAL_PIXEL_FORMAT_BGRA_8888;

     alloc->alloc_mod->alloc( alloc->alloc_mod, width, height, format, usage, &buf_handle, &stride );
     if (!buf_handle) {
          D_ERROR( "DirectFB/EGL: failed to alloc buffer\n");
          return NULL;
     }

     ANativeWindowBuffer_t *wbuf = (ANativeWindowBuffer_t *)malloc( sizeof(ANativeWindowBuffer_t) );
     wbuf->common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
     wbuf->common.version = sizeof(ANativeWindowBuffer_t);
     memset(wbuf->common.reserved, 0, sizeof(wbuf->common.reserved));
     wbuf->width = width;
     wbuf->height = height;
     wbuf->stride = stride;
     wbuf->format = format;
     wbuf->common.incRef = incRef;
     wbuf->common.decRef = decRef;
     wbuf->usage = usage;
     wbuf->handle = buf_handle;

     alloc->win_buf = wbuf;
     alloc->gralloc_mod = (gralloc_module_t *)alloc->hw_mod;

     return wbuf;
}

/**********************************************************************************************************************/

static inline bool
TestEGLError( const char* pszLocation )
{
     EGLint iErr = eglGetError();
     if (iErr != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: %s failed (%d).\n", pszLocation, iErr );
          return false;
     }
     D_INFO("############################ open success!\n");
     return true;
}

/**********************************************************************************************************************/

static int
fboPoolDataSize( void )
{
     return sizeof(FBOPoolData);
}

static int
fboPoolLocalDataSize( void )
{
     return sizeof(FBOPoolLocalData);
}

static int
fboAllocationDataSize( void )
{
     return sizeof(FBOAllocationData);
}

static DFBResult
fboInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     AndroidData      *android_data = system_data;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;
     ret_desc->types             = CSTF_WINDOW | CSTF_LAYER | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_ULTIMATE;
     ret_desc->size              = 0;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "FBO Pool" );

     local->core = core;
     local->data = android_data;

     D_MAGIC_SET( data, FBOPoolData );
     D_MAGIC_SET( local, FBOPoolLocalData );

     return DFB_OK;
}

static DFBResult
fboJoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     AndroidData      *android_data = system_data;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;
     local->data = android_data;

     D_MAGIC_SET( local, FBOPoolLocalData );

     return DFB_OK;
}

static DFBResult
fboDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fboLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fboTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     DFBResult         ret = DFB_OK;
     CoreSurface      *surface;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );


     D_DEBUG_AT( Android_FBO, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
checkFramebufferStatus( void )
{
     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

     switch (status) {
          case 0:
               D_WARN( "zero status" );
          case GL_FRAMEBUFFER_COMPLETE:
               return DFB_OK;

          case GL_FRAMEBUFFER_UNSUPPORTED:
               D_ERROR( "%s(): Unsupported!\n", __FUNCTION__);
               return DFB_UNSUPPORTED;

          case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
               D_ERROR( "%s(): Incomplete attachment!\n", __FUNCTION__);
               return DFB_INVARG;

          case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
               D_ERROR( "%s(): Incomplete missing attachment!\n", __FUNCTION__);
               return DFB_INVARG;

          case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
               D_ERROR( "%s(): Incomplete dimensions!\n", __FUNCTION__);
               return DFB_INVARG;

          //case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
          //     D_ERROR( "%s(): Incomplete formats!\n", __FUNCTION__);
          //     return DFB_INVARG;

          default:
               D_ERROR( "%s(): Failure! (0x%04x)\n", __FUNCTION__, status );
               return DFB_FAILURE;
     }
}

static DFBResult
fboAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface       *surface;
     FBOPoolData       *data  = pool_data;
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     D_INFO("FBO %dx%d\n", buffer->config.size.w, buffer->config.size.h);

     ANativeWindowBuffer_t *buf = AndroidAllocNativeBuffer( alloc, buffer->config.size.w, buffer->config.size.h );

     CHECK_GL_ERROR();
     EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
     alloc->image = eglCreateImageKHR( eglGetDisplay( EGL_DEFAULT_DISPLAY ), EGL_NO_CONTEXT,
                                       EGL_NATIVE_BUFFER_ANDROID, buf, eglImgAttrs );
     CHECK_GL_ERROR();

     int tex, fbo;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
     CHECK_GL_ERROR();
     
     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     CHECK_GL_ERROR();
     
     glGenTextures( 1, &alloc->texture );
     CHECK_GL_ERROR();

     glBindTexture( GL_TEXTURE_2D, alloc->texture );
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
     CHECK_GL_ERROR();

     glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, alloc->image );
     CHECK_GL_ERROR();

     glGenRenderbuffers( 1, &alloc->depth_rb );
     CHECK_GL_ERROR();

     /* Update depth buffer */
     glBindRenderbuffer( GL_RENDERBUFFER, alloc->depth_rb );
     CHECK_GL_ERROR();

     /*
      * Color Render Buffer
      */
     glGenRenderbuffers( 1, &alloc->color_rb );
     CHECK_GL_ERROR();

     glBindRenderbuffer( GL_RENDERBUFFER, alloc->color_rb );
     CHECK_GL_ERROR();

     glEGLImageTargetRenderbufferStorageOES( GL_RENDERBUFFER, alloc->image );
     CHECK_GL_ERROR();

     /*
      * Framebuffer
      */
     glGenFramebuffers( 1, &alloc->fbo );
     CHECK_GL_ERROR();

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
     CHECK_GL_ERROR();

     glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, alloc->color_rb );
     CHECK_GL_ERROR();

     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
          D_ERROR( "DirectFB/Mesa: Framebuffer not complete\n" );
     }

     checkFramebufferStatus();

     glBindFramebuffer( GL_FRAMEBUFFER, fbo );
     glBindTexture( GL_TEXTURE_2D, tex );

     allocation->size = alloc->size;

     D_MAGIC_SET( alloc, FBOAllocationData );

     return DFB_OK;
}

static DFBResult
fboDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     FBOPoolData       *data  = pool_data;
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) data;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     glDeleteTextures( 1, &alloc->texture );
     glDeleteFramebuffers( 1, &alloc->fbo );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
fboMuckOut( CoreSurfacePool   *pool,
            void              *pool_data,
            void              *pool_local,
            CoreSurfaceBuffer *buffer )
{
     CoreSurface      *surface;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     return DFB_UNSUPPORTED;
}

static DFBResult
fboLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_FBOLock, "%s( %p, accessor 0x%02x, access 0x%02x )\n",
                 __FUNCTION__, lock->buffer, lock->accessor, lock->access );

//     if (!dfb_core_is_master(local->core))
//          return DFB_UNSUPPORTED;

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_CPU:
               if (alloc->gralloc_mod->lock(alloc->gralloc_mod, alloc->win_buf->handle, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0, allocation->config.size.w, allocation->config.size.h, &lock->addr))
                    return DFB_ACCESSDENIED;
               break;
          case CSAID_GPU:
          case CSAID_LAYER0:
               if (lock->access & CSAF_WRITE) {
                    if (allocation->type & CSTF_LAYER) {
                         lock->handle = NULL;

                         glBindFramebuffer( GL_FRAMEBUFFER, 0 );
                    }
                    else {
                         lock->handle = (void*) (long) alloc->fbo;
     
                         glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
                    }
               }
               else {
                    lock->handle = (void*) (long) alloc->texture;
               }
               break;

          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               return DFB_BUG;
     }

     D_DEBUG_AT( Android_FBOLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
fboUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     FBOAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     switch (lock->accessor) {
          case CSAID_CPU:
               alloc->gralloc_mod->unlock(alloc->gralloc_mod, alloc->win_buf->handle);
               break;
          case CSAID_GPU:
               if (lock->access & CSAF_WRITE) {
                    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
               }
               break;

          default:
               break;
     }

     return DFB_OK;
}

static DFBResult
fboRead( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         void                  *destination,
         int                    pitch,
         const DFBRectangle    *rect )
{
     FBOAllocationData *alloc = alloc_data;
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     (void) alloc;

/*
     buff = (GLuint *)D_MALLOC(rect->w * rect->h * 4);
     if (!buff) {
          D_ERROR("EGL: failed to allocate %d bytes for texture download!\n",
                  rect->w * rect->h);
          return D_OOM();
     }
*/

     int fbo;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glReadPixels(rect->x, rect->y,
                  rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, destination);

     glBindFramebuffer( GL_FRAMEBUFFER, fbo );

/*
     pixels_per_line = pitch/4;

     sline = buff;
     dline = (GLuint *)destination + rect->x + (rect->y * pixels_per_line);

     h = rect->h;
     while (h--) {
          s = sline;
          d = dline;
          w = rect->w;
          while (w--) {
               pixel = *s++;
               *d++ = (pixel & 0xff00ff00) |
                      ((pixel >> 16) & 0x000000ff) |
                      ((pixel << 16) & 0x00ff0000);
          }
          sline += rect->w;
          dline += pixels_per_line;
     }

     D_FREE(buff);
*/
     return DFB_OK;
}

static DFBResult
fboWrite( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          const void            *source,
          int                    pitch,
          const DFBRectangle    *rect )
{
     FBOAllocationData *alloc = alloc_data;
     CoreSurface       *surface;
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );


     EGLint err;

     int tex;
return DFB_OK;
     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

/*
     buff = (GLuint *)D_MALLOC(rect->w * rect->h * 4);
     if (!buff) {
          D_ERROR("EGL: failed to allocate %d bytes for texture upload!\n",
                  rect->w * rect->h * 4);
          return D_OOM();
     }

     pixels_per_line = pitch/4;

     sline = (GLuint *)source + rect->x + (rect->y * pixels_per_line);
     dline = buff;

     h = rect->h;
     while (h--) {
          s = sline;
          d = dline;
          w = rect->w;
          while (w--) {
               pixel = *s++;
               *d++ = (pixel & 0xff00ff00) |
                      ((pixel >> 16) & 0x000000ff) |
                      ((pixel << 16) & 0x00ff0000);
          }
          sline += pixels_per_line;
          dline += rect->w;
     }
*/
     //glTexSubImage2D(GL_TEXTURE_2D, 0,
     //                rect->x, rect->y,
     //                rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, buff);
     // glTexImage2D(GL_TEXTURE_2D, 0,
     //              GL_RGBA, rect->w, rect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff);
     glPixelStorei( GL_UNPACK_ALIGNMENT, 8);
     glTexImage2D(GL_TEXTURE_2D, 0,
                  GL_RGBA, rect->w, rect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);


     //D_FREE(buff);



//     glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, allocation->config.size.w, allocation->config.size.h, GL_RGBA, GL_UNSIGNED_BYTE, source );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/PVR2D: glTexSubImage2D() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }


     glBindTexture( GL_TEXTURE_2D, tex );


     return DFB_OK;
}

const SurfacePoolFuncs androidSurfacePoolFuncs = {
     PoolDataSize:       fboPoolDataSize,
     PoolLocalDataSize:  fboPoolLocalDataSize,
     AllocationDataSize: fboAllocationDataSize,

     InitPool:           fboInitPool,
     JoinPool:           fboJoinPool,
     DestroyPool:        fboDestroyPool,
     LeavePool:          fboLeavePool,

     TestConfig:         fboTestConfig,
     AllocateBuffer:     fboAllocateBuffer,
     DeallocateBuffer:   fboDeallocateBuffer,
     MuckOut:            fboMuckOut,

     Lock:               fboLock,
     Unlock:             fboUnlock,

     Read:               fboRead,
     Write:              fboWrite,
};

const SurfacePoolFuncs *fboSurfacePoolFuncs = &androidSurfacePoolFuncs;

