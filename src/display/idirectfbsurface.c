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
#include <unistd.h>
#include <string.h>
#include <alloca.h>

#include <math.h>


#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/fonts.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <core/CoreDFB.h>
#include <core/CoreGraphicsState.h>
#include <core/CoreSurface.h>
#include <core/CoreSurfaceClient.h>

#include <media/idirectfbfont.h>

#include <display/idirectfbsurface.h>
#include <display/idirectfbpalette.h>

#include <input/idirectfbinputbuffer.h>

#include <misc/util.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>


D_DEBUG_DOMAIN( Surface, "IDirectFBSurface", "IDirectFBSurface Interface" );

/**********************************************************************************************************************/

static ReactionResult IDirectFBSurface_listener( const void *msg_data, void *ctx );

/**********************************************************************************************************************/

static DFBResult
register_prealloc( IDirectFBSurface_data *data )
{
     DFBResult     ret;
     unsigned int  i;
     CoreSurface  *surface = data->surface;

     if (surface->config.caps & DSCAPS_TRIPLE)
          data->memory_permissions_count = 3;
     else if (surface->config.caps & DSCAPS_DOUBLE)
          data->memory_permissions_count = 2;
     else
          data->memory_permissions_count = 1;

     for (i=0; i<data->memory_permissions_count; i++) {
          ret = dfb_core_memory_permissions_add( data->core, CMPF_READ | CMPF_WRITE,
                                                 surface->config.preallocated[i].addr,
                                                 surface->config.preallocated[i].pitch *
                                                 DFB_PLANE_MULTIPLY( surface->config.format, surface->config.size.h ),
                                                 &data->memory_permissions[i] );
          if (ret)
               goto error;
     }

     return DFB_OK;


error:
     for (--i; i>=0; i--)
          dfb_core_memory_permissions_remove( data->core, data->memory_permissions[i] );

     data->memory_permissions_count = 0;

     return ret;
}

static DFBResult
unregister_prealloc( IDirectFBSurface_data *data )
{
     unsigned int i;

     for (i=0; i<data->memory_permissions_count; i++)
          dfb_core_memory_permissions_remove( data->core, data->memory_permissions[i] );

     data->memory_permissions_count = 0;

     return DFB_OK;
}

/**********************************************************************************************************************/

void
IDirectFBSurface_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data;
     IDirectFBSurface      *parent;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;

     D_ASSERT( data != NULL );
     D_ASSERT( data->children_data == NULL );

     if (data->surface_client)
          dfb_surface_client_unref( data->surface_client );

     CoreDFB_WaitIdle( data->core );

     unregister_prealloc( data );

     parent = data->parent;
     if (parent) {
          IDirectFBSurface_data *parent_data;

          D_MAGIC_ASSERT( (IAny*) parent, DirectInterface );

          parent_data = parent->priv;
          D_ASSERT( parent_data != NULL );

          pthread_mutex_lock( &parent_data->children_lock );

          direct_list_remove( &parent_data->children_data, &data->link );

          pthread_mutex_unlock( &parent_data->children_lock );
     }

     if (data->surface)
          dfb_surface_detach( data->surface, &data->reaction );

     CoreGraphicsStateClient_Deinit( &data->state_client );

     dfb_state_stop_drawing( &data->state );

     dfb_state_set_destination( &data->state, NULL );
     dfb_state_set_source( &data->state, NULL );
     dfb_state_set_source_mask( &data->state, NULL );
     dfb_state_set_source2( &data->state, NULL );

     dfb_state_destroy( &data->state );

     if (data->font)
          data->font->Release( data->font );

     if (data->surface) {
          if (data->locked)
               dfb_surface_unlock_buffer( data->surface, &data->lock );

          dfb_surface_unref( data->surface );
     }

     pthread_mutex_destroy( &data->children_lock );

     direct_waitqueue_deinit( &data->back_buffer_wq );
     direct_mutex_deinit( &data->back_buffer_lock );

     direct_mutex_deinit( &data->surface_client_lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     if (parent)
          parent->Release( parent );
}

static DirectResult
IDirectFBSurface_AddRef( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBSurface_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBSurface_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IDirectFBSurface_GetPixelFormat( IDirectFBSurface      *thiz,
                                 DFBSurfacePixelFormat *format )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!format)
          return DFB_INVARG;

     *format = data->surface->config.format;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetColorSpace( IDirectFBSurface      *thiz,
                                DFBSurfaceColorSpace  *colorspace )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!colorspace)
          return DFB_INVARG;

     *colorspace = data->surface->config.colorspace;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetAccelerationMask( IDirectFBSurface    *thiz,
                                      IDirectFBSurface    *source,
                                      DFBAccelerationMask *ret_mask )
{
     DFBAccelerationMask mask = DFXL_NONE;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_mask)
          return DFB_INVARG;

     dfb_state_lock( &data->state );

     /* Check drawing functions */
     if (dfb_gfxcard_state_check( &data->state, DFXL_FILLRECTANGLE ))
          mask |= DFXL_FILLRECTANGLE;

     if (dfb_gfxcard_state_check( &data->state, DFXL_DRAWRECTANGLE ))
          mask |= DFXL_DRAWRECTANGLE;

     if (dfb_gfxcard_state_check( &data->state, DFXL_DRAWLINE ))
          mask |= DFXL_DRAWLINE;

     if (dfb_gfxcard_state_check( &data->state, DFXL_FILLTRIANGLE ))
          mask |= DFXL_FILLTRIANGLE;

     if (dfb_gfxcard_state_check( &data->state, DFXL_FILLTRAPEZOID ))
          mask |= DFXL_FILLTRAPEZOID;

     dfb_state_unlock( &data->state );

     /* Check blitting functions */
     if (source) {
          IDirectFBSurface_data *src_data = source->priv;

          dfb_state_set_source( &data->state, src_data->surface );
          dfb_state_set_source2( &data->state, data->surface ); // FIXME

          dfb_state_lock( &data->state );

          if (dfb_gfxcard_state_check( &data->state, DFXL_BLIT ))
               mask |= DFXL_BLIT;

          if (dfb_gfxcard_state_check( &data->state, DFXL_BLIT2 ))
               mask |= DFXL_BLIT2;

          if (dfb_gfxcard_state_check( &data->state, DFXL_STRETCHBLIT ))
               mask |= DFXL_STRETCHBLIT;

          if (dfb_gfxcard_state_check( &data->state, DFXL_TEXTRIANGLES ))
               mask |= DFXL_TEXTRIANGLES;

          dfb_state_unlock( &data->state );
     }

     /* Check text rendering function */
     if (data->font) {
          IDirectFBFont_data *font_data = data->font->priv;

          if (dfb_gfxcard_drawstring_check_state( font_data->font, &data->state ))
               mask |= DFXL_DRAWSTRING;
     }

     *ret_mask = mask;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPosition( IDirectFBSurface *thiz,
                          int              *x,
                          int              *y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!x && !y)
          return DFB_INVARG;

     if (x)
          *x = data->area.wanted.x;

     if (y)
          *y = data->area.wanted.y;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSize( IDirectFBSurface *thiz,
                          int              *width,
                          int              *height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->area.wanted.w;

     if (height)
          *height = data->area.wanted.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetVisibleRectangle( IDirectFBSurface *thiz,
                                      DFBRectangle     *rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!rect)
          return DFB_INVARG;

     rect->x = data->area.current.x - data->area.wanted.x;
     rect->y = data->area.current.y - data->area.wanted.y;
     rect->w = data->area.current.w;
     rect->h = data->area.current.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetCapabilities( IDirectFBSurface       *thiz,
                                  DFBSurfaceCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!caps)
          return DFB_INVARG;

     *caps = data->caps;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPalette( IDirectFBSurface  *thiz,
                             IDirectFBPalette **interface )
{
     DFBResult         ret;
     CoreSurface      *surface;
     CorePalette      *core_palette;
     IDirectFBPalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!surface->palette)
          return DFB_UNSUPPORTED;

     if (!interface)
          return DFB_INVARG;

     ret = CoreSurface_GetPalette( surface, &core_palette );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( palette, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( palette, core_palette, data->core );
     if (ret)
          goto out;

     *interface = palette;

out:
     dfb_palette_unref( core_palette );

     return ret;
}

static DFBResult
IDirectFBSurface_SetPalette( IDirectFBSurface *thiz,
                             IDirectFBPalette *palette )
{
     CoreSurface           *surface;
     IDirectFBPalette_data *palette_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!palette)
          return DFB_INVARG;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          return DFB_UNSUPPORTED;

     palette_data = (IDirectFBPalette_data*) palette->priv;
     if (!palette_data)
          return DFB_DEAD;

     if (!palette_data->palette)
          return DFB_DESTROYED;

     CoreSurface_SetPalette( surface, palette_data->palette );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetAlphaRamp( IDirectFBSurface *thiz,
                               u8                a0,
                               u8                a1,
                               u8                a2,
                               u8                a3 )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     CoreSurface_SetAlphaRamp( data->surface, a0, a1, a2, a3 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Lock( IDirectFBSurface *thiz,
                       DFBSurfaceLockFlags flags,
                       void **ret_ptr, int *ret_pitch )
{
     DFBResult              ret;
     CoreSurfaceBufferRole  role   = CSBR_FRONT;
     CoreSurfaceAccessFlags access = CSAF_NONE;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked)
          return DFB_LOCKED;

     if (!flags || !ret_ptr || !ret_pitch)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (flags & DSLF_READ)
          access |= CSAF_READ;

     if (flags & DSLF_WRITE) {
          access |= CSAF_WRITE;
          role = CSBR_BACK;
     }

     ret = dfb_surface_lock_buffer( data->surface, role, CSAID_CPU, access, &data->lock );
     if (ret)
          return ret;

     data->locked = true;

     *ret_ptr   = data->lock.addr + data->lock.pitch * data->area.current.y +
                  DFB_BYTES_PER_LINE( data->surface->config.format, data->area.current.x );
     *ret_pitch = data->lock.pitch;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetFramebufferOffset( IDirectFBSurface *thiz,
                                       int *offset )
{
     /*
      * Previously returned the framebuffer offset of a locked surface.
      * However, it is not a safe API to use at all, since it is not
      * guaranteed that the offset actually belongs to fbmem (e.g. could be AGP memory).
      */

     return DFB_FAILURE;
}

static DFBResult
IDirectFBSurface_GetPhysicalAddress( IDirectFBSurface *thiz,
                                     unsigned long    *addr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!addr)
          return DFB_INVARG;

     if (!data->locked)
          return DFB_ACCESSDENIED;

     if (!data->lock.phys) {
          /* The surface is probably in a system buffer if there's no physical address. */
          return DFB_UNSUPPORTED;
     }

     *addr = data->lock.phys;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Unlock( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked) {
          dfb_surface_unlock_buffer( data->surface, &data->lock );

          data->locked = false;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Write( IDirectFBSurface    *thiz,
                        const DFBRectangle  *rect,
                        const void          *ptr,
                        int                  pitch )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, thiz, rect, ptr, pitch );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!rect || !ptr)
          return DFB_INVARG;

     if (ABS(pitch) < DFB_BYTES_PER_LINE( surface->config.format, rect->w ))
          return DFB_INVARG;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     D_DEBUG_AT( Surface, "  ->      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( rect ) );

     //FIXME: check rectangle

     return dfb_surface_write_buffer( data->surface, CSBR_BACK, ptr, pitch, rect );
}

static DFBResult
IDirectFBSurface_Read( IDirectFBSurface    *thiz,
                       const DFBRectangle  *rect,
                       void                *ptr,
                       int                  pitch )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, thiz, rect, ptr, pitch );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!rect || !ptr || pitch < DFB_BYTES_PER_LINE(surface->config.format,rect->w ) )
          return DFB_INVARG;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     D_DEBUG_AT( Surface, "  ->      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( rect ) );

     //FIXME: check rectangle

     return dfb_surface_read_buffer( data->surface, CSBR_FRONT, ptr, pitch, rect );
}

static DFBResult
IDirectFBSurface_Flip( IDirectFBSurface    *thiz,
                       const DFBRegion     *region,
                       DFBSurfaceFlipFlags  flags )
{
     DFBResult    ret = DFB_OK;
     DFBRegion    reg;
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h ||
         (region && (region->x1 > region->x2 || region->y1 > region->y2)))
          return DFB_INVAREA;

     IDirectFBSurface_StopAll( data );

     /* FIXME: This is a temporary workaround for LiTE. */
     if (data->parent) {
          IDirectFBSurface_data *parent_data;

          DIRECT_INTERFACE_GET_DATA_FROM( data->parent, parent_data, IDirectFBSurface );

          /* Signal end of sequence of operations. */
          dfb_state_lock( &parent_data->state );
          dfb_state_stop_drawing( &parent_data->state );
          dfb_state_unlock( &parent_data->state );
     }

     dfb_region_from_rectangle( &reg, &data->area.current );

     if (region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( region,
                                                       data->area.wanted.x,
                                                       data->area.wanted.y );

          if (!dfb_region_region_intersect( &reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  ->      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &reg ) );

     CoreGraphicsStateClient_Flush( &data->state_client );

     if (surface->config.caps & DSCAPS_FLIPPING) {
          if (!(flags & DSFLIP_BLIT) && reg.x1 == 0 && reg.y1 == 0 &&
              reg.x2 == surface->config.size.w - 1 && reg.y2 == surface->config.size.h - 1)
          {
               ret = CoreSurface_Flip( data->surface, false );
          }
          else
               CoreSurface_BackToFrontCopy( data->surface, DSSE_LEFT, &reg, NULL );
     }

     dfb_surface_dispatch_update( data->surface, &reg, &reg );

     IDirectFBSurface_WaitForBackBuffer( data );

     return ret;
}

static DFBResult
IDirectFBSurface_SetField( IDirectFBSurface    *thiz,
                           int                  field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_INTERLACED))
          return DFB_UNSUPPORTED;

     if (field < 0 || field > 1)
          return DFB_INVARG;

     CoreSurface_SetField( data->surface, field );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Clear( IDirectFBSurface *thiz,
                        u8 r, u8 g, u8 b, u8 a )
{
     DFBColor                old_color;
     unsigned int            old_index;
     DFBSurfaceDrawingFlags  old_flags;
     DFBSurfaceRenderOptions old_options;
     CoreSurface            *surface;
     DFBColor                color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, PIXEL_ARGB(a,r,g,b) );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     /* save current color and drawing flags */
     old_color   = data->state.color;
     old_index   = data->state.color_index;
     old_flags   = data->state.drawingflags;
     old_options = data->state.render_options;

     /* set drawing flags */
     dfb_state_set_drawing_flags( &data->state, DSDRAW_NOFX );

     /* set render options */
     dfb_state_set_render_options( &data->state, DSRO_NONE );

     /* set color */
     if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          dfb_state_set_color_index( &data->state,
                                     dfb_palette_search( surface->palette, r, g, b, a ) );

     dfb_state_set_color( &data->state, &color );

     /* fill the visible rectangle */
     CoreGraphicsStateClient_FillRectangles( &data->state_client, &data->area.current, 1 );

     /* clear the depth buffer */
     if (data->caps & DSCAPS_DEPTH)
          dfb_clear_depth( data->surface, &data->state.clip );

     /* restore drawing flags */
     dfb_state_set_drawing_flags( &data->state, old_flags );

     /* restore render options */
     dfb_state_set_render_options( &data->state, old_options );

     /* restore color */
     if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          dfb_state_set_color_index( &data->state, old_index );

     dfb_state_set_color( &data->state, &old_color );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetClip( IDirectFBSurface *thiz, const DFBRegion *clip )
{
     DFBRegion newclip;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, clip );


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (clip) {
          newclip = DFB_REGION_INIT_TRANSLATED( clip, data->area.wanted.x, data->area.wanted.y );

          D_DEBUG_AT( Surface, "  <-      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(&newclip) );

          if (!dfb_unsafe_region_rectangle_intersect( &newclip,
                                                      &data->area.wanted ))
               return DFB_INVARG;

          D_DEBUG_AT( Surface, "  ->      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(&newclip) );

          data->clip_set = true;
          data->clip_wanted = newclip;

          if (!dfb_region_rectangle_intersect( &newclip, &data->area.current ))
               return DFB_INVAREA;
     }
     else {
          dfb_region_from_rectangle( &newclip, &data->area.current );
          data->clip_set = false;
     }

     D_DEBUG_AT( Surface, "  => CLIP %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(&newclip) );

     dfb_state_set_clip( &data->state, &newclip );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetClip( IDirectFBSurface *thiz, DFBRegion *ret_clip )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_clip)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     *ret_clip = DFB_REGION_INIT_TRANSLATED( &data->state.clip, -data->area.wanted.x, -data->area.wanted.y );

     D_DEBUG_AT( Surface, "  ->      %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(ret_clip) );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColor( IDirectFBSurface *thiz,
                           u8 r, u8 g, u8 b, u8 a )
{
     CoreSurface *surface;
     DFBColor     color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, COLOR 0x%08x )\n", __FUNCTION__, thiz, PIXEL_ARGB(a, r, g, b) );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     dfb_state_set_color( &data->state, &color );

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          dfb_state_set_color_index( &data->state,
                                     dfb_palette_search( surface->palette, r, g, b, a ) );

     data->state.colors[0]        = data->state.color;
     data->state.color_indices[0] = data->state.color_index;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColors( IDirectFBSurface *thiz,
                            const DFBColorID *ids,
                            const DFBColor   *colors,
                            unsigned int      num )
{
     unsigned int  i;
     CoreSurface  *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, %u )\n", __FUNCTION__, thiz, ids, colors, num );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     for (i=0; i<num; i++) {
          D_DEBUG_AT( Surface, "  -> [%d] id %d = %02x %02x %02x %02x\n",
                      i, ids[i], colors[i].a, colors[i].r, colors[i].g, colors[i].b );

          if (ids[i] >= DFB_COLOR_IDS_MAX)
               return DFB_INVARG;

          data->state.colors[ids[i]] = colors[i];

          if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
               data->state.color_indices[ids[i]] = dfb_palette_search( surface->palette,
                                                                       colors[i].r, colors[i].g, colors[i].b, colors[i].a );
     }

     dfb_state_set_color( &data->state, &data->state.colors[0] );
     dfb_state_set_color_index( &data->state, data->state.color_indices[0] );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColorIndex( IDirectFBSurface *thiz,
                                unsigned int      index )
{
     CoreSurface *surface;
     CorePalette *palette;
     DFBResult    ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, COLOR INDEX %3u )\n", __FUNCTION__, thiz, index );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          return DFB_UNSUPPORTED;

     palette = surface->palette;
     if (!palette)
          return DFB_UNSUPPORTED;

     if (index > palette->num_entries)
          return DFB_INVARG;

     ret = CoreGraphicsState_SetColorAndIndex( data->state_client.gfx_state, &palette->entries[index], index );
     if (ret)
          return ret;

     dfb_state_set_color( &data->state, &palette->entries[index] );
     dfb_state_set_color_index( &data->state, index );

     data->state.colors[0]        = data->state.color;
     data->state.color_indices[0] = data->state.color_index;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  src )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, src );

     switch (src) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               dfb_state_set_src_blend( &data->state, src );
               return DFB_OK;

          default:
               break;
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFBSurface_SetDstBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  dst )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, dst );

     switch (dst) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               dfb_state_set_dst_blend( &data->state, dst );
               return DFB_OK;

          default:
               break;
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFBSurface_SetPorterDuff( IDirectFBSurface         *thiz,
                                DFBSurfacePorterDuffRule  rule )
{
     DFBSurfaceBlendFunction src;
     DFBSurfaceBlendFunction dst;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, rule );


     switch (rule) {
          case DSPD_NONE:
               src = DSBF_SRCALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_CLEAR:
               src = DSBF_ZERO;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC:
               src = DSBF_ONE;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC_OVER:
               src = DSBF_ONE;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST_OVER:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ONE;
               break;
          case DSPD_SRC_IN:
               src = DSBF_DESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_IN:
               src = DSBF_ZERO;
               dst = DSBF_SRCALPHA;
               break;
          case DSPD_SRC_OUT:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_OUT:
               src = DSBF_ZERO;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_SRC_ATOP:
               src = DSBF_DESTALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST_ATOP:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_SRCALPHA;
               break;
          case DSPD_ADD:
               src = DSBF_ONE;
               dst = DSBF_ONE;
               break;
          case DSPD_XOR:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST:
               src = DSBF_ZERO;
               dst = DSBF_ONE;
               break;
          default:
               return DFB_INVARG;
     }

     dfb_state_set_src_blend( &data->state, src );
     dfb_state_set_dst_blend( &data->state, dst );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKey( IDirectFBSurface *thiz,
                                 u8                r,
                                 u8                g,
                                 u8                b )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     data->src_key.r = r;
     data->src_key.g = g;
     data->src_key.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          data->src_key.value = dfb_palette_search( surface->palette,
                                                    r, g, b, 0x80 );
     else
          data->src_key.value = dfb_color_to_pixel( surface->config.format, r, g, b );

     /* The new key won't be applied to this surface's state.
        The key will be taken by the destination surface to apply it
        to its state when source color keying is used. */

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKeyIndex( IDirectFBSurface *thiz,
                                      unsigned int      index )
{
     CoreSurface *surface;
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          return DFB_UNSUPPORTED;

     palette = surface->palette;
     if (!palette)
          return DFB_UNSUPPORTED;

     if (index > palette->num_entries)
          return DFB_INVARG;

     data->src_key.r = palette->entries[index].r;
     data->src_key.g = palette->entries[index].g;
     data->src_key.b = palette->entries[index].b;

     data->src_key.value = index;

     /* The new key won't be applied to this surface's state.
        The key will be taken by the destination surface to apply it
        to its state when source color keying is used. */

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKey( IDirectFBSurface *thiz,
                                 u8                r,
                                 u8                g,
                                 u8                b )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     data->dst_key.r = r;
     data->dst_key.g = g;
     data->dst_key.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          data->dst_key.value = dfb_palette_search( surface->palette,
                                                    r, g, b, 0x80 );
     else
          data->dst_key.value = dfb_color_to_pixel( surface->config.format, r, g, b );

     dfb_state_set_dst_colorkey( &data->state, data->dst_key.value );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKeyIndex( IDirectFBSurface *thiz,
                                      unsigned int      index )
{
     CoreSurface *surface;
     CorePalette *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          return DFB_UNSUPPORTED;

     palette = surface->palette;
     if (!palette)
          return DFB_UNSUPPORTED;

     if (index > palette->num_entries)
          return DFB_INVARG;

     data->dst_key.r = palette->entries[index].r;
     data->dst_key.g = palette->entries[index].g;
     data->dst_key.b = palette->entries[index].b;

     data->dst_key.value = index;

     dfb_state_set_dst_colorkey( &data->state, data->dst_key.value );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetIndexTranslation( IDirectFBSurface *thiz,
                                      const int        *indices,
                                      int               num_indices )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->config.format ))
          return DFB_UNSUPPORTED;

     if (!indices && num_indices > 0)
          return DFB_INVAREA;

     if (num_indices < 0 || num_indices > 256)
          return DFB_INVARG;

     return dfb_state_set_index_translation( &data->state, indices, num_indices );
}

static DFBResult
IDirectFBSurface_SetFont( IDirectFBSurface *thiz,
                          IDirectFBFont    *font )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, font );

     if (data->font != font) {
         if (font) {
              IDirectFBFont_data *font_data;

              ret = font->AddRef( font );
              if (ret)
                   return ret;

              DIRECT_INTERFACE_GET_DATA_FROM( font, font_data, IDirectFBFont );

              data->encoding = font_data->encoding;
         }

         if (data->font)
              data->font->Release( data->font );

         data->font = font;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetFont( IDirectFBSurface  *thiz,
                          IDirectFBFont    **ret_font )
{
     DFBResult      ret;
     IDirectFBFont *font;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_font)
          return DFB_INVARG;

     font = data->font;
     if (!font) {
          *ret_font = NULL;
          return DFB_MISSINGFONT;
     }

     ret = font->AddRef( font );
     if (ret)
          return ret;

     *ret_font = font;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDrawingFlags( IDirectFBSurface       *thiz,
                                  DFBSurfaceDrawingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, flags );

     dfb_state_set_drawing_flags( &data->state, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillRectangle( IDirectFBSurface *thiz,
                                int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d\n", 0, x, y, w, h );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     CoreGraphicsStateClient_FillRectangles( &data->state_client, &rect, 1 );

     return DFB_OK;
}


static DFBResult
IDirectFBSurface_DrawLine( IDirectFBSurface *thiz,
                           int x1, int y1, int x2, int y2 )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4d,%4d\n", 0, x1, y1, x2, y2 );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if ((x1 == x2 || y1 == y2) && !(data->state.render_options & DSRO_MATRIX)) {
          DFBRectangle rect;

          if (x1 <= x2) {
               rect.x = x1;
               rect.w = x2 - x1 + 1;
          }
          else {
               rect.x = x2;
               rect.w = x1 - x2 + 1;
          }

          if (y1 <= y2) {
               rect.y = y1;
               rect.h = y2 - y1 + 1;
          }
          else {
               rect.y = y2;
               rect.h = y1 - y2 + 1;
          }

          rect.x += data->area.wanted.x;
          rect.y += data->area.wanted.y;

          CoreGraphicsStateClient_FillRectangles( &data->state_client, &rect, 1 );
     }
     else {
          DFBRegion line = { x1, y1, x2, y2 };

          line.x1 += data->area.wanted.x;
          line.x2 += data->area.wanted.x;
          line.y1 += data->area.wanted.y;
          line.y2 += data->area.wanted.y;

          CoreGraphicsStateClient_DrawLines( &data->state_client, &line, 1 );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawLines( IDirectFBSurface *thiz,
                            const DFBRegion  *lines,
                            unsigned int      num_lines )
{
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p [%d] )\n", __FUNCTION__, thiz, lines, num_lines );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!lines || !num_lines)
          return DFB_INVARG;

     /* Check if all lines are either horizontal or vertical */
     for (i=0; i<num_lines; i++) {
          if (lines[i].x1 != lines[i].x2 && lines[i].y1 != lines[i].y2)
               break;
     }

     /* Use real line drawing? */
     if (i < num_lines) {
          DFBRegion *local_lines = alloca(sizeof(DFBRegion) * num_lines);

          if (data->area.wanted.x || data->area.wanted.y) {
               for (i=0; i<num_lines; i++) {
                    local_lines[i].x1 = lines[i].x1 + data->area.wanted.x;
                    local_lines[i].x2 = lines[i].x2 + data->area.wanted.x;
                    local_lines[i].y1 = lines[i].y1 + data->area.wanted.y;
                    local_lines[i].y2 = lines[i].y2 + data->area.wanted.y;
               }
          }
          else
               /* clipping may modify lines, so we copy them */
               direct_memcpy( local_lines, lines, sizeof(DFBRegion) * num_lines );

          CoreGraphicsStateClient_DrawLines( &data->state_client, local_lines, num_lines );
     }
     /* Optimized rectangle drawing */
     else {
          DFBRectangle *local_rects = alloca(sizeof(DFBRectangle) * num_lines);

          for (i=0; i<num_lines; i++) {
               /* Vertical line? */
               if (lines[i].x1 == lines[i].x2) {
                    local_rects[i].x = data->area.wanted.x + lines[i].x1;
                    local_rects[i].y = data->area.wanted.y + MIN( lines[i].y1, lines[i].y2 );
                    local_rects[i].w = 1;
                    local_rects[i].h = ABS( lines[i].y2 - lines[i].y1 ) + 1;
               }
               /* Horizontal line */
               else {
                    local_rects[i].x = data->area.wanted.x + MIN( lines[i].x1, lines[i].x2 );
                    local_rects[i].y = data->area.wanted.y + lines[i].y1;
                    local_rects[i].w = ABS( lines[i].x2 - lines[i].x1 ) + 1;
                    local_rects[i].h = 1;
               }
          }

          CoreGraphicsStateClient_FillRectangles( &data->state_client, local_rects, num_lines );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawRectangle( IDirectFBSurface *thiz,
                                int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d\n", 0, x, y, w, h );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     CoreGraphicsStateClient_DrawRectangles( &data->state_client, &rect, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTriangle( IDirectFBSurface *thiz,
                               int x1, int y1,
                               int x2, int y2,
                               int x3, int y3 )
{
     DFBTriangle tri = { x1, y1, x2, y2, x3, y3 };

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4d,%4d-%4d,%4d\n", 0, x1, y1, x2, y2, x3, y3 );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     tri.x1 += data->area.wanted.x;
     tri.y1 += data->area.wanted.y;
     tri.x2 += data->area.wanted.x;
     tri.y2 += data->area.wanted.y;
     tri.x3 += data->area.wanted.x;
     tri.y3 += data->area.wanted.y;

     CoreGraphicsStateClient_FillTriangles( &data->state_client, &tri, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTrapezoids( IDirectFBSurface   *thiz,
                                 const DFBTrapezoid *traps,
                                 unsigned int        num_traps )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!traps || !num_traps)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBTrapezoid  *local_traps;
          bool           malloced = (num_traps > 170);

          if (malloced)
               local_traps = D_MALLOC( sizeof(DFBTrapezoid) * num_traps );
          else
               local_traps = alloca( sizeof(DFBTrapezoid) * num_traps );

          for (i=0; i<num_traps; i++) {
               local_traps[i].x1 = traps[i].x1 + data->area.wanted.x;
               local_traps[i].y1 = traps[i].y1 + data->area.wanted.y;
               local_traps[i].w1 = traps[i].w1;
               local_traps[i].x2 = traps[i].x2 + data->area.wanted.x;
               local_traps[i].y2 = traps[i].y2 + data->area.wanted.y;
               local_traps[i].w2 = traps[i].w2;
          }

          CoreGraphicsStateClient_FillTrapezoids( &data->state_client, local_traps, num_traps );

          if (malloced)
               D_FREE( local_traps );
     }
     else
          CoreGraphicsStateClient_FillTrapezoids( &data->state_client, traps, num_traps );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillRectangles( IDirectFBSurface   *thiz,
                                 const DFBRectangle *rects,
                                 unsigned int        num_rects )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p [%d] )\n", __FUNCTION__, thiz, rects, num_rects );

     DFB_RECTANGLES_DEBUG_AT( Surface, rects, num_rects );


     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!rects || !num_rects)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBRectangle *local_rects;
          bool          malloced = (num_rects > 256);

          if (malloced)
               local_rects = D_MALLOC( sizeof(DFBRectangle) * num_rects );
          else
               local_rects = alloca( sizeof(DFBRectangle) * num_rects );

          for (i=0; i<num_rects; i++) {
               local_rects[i].x = rects[i].x + data->area.wanted.x;
               local_rects[i].y = rects[i].y + data->area.wanted.y;
               local_rects[i].w = rects[i].w;
               local_rects[i].h = rects[i].h;
          }

          CoreGraphicsStateClient_FillRectangles( &data->state_client, local_rects, num_rects );

          if (malloced)
               D_FREE( local_rects );
     }
     else
          CoreGraphicsStateClient_FillRectangles( &data->state_client, rects, num_rects );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillSpans( IDirectFBSurface *thiz,
                            int               y,
                            const DFBSpan    *spans,
                            unsigned int      num_spans )
{
     DFBSpan *local_spans = alloca(sizeof(DFBSpan) * num_spans);

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!spans || !num_spans)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int i;

          for (i=0; i<num_spans; i++) {
               local_spans[i].x = spans[i].x + data->area.wanted.x;
               local_spans[i].w = spans[i].w;
          }
     }
     else
          /* clipping may modify spans, so we copy them */
          direct_memcpy( local_spans, spans, sizeof(DFBSpan) * num_spans );

     CoreGraphicsStateClient_FillSpans( &data->state_client, y + data->area.wanted.y, local_spans, num_spans );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTriangles( IDirectFBSurface  *thiz,
                                const DFBTriangle *tris,
                                unsigned int       num_tris )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!tris || !num_tris)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBTriangle  *local_tris;
          bool          malloced = (num_tris > 170);

          if (malloced)
               local_tris = D_MALLOC( sizeof(DFBTriangle) * num_tris );
          else
               local_tris = alloca( sizeof(DFBTriangle) * num_tris );

          for (i=0; i<num_tris; i++) {
               local_tris[i].x1 = tris[i].x1 + data->area.wanted.x;
               local_tris[i].y1 = tris[i].y1 + data->area.wanted.y;
               local_tris[i].x2 = tris[i].x2 + data->area.wanted.x;
               local_tris[i].y2 = tris[i].y2 + data->area.wanted.y;
               local_tris[i].x3 = tris[i].x3 + data->area.wanted.x;
               local_tris[i].y3 = tris[i].y3 + data->area.wanted.y;
          }

          CoreGraphicsStateClient_FillTriangles( &data->state_client, local_tris, num_tris );

          if (malloced)
               D_FREE( local_tris );
     }
     else
          CoreGraphicsStateClient_FillTriangles( &data->state_client, tris, num_tris );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetBlittingFlags( IDirectFBSurface        *thiz,
                                   DFBSurfaceBlittingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_blitting_flags( &data->state, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Blit( IDirectFBSurface   *thiz,
                       IDirectFBSurface   *source,
                       const DFBRectangle *sr,
                       int dx, int dy )
{
     DFBRectangle srect;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (sr)
          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d\n", 0, dx, dy, sr->w, sr->h, sr->x, sr->y );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!source)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     if (sr) {
          if (sr->w < 1  ||  sr->h < 1)
               return DFB_OK;

          srect = *sr;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (sr->x + src_data->area.wanted.x);
          dy += srect.y - (sr->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     if (src_data->surface_client) {
          direct_mutex_lock( &data->surface_client_lock );
          dfb_state_set_source_2( &data->state, src_data->surface, src_data->surface_client_flip_count );
     }
     else
          dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     DFBPoint p = { data->area.wanted.x + dx, data->area.wanted.y + dy };

     CoreGraphicsStateClient_Blit( &data->state_client, &srect, &p, 1 );

     if (src_data->surface_client)
          direct_mutex_unlock( &data->surface_client_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_TileBlit( IDirectFBSurface   *thiz,
                           IDirectFBSurface   *source,
                           const DFBRectangle *sr,
                           int dx, int dy )
{
     DFBRectangle           srect;
     DFBPoint               p1, p2;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (sr)
          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d\n", 0, dx, dy, sr->w, sr->h, sr->x, sr->y );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;


     if (sr) {
          if (sr->w < 1  ||  sr->h < 1)
               return DFB_OK;

          srect = *sr;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (sr->x + src_data->area.wanted.x);
          dy += srect.y - (sr->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     dx %= srect.w;
     if (dx > 0)
          dx -= srect.w;

     dy %= srect.h;
     if (dy > 0)
          dy -= srect.h;

     dx += data->area.wanted.x;
     dy += data->area.wanted.y;

     p1.x = dx;
     p1.y = dy;

     p2.x = dx + data->area.wanted.w + srect.w - 1;
     p2.y = dy + data->area.wanted.h + srect.h - 1;

     CoreGraphicsStateClient_TileBlit( &data->state_client, &srect, &p1, &p2, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_BatchBlit( IDirectFBSurface   *thiz,
                            IDirectFBSurface   *source,
                            const DFBRectangle *source_rects,
                            const DFBPoint     *dest_points,
                            int                 num )
{
     int                    i, dx, dy, sx, sy;
     DFBRectangle          *rects;
     DFBPoint              *points;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!source || !source_rects || !dest_points || num < 1)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     rects  = alloca( sizeof(DFBRectangle) * num );
     points = alloca( sizeof(DFBPoint) * num );

     direct_memcpy( rects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( points, dest_points, sizeof(DFBPoint) * num );

     for (i=0; i<num; i++) {
          rects[i].x += sx;
          rects[i].y += sy;

          points[i].x += dx;
          points[i].y += dy;

          if (!dfb_rectangle_intersect( &rects[i], &src_data->area.current ))
               rects[i].w = rects[i].h = 0;

          points[i].x += rects[i].x - (source_rects[i].x + sx);
          points[i].y += rects[i].y - (source_rects[i].y + sy);
     }

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_Blit( &data->state_client, rects, points, num );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_BatchBlit2( IDirectFBSurface   *thiz,
                             IDirectFBSurface   *source,
                             IDirectFBSurface   *source2,
                             const DFBRectangle *source_rects,
                             const DFBPoint     *dest_points,
                             const DFBPoint     *source2_points,
                             int                 num )
{
     int                    i, dx, dy, sx, sy, sx2, sy2;
     DFBRectangle          *rects;
     DFBPoint              *points;
     DFBPoint              *points2;
     IDirectFBSurface_data *src_data;
     IDirectFBSurface_data *src2_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!source || !source_rects || !dest_points || num < 1)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     src2_data = (IDirectFBSurface_data*)source2->priv;

     if (!src2_data->area.current.w || !src2_data->area.current.h)
          return DFB_INVAREA;


     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     sx2 = src2_data->area.wanted.x;
     sy2 = src2_data->area.wanted.y;

     rects   = alloca( sizeof(DFBRectangle) * num );
     points  = alloca( sizeof(DFBPoint) * num );
     points2 = alloca( sizeof(DFBPoint) * num );

     direct_memcpy( rects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( points, dest_points, sizeof(DFBPoint) * num );
     direct_memcpy( points2, source2_points, sizeof(DFBPoint) * num );

     for (i=0; i<num; i++) {
          DFBRectangle rect2;

          rects[i].x += sx;
          rects[i].y += sy;

          points[i].x += dx;
          points[i].y += dy;

          points2[i].x += sx2;
          points2[i].y += sy2;

          if (!dfb_rectangle_intersect( &rects[i], &src_data->area.current ))
               rects[i].w = rects[i].h = 0;
          else {
               points[i].x += rects[i].x - (source_rects[i].x + sx);
               points[i].y += rects[i].y - (source_rects[i].y + sy);
               points2[i].x += rects[i].x - (source_rects[i].x + sx);
               points2[i].y += rects[i].y - (source_rects[i].y + sy);


               rect2.x = points2[i].x;
               rect2.y = points2[i].y;
               rect2.w = rects[i].w;
               rect2.h = rects[i].h;

               if (!dfb_rectangle_intersect( &rect2, &src2_data->area.current ))
                    rects[i].w = rects[i].h = 0;

               points[i].x += rect2.x - points2[i].x;
               points[i].y += rect2.y - points2[i].y;
               points2[i].x += rect2.x - points2[i].x;
               points2[i].y += rect2.y - points2[i].y;

               rects[i].w = rect2.w;
               rects[i].h = rect2.h;
          }
     }

     dfb_state_set_source( &data->state, src_data->surface );
     dfb_state_set_source2( &data->state, src2_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_Blit2( &data->state_client, rects, points, points2, num );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_BatchStretchBlit( IDirectFBSurface   *thiz,
                                   IDirectFBSurface   *source,
                                   const DFBRectangle *source_rects,
                                   const DFBRectangle *dest_rects,
                                   int                 num )
{
     int i, dx, dy, sx, sy;
     DFBRectangle *srects, *drects;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, num );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source || !source_rects || !dest_rects || num < 1)
          return DFB_INVARG;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     srects = alloca( sizeof(DFBRectangle) * num );
     drects = alloca( sizeof(DFBRectangle) * num );

     direct_memcpy( srects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( drects, dest_rects, sizeof(DFBRectangle) * num );

     for (i=0; i<num; ++i) {
          DFBRectangle orig_src;

          if (drects[i].w < 1 || drects[i].h < 1) {
               drects[i].w = 0;
               drects[i].h = 0;
               continue;
          }
          drects[i].x += dx;
          drects[i].y += dy;

          if (srects[i].w < 1 || srects[i].h < 1)
               return DFB_INVARG;
          srects[i].x += sx;
          srects[i].y += sy;

          /* clipping of the source rectangle must be applied to the destination */
          orig_src = srects[i];

          if (!dfb_rectangle_intersect( &srects[i], &src_data->area.current )) {
               srects[i].w = srects[i].h = 0;
               drects[i].w = drects[i].h = 0;
               continue;
          }

          if (srects[i].x != orig_src.x)
               drects[i].x += (int)( (srects[i].x - orig_src.x) *
                                     (drects[i].w / (float)orig_src.w) + 0.5f);

          if (srects[i].y != orig_src.y)
               drects[i].y += (int)( (srects[i].y - orig_src.y) *
                                     (drects[i].h / (float)orig_src.h) + 0.5f);

          if (srects[i].w != orig_src.w)
               drects[i].w = D_ICEIL(drects[i].w * (srects[i].w / (float)orig_src.w));
          if (srects[i].h != orig_src.h)
               drects[i].h = D_ICEIL(drects[i].h * (srects[i].h / (float)orig_src.h));

          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d\n",
                      i, drects[i].x, drects[i].y, drects[i].w, drects[i].h,
                      srects[i].x, srects[i].y, srects[i].w, srects[i].h );
     }

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_StretchBlit( &data->state_client, srects, drects, num );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_StretchBlit( IDirectFBSurface   *thiz,
                              IDirectFBSurface   *source,
                              const DFBRectangle *source_rect,
                              const DFBRectangle *destination_rect )
{
     DFBRectangle srect, drect;

     if (!source)
          return DFB_INVARG;

     if (destination_rect)
          drect = *destination_rect;
     else {
          DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

          drect = (DFBRectangle) { 0, 0, data->area.wanted.w, data->area.wanted.h };
     }

     if (source_rect)
          srect = *source_rect;
     else {
          IDirectFBSurface_data *sd = (IDirectFBSurface_data*)source->priv;

          srect = (DFBRectangle) { 0, 0, sd->area.wanted.w, sd->area.wanted.h };
     }

     return IDirectFBSurface_BatchStretchBlit( thiz, source, &srect, &drect, 1 );
}

#define SET_VERTEX(v,X,Y,Z,W,S,T)  \
     do {                          \
          (v)->x = X;              \
          (v)->y = Y;              \
          (v)->z = Z;              \
          (v)->w = W;              \
          (v)->s = S;              \
          (v)->t = T;              \
     } while (0)

static DFBResult
IDirectFBSurface_TextureTriangles( IDirectFBSurface     *thiz,
                                   IDirectFBSurface     *source,
                                   const DFBVertex      *vertices,
                                   const int            *indices,
                                   int                   num,
                                   DFBTriangleFormation  formation )
{
     int                    i;
     DFBVertex             *translated;
     IDirectFBSurface_data *src_data;
     bool                   src_sub;
     float                  x0 = 0;
     float                  y0 = 0;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source || !vertices || num < 3)
          return DFB_INVARG;

     src_data = (IDirectFBSurface_data*)source->priv;

     if ((src_sub = (src_data->caps & DSCAPS_SUBSURFACE))) {
          D_ONCE( "sub surface texture not fully working with 'repeated' mapping" );

          x0 = data->area.wanted.x;
          y0 = data->area.wanted.y;
     }

     switch (formation) {
          case DTTF_LIST:
               if (num % 3)
                    return DFB_INVARG;
               break;

          case DTTF_STRIP:
          case DTTF_FAN:
               break;

          default:
               return DFB_INVARG;
     }

     translated = alloca( num * sizeof(DFBVertex) );
     if (!translated)
          return DFB_NOSYSTEMMEMORY;

     /* TODO: pass indices through to driver */
     if (src_sub) {
          float oowidth  = 1.0f / src_data->surface->config.size.w;
          float ooheight = 1.0f / src_data->surface->config.size.h;

          float s0 = src_data->area.wanted.x * oowidth;
          float t0 = src_data->area.wanted.y * ooheight;

          float fs = src_data->area.wanted.w * oowidth;
          float ft = src_data->area.wanted.h * ooheight;

          for (i=0; i<num; i++) {
               const DFBVertex *in  = &vertices[ indices ? indices[i] : i ];
               DFBVertex       *out = &translated[i];

               SET_VERTEX( out, x0 + in->x, y0 + in->y, in->z, in->w,
                           s0 + fs * in->s, t0 + ft * in->t );
          }
     }
     else {
          if (indices) {
               for (i=0; i<num; i++) {
                    const DFBVertex *in  = &vertices[ indices[i] ];
                    DFBVertex       *out = &translated[i];

                    SET_VERTEX( out, x0 + in->x, y0 + in->y, in->z, in->w, in->s, in->t );
               }
          }
          else {
               direct_memcpy( translated, vertices, num * sizeof(DFBVertex) );

               for (i=0; i<num; i++) {
                    translated[i].x += x0;
                    translated[i].y += y0;
               }
          }
     }

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, CSBR_FRONT, src_data->src_eye );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_TextureTriangles( &data->state_client, translated, num, formation );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawString( IDirectFBSurface *thiz,
                             const char *text, int bytes,
                             int x, int y,
                             DFBSurfaceTextFlags flags )
{
     DFBResult           ret;
     IDirectFBFont      *font;
     IDirectFBFont_data *font_data;
     CoreFont           *core_font;
     unsigned int        layers = 1;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!text)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;

     if (bytes < 0)
          bytes = strlen (text);

     if (bytes == 0)
          return DFB_OK;

     font = data->font;

     DIRECT_INTERFACE_GET_DATA_FROM( font, font_data, IDirectFBFont );

     core_font = font_data->font;

     if (flags & DSTF_OUTLINE) {
          if (!(core_font->attributes & DFFA_OUTLINED))
               return DFB_UNSUPPORTED;

          layers = 2;
     }

     if (!(flags & DSTF_TOP)) {
          x += core_font->ascender * core_font->up_unit_x;
          y += core_font->ascender * core_font->up_unit_y;

          if (flags & DSTF_BOTTOM) {
               x -= core_font->descender * core_font->up_unit_x;
               y -= core_font->descender * core_font->up_unit_y;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          int          i, num, kx, ky;
          int          xsize = 0;
          int          ysize = 0;
          unsigned int prev = 0;
          unsigned int indices[bytes];

          /* FIXME: Avoid double locking and decoding. */
          dfb_font_lock( core_font );

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( core_font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( core_font );
               return ret;
          }

          /* Calculate string width. */
          for (i=0; i<num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( core_font, current, 0, &glyph ) == DFB_OK) {
                    xsize += glyph->xadvance;
                    ysize += glyph->yadvance;

                    if (prev && core_font->GetKerning &&
                        core_font->GetKerning( core_font, prev, current, &kx, &ky ) == DFB_OK) {
                         xsize += kx << 8;
                         ysize += ky << 8;
                    }
               }

               prev = current;
          }

          dfb_font_unlock( core_font );

          /* Justify. */
          if (flags & DSTF_RIGHT) {
               x -= xsize >> 8;
               y -= ysize >> 8;
          }
          else if (flags & DSTF_CENTER) {
               x -= xsize >> 9;
               y -= ysize >> 9;
          }
     }

     dfb_gfxcard_drawstring( (const unsigned char*) text, bytes, data->encoding,
                             data->area.wanted.x + x, data->area.wanted.y + y,
                             core_font, layers, &data->state_client );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawGlyph( IDirectFBSurface *thiz,
                            unsigned int character, int x, int y,
                            DFBSurfaceTextFlags flags )
{
     DFBResult           ret;
     int                 l;
     IDirectFBFont      *font;
     IDirectFBFont_data *font_data;
     CoreFont           *core_font;
     CoreGlyphData      *glyph[DFB_FONT_MAX_LAYERS];
     unsigned int        index;
     unsigned int        layers = 1;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, 0x%x, %d,%d, 0x%x )\n",
                 __FUNCTION__, thiz, character, x, y, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!character)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;

     font = data->font;

     DIRECT_INTERFACE_GET_DATA_FROM( font, font_data, IDirectFBFont );

     core_font = font_data->font;

     if (flags & DSTF_OUTLINE) {
          if (!(core_font->attributes & DFFA_OUTLINED))
               return DFB_UNSUPPORTED;

          layers = 2;
     }

     dfb_font_lock( core_font );

     ret = dfb_font_decode_character( core_font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( core_font );
          return ret;
     }

     for (l=0; l<layers; l++) {
          ret = dfb_font_get_glyph_data( core_font, index, l, &glyph[l] );
          if (ret) {
               dfb_font_unlock( core_font );
               return ret;
          }
     }

     if (!(flags & DSTF_TOP)) {
          x += core_font->ascender * core_font->up_unit_x;
          y += core_font->ascender * core_font->up_unit_y;

          if (flags & DSTF_BOTTOM) {
               x -= core_font->descender * core_font->up_unit_x;
               y -= core_font->descender * core_font->up_unit_y;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          if (flags & DSTF_RIGHT) {
               x -= glyph[0]->xadvance;
               y -= glyph[0]->yadvance;
          }
          else if (flags & DSTF_CENTER) {
               x -= glyph[0]->xadvance >> 1;
               y -= glyph[0]->yadvance >> 1;
          }
     }

     dfb_gfxcard_drawglyph( glyph,
                            data->area.wanted.x + x, data->area.wanted.y + y,
                            core_font, layers, &data->state_client );

     dfb_font_unlock( core_font );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetEncoding( IDirectFBSurface  *thiz,
                              DFBTextEncodingID  encoding )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, encoding );

     /* TODO: check for support or fail later? */
     data->encoding = encoding;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSubSurface( IDirectFBSurface    *thiz,
                                const DFBRectangle  *rect,
                                IDirectFBSurface   **surface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!data->surface)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     /* Allocate interface */
     DIRECT_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     if (rect || data->limit_set) {
          DFBRectangle wanted, granted;

          /* Compute wanted rectangle */
          if (rect) {
               wanted = *rect;

               wanted.x += data->area.wanted.x;
               wanted.y += data->area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = data->area.wanted;
          }

          /* Compute granted rectangle */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &data->area.granted );

          /* Construct */
          ret = IDirectFBSurface_Construct( *surface, thiz,
                                            &wanted, &granted, &data->area.insets,
                                            data->surface,
                                            data->caps | DSCAPS_SUBSURFACE, data->core, data->idirectfb );
     }
     else {
          /* Construct */
          ret = IDirectFBSurface_Construct( *surface, thiz,
                                            NULL, NULL, &data->area.insets,
                                            data->surface,
                                            data->caps | DSCAPS_SUBSURFACE, data->core, data->idirectfb );
     }

     return ret;
}

static DFBResult
IDirectFBSurface_MakeSubSurface( IDirectFBSurface   *thiz,
                                 IDirectFBSurface   *from,
                                 const DFBRectangle *rect )
{
     CoreSurface           *surface;
     DFBRectangle           wanted, granted;
     DFBRectangle           full_rect;
     IDirectFBSurface_data *from_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!from)
          return DFB_INVARG;

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     DIRECT_INTERFACE_GET_DATA_FROM(from, from_data, IDirectFBSurface);

     /* Check if CoreSurface is the same */
     if (from_data->surface != surface)
          return DFB_UNSUPPORTED;


     full_rect.x = 0;
     full_rect.y = 0;
     full_rect.w = surface->config.size.w;
     full_rect.h = surface->config.size.h;

     if (rect || from_data->limit_set) {
          /* Compute wanted rectangle */
          if (rect) {
               wanted = *rect;

               wanted.x += from_data->area.wanted.x;
               wanted.y += from_data->area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = from_data->area.wanted;
          }

          /* Compute granted rectangle */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &from_data->area.granted );
     }
     else {
          wanted  = full_rect;
          granted = full_rect;
     }


     data->caps |= DSCAPS_SUBSURFACE;


     data->area.wanted  = wanted;
     data->area.granted = granted;

     data->area.current = data->area.granted;
     dfb_rectangle_intersect( &data->area.current, &full_rect );


     data->state.clip.x1   = data->area.current.x;
     data->state.clip.y1   = data->area.current.y;
     data->state.clip.x2   = data->area.current.x + (data->area.current.w ? : 1) - 1;
     data->state.clip.y2   = data->area.current.y + (data->area.current.h ? : 1) - 1;

     data->state.modified |= SMF_CLIP;


     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetGL( IDirectFBSurface   *thiz,
                        IDirectFBGL       **interface )
{
     DFBResult ret;
     DirectInterfaceFuncs *funcs = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!interface)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;


     ret = DirectGetInterface( &funcs, "IDirectFBGL", NULL, DirectProbeInterface, thiz );
     if (ret)
          return ret;

     ret = funcs->Allocate( (void**)interface );
     if (ret)
          return ret;

     ret = funcs->Construct( *interface, thiz, data->idirectfb );
     if (ret)
          *interface = NULL;

     return ret;
}

static DFBResult
IDirectFBSurface_Dump( IDirectFBSurface   *thiz,
                       const char         *directory,
                       const char         *prefix )
{
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!directory)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->caps & DSCAPS_SUBSURFACE) {
          D_ONCE( "sub surface dumping not supported yet" );
          return DFB_UNSUPPORTED;
     }

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     return dfb_surface_dump_buffer2( surface, CSBR_FRONT, DSSE_LEFT, directory, prefix );
}

static DFBResult
IDirectFBSurface_DisableAcceleration( IDirectFBSurface    *thiz,
                                      DFBAccelerationMask  mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (D_FLAGS_INVALID( mask, DFXL_ALL ))
          return DFB_INVARG;

     data->state.disabled = mask;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_ReleaseSource( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_source( &data->state, NULL );
     dfb_state_set_source_mask( &data->state, NULL );
     dfb_state_set_source2( &data->state, NULL );

     CoreGraphicsState_ReleaseSource( data->state_client.gfx_state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetRenderOptions( IDirectFBSurface        *thiz,
                                   DFBSurfaceRenderOptions  options )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_render_options( &data->state, options );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetMatrix( IDirectFBSurface *thiz,
                            const s32        *matrix )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, matrix );

     if (!matrix)
          return DFB_INVARG;

     dfb_state_set_matrix( &data->state, matrix );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSourceMask( IDirectFBSurface    *thiz,
                                IDirectFBSurface    *mask,
                                int                  x,
                                int                  y,
                                DFBSurfaceMaskFlags  flags )
{
     DFBResult              ret;
     DFBPoint               offset = { x, y };
     IDirectFBSurface_data *mask_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, %d,%d, 0x%04x )\n", __FUNCTION__, thiz, mask, x, y, flags );

     if (!mask || flags & ~DSMF_ALL)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM(mask, mask_data, IDirectFBSurface);

     if (!mask_data->surface)
          return DFB_DESTROYED;

     ret = dfb_state_set_source_mask( &data->state, mask_data->surface );
     if (ret)
          return ret;

     dfb_state_set_source_mask_vals( &data->state, &offset, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetStereoEye( IDirectFBSurface    *thiz,
                               DFBSurfaceStereoEye *ret_eye )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, ret_eye );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     *ret_eye = data->src_eye;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetStereoEye( IDirectFBSurface    *thiz,
                               DFBSurfaceStereoEye  eye )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, eye );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     dfb_state_set_to( &data->state, CSBR_BACK, eye );

     data->src_eye = eye;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FlipStereo( IDirectFBSurface    *thiz,
                             const DFBRegion     *left_region,
                             const DFBRegion     *right_region,
                             DFBSurfaceFlipFlags  flags )
{
     DFBRegion l_reg, r_reg;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h ||
         (left_region && (left_region->x1 > left_region->x2 || left_region->y1 > left_region->y2)) ||
         (right_region && (right_region->x1 > right_region->x2 || right_region->y1 > right_region->y2)))
          return DFB_INVAREA;


     IDirectFBSurface_StopAll( data );

     if (data->parent) {
          IDirectFBSurface_data *parent_data;

          DIRECT_INTERFACE_GET_DATA_FROM( data->parent, parent_data, IDirectFBSurface );

          /* Signal end of sequence of operations. */
          dfb_state_lock( &parent_data->state );
          dfb_state_stop_drawing( &parent_data->state );
          dfb_state_unlock( &parent_data->state );
     }

     dfb_region_from_rectangle( &l_reg, &data->area.current );
     dfb_region_from_rectangle( &r_reg, &data->area.current );

     if (left_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( left_region,
                                                       data->area.wanted.x,
                                                       data->area.wanted.y );

          if (!dfb_region_region_intersect( &l_reg, &clip ))
               return DFB_INVAREA;
     }
     if (right_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( right_region,
                                                       data->area.wanted.x,
                                                       data->area.wanted.y );

          if (!dfb_region_region_intersect( &r_reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> FLIP Left: %4d,%4d-%4dx%4d Right: %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &l_reg ), DFB_RECTANGLE_VALS_FROM_REGION( &r_reg ) );


     CoreGraphicsStateClient_Flush( &data->state_client );

     if (data->surface->config.caps & DSCAPS_FLIPPING) {
          if (!(flags & DSFLIP_BLIT)) {
               if (l_reg.x1 == 0 && l_reg.y1 == 0 &&
                   l_reg.x2 == data->surface->config.size.w  - 1 &&
                   l_reg.y2 == data->surface->config.size.h - 1 &&
                   r_reg.x1 == 0 && r_reg.y1 == 0 &&
                   r_reg.x2 == data->surface->config.size.w  - 1 &&
                   r_reg.y2 == data->surface->config.size.h - 1)
               {
                    CoreSurface_Flip( data->surface, false );
               }
               else
                    CoreSurface_BackToFrontCopy( data->surface, DSSE_LEFT | DSSE_RIGHT, &l_reg, &r_reg );
          }
     }

     dfb_surface_dispatch_update( data->surface, &l_reg, &r_reg );

     IDirectFBSurface_WaitForBackBuffer( data );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetWriteMaskBits( IDirectFBSurface *thiz,
                                   u64               bits )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, MASK 0x%08zx )\n", __FUNCTION__, thiz, (size_t)bits );

     dfb_state_set_write_mask_bits( &data->state, bits );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetRop( IDirectFBSurface      *thiz,
                         DFBSurfaceRopCode      rop_code,
                         const DFBColor        *fg_color,
                         const DFBColor        *bg_color,
                         const u32             *pattern,
                         DFBSurfacePatternMode  pattern_mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, CODE 0x%02x )\n", __FUNCTION__, thiz, rop_code );

     dfb_state_set_rop_code( &data->state, rop_code );
     dfb_state_set_rop_fg_color( &data->state, fg_color );
     dfb_state_set_rop_bg_color( &data->state, bg_color );
     dfb_state_set_rop_pattern( &data->state, pattern, pattern_mode );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKeyExtended( IDirectFBSurface          *thiz,
                                         const DFBColorKeyExtended *colorkey_extended )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_src_colorkey_extended( &data->state, colorkey_extended );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKeyExtended( IDirectFBSurface          *thiz,
                                         const DFBColorKeyExtended *colorkey_extended )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_dst_colorkey_extended( &data->state, colorkey_extended );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawMonoGlyphs( IDirectFBSurface             *thiz,
                                 const void                   *glyphs[],
                                 const DFBMonoGlyphAttributes *attributes,
                                 const DFBPoint               *dest_points,
                                 unsigned int                  num )
{
     int       i, dx, dy;
     DFBPoint *points;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!glyphs || !attributes || !dest_points || num < 1)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     points = alloca( sizeof(DFBPoint) * num );

     for (i=0; i<num; i++) {
          points[i].x = dest_points[i].x + dx;
          points[i].y = dest_points[i].y + dy;
     }

     dfb_gfxcard_draw_mono_glyphs( glyphs, attributes, points, num, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorMatrix( IDirectFBSurface *thiz,
                                    const s32        *matrix )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, matrix );

     if (!matrix)
          return DFB_INVARG;

     dfb_state_set_src_colormatrix( &data->state, matrix );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcConvolution( IDirectFBSurface           *thiz,
                                    const DFBConvolutionFilter *filter )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, filter );

     if (!filter)
          return DFB_INVARG;

     dfb_state_set_src_convolution( &data->state, filter );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetID( IDirectFBSurface *thiz,
                        DFBSurfaceID     *ret_id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_id)
          return DFB_INVARG;

     *ret_id = data->surface->object.id;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_AllowAccess( IDirectFBSurface *thiz,
                              const char       *executable )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     return CoreDFB_AllowSurface( data->core, data->surface, executable, strlen(executable)+1 );
}

static DFBResult
IDirectFBSurface_CreateEventBuffer( IDirectFBSurface      *thiz,
                                    IDirectFBEventBuffer **ret_buffer )
{
     IDirectFBEventBuffer *buffer;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s()\n", __FUNCTION__ );

     if (!data->surface)
          return DFB_DESTROYED;

     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( buffer, NULL, NULL );

     IDirectFBEventBuffer_AttachSurface( buffer, data->surface );

     *ret_buffer = buffer;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_AttachEventBuffer( IDirectFBSurface     *thiz,
                                    IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s()\n", __FUNCTION__ );

     if (!data->surface)
          return DFB_DESTROYED;

     IDirectFBEventBuffer_AttachSurface( buffer, data->surface );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DetachEventBuffer( IDirectFBSurface     *thiz,
                                    IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s()\n", __FUNCTION__ );

     return IDirectFBEventBuffer_DetachSurface( buffer, data->surface );
}

static DFBResult
IDirectFBSurface_MakeClient( IDirectFBSurface *thiz )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s()\n", __FUNCTION__ );

     if (data->surface_client)
          return DFB_BUSY;

     ret = CoreSurface_CreateClient( data->surface, &data->surface_client );
     if (ret)
          return ret;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FrameAck( IDirectFBSurface *thiz,
                           u32               flip_count )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s()\n", __FUNCTION__ );

     if (!data->surface_client)
          return DFB_UNSUPPORTED;

     direct_mutex_lock( &data->surface_client_lock );

     D_DEBUG_AT( Surface, "  -> flip count %d\n", flip_count );

     data->surface_client_flip_count = flip_count;

     CoreSurfaceClient_FrameAck( data->surface_client, flip_count );

     direct_mutex_unlock( &data->surface_client_lock );

     return DFB_OK;
}

/******/

DFBResult IDirectFBSurface_Construct( IDirectFBSurface       *thiz,
                                      IDirectFBSurface       *parent,
                                      DFBRectangle           *wanted,
                                      DFBRectangle           *granted,
                                      DFBInsets              *insets,
                                      CoreSurface            *surface,
                                      DFBSurfaceCapabilities  caps,
                                      CoreDFB                *core,
                                      IDirectFB              *idirectfb )
{
     DFBResult    ret;
     DFBRectangle rect = { 0, 0, surface->config.size.w, surface->config.size.h };

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref       = 1;
     data->caps      = caps | surface->config.caps;
     data->core      = core;
     data->idirectfb = idirectfb;

     if (dfb_surface_ref( surface )) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return DFB_FAILURE;
     }

     if (parent && dfb_config->startstop) {
          IDirectFBSurface_data *parent_data;

          if (parent->AddRef( parent )) {
               dfb_surface_unref( surface );
               DIRECT_DEALLOCATE_INTERFACE(thiz);
               return DFB_FAILURE;
          }

          DIRECT_INTERFACE_GET_DATA_FROM( parent, parent_data, IDirectFBSurface );

          pthread_mutex_lock( &parent_data->children_lock );

          direct_list_append( &parent_data->children_data, &data->link );

          pthread_mutex_unlock( &parent_data->children_lock );

          data->parent = parent;
     }

     pthread_mutex_init( &data->children_lock, NULL );

     direct_waitqueue_init( &data->back_buffer_wq );
     direct_mutex_init( &data->back_buffer_lock );

     direct_mutex_init( &data->surface_client_lock );

     /* The area insets */
     if (insets) {
          data->area.insets = *insets;
          dfb_rectangle_subtract( &rect, insets );
     }

     /* The area that was requested */
     if (wanted)
          data->area.wanted = *wanted;
     else
          data->area.wanted = rect;

     /* The area that will never be exceeded */
     if (granted)
          data->area.granted = *granted;
     else
          data->area.granted = data->area.wanted;

     /* The currently accessible rectangle */
     data->area.current = data->area.granted;
     dfb_rectangle_intersect( &data->area.current, &rect );

     /* Whether granted rectangle is meaningful */
     data->limit_set = (granted != NULL);

     data->surface = surface;

     dfb_state_init( &data->state, core );
     dfb_state_set_destination( &data->state, surface );

     data->state.clip.x1  = data->area.current.x;
     data->state.clip.y1  = data->area.current.y;
     data->state.clip.x2  = data->area.current.x + (data->area.current.w ? : 1) - 1;
     data->state.clip.y2  = data->area.current.y + (data->area.current.h ? : 1) - 1;

     data->state.modified = SMF_ALL;

     data->src_eye = DSSE_LEFT;

     ret = CoreGraphicsStateClient_Init( &data->state_client, &data->state );
     if (ret)
          return ret;    // FIXME: deinit

     if (data->surface->config.flags & CSCONF_PREALLOCATED) {

          ret = register_prealloc( data );

          if (ret)
               return ret;    // FIXME: deinit
     }

     thiz->AddRef = IDirectFBSurface_AddRef;
     thiz->Release = IDirectFBSurface_Release;

     thiz->GetCapabilities = IDirectFBSurface_GetCapabilities;
     thiz->GetPosition = IDirectFBSurface_GetPosition;
     thiz->GetSize = IDirectFBSurface_GetSize;
     thiz->GetVisibleRectangle = IDirectFBSurface_GetVisibleRectangle;
     thiz->GetPixelFormat = IDirectFBSurface_GetPixelFormat;
     thiz->GetColorSpace = IDirectFBSurface_GetColorSpace;
     thiz->GetAccelerationMask = IDirectFBSurface_GetAccelerationMask;

     thiz->GetPalette = IDirectFBSurface_GetPalette;
     thiz->SetPalette = IDirectFBSurface_SetPalette;
     thiz->SetAlphaRamp = IDirectFBSurface_SetAlphaRamp;

     thiz->Lock = IDirectFBSurface_Lock;
     thiz->GetFramebufferOffset = IDirectFBSurface_GetFramebufferOffset;
     thiz->GetPhysicalAddress = IDirectFBSurface_GetPhysicalAddress;
     thiz->Unlock = IDirectFBSurface_Unlock;
     thiz->Flip = IDirectFBSurface_Flip;
     thiz->SetField = IDirectFBSurface_SetField;
     thiz->Clear = IDirectFBSurface_Clear;

     thiz->SetClip = IDirectFBSurface_SetClip;
     thiz->GetClip = IDirectFBSurface_GetClip;
     thiz->SetColor = IDirectFBSurface_SetColor;
     thiz->SetColorIndex = IDirectFBSurface_SetColorIndex;
     thiz->SetSrcBlendFunction = IDirectFBSurface_SetSrcBlendFunction;
     thiz->SetDstBlendFunction = IDirectFBSurface_SetDstBlendFunction;
     thiz->SetPorterDuff = IDirectFBSurface_SetPorterDuff;
     thiz->SetSrcColorKey = IDirectFBSurface_SetSrcColorKey;
     thiz->SetSrcColorKeyIndex = IDirectFBSurface_SetSrcColorKeyIndex;
     thiz->SetDstColorKey = IDirectFBSurface_SetDstColorKey;
     thiz->SetDstColorKeyIndex = IDirectFBSurface_SetDstColorKeyIndex;
     thiz->SetIndexTranslation = IDirectFBSurface_SetIndexTranslation;

     thiz->SetBlittingFlags = IDirectFBSurface_SetBlittingFlags;
     thiz->Blit = IDirectFBSurface_Blit;
     thiz->TileBlit = IDirectFBSurface_TileBlit;
     thiz->BatchBlit = IDirectFBSurface_BatchBlit;
     thiz->BatchBlit2 = IDirectFBSurface_BatchBlit2;
     thiz->StretchBlit = IDirectFBSurface_StretchBlit;
     thiz->BatchStretchBlit = IDirectFBSurface_BatchStretchBlit;
     thiz->TextureTriangles = IDirectFBSurface_TextureTriangles;

     thiz->SetDrawingFlags = IDirectFBSurface_SetDrawingFlags;
     thiz->FillRectangle = IDirectFBSurface_FillRectangle;
     thiz->DrawLine = IDirectFBSurface_DrawLine;
     thiz->DrawLines = IDirectFBSurface_DrawLines;
     thiz->DrawRectangle = IDirectFBSurface_DrawRectangle;
     thiz->FillTriangle = IDirectFBSurface_FillTriangle;
     thiz->FillRectangles = IDirectFBSurface_FillRectangles;
     thiz->FillSpans = IDirectFBSurface_FillSpans;
     thiz->FillTriangles = IDirectFBSurface_FillTriangles;
     thiz->FillTrapezoids = IDirectFBSurface_FillTrapezoids;

     thiz->SetFont = IDirectFBSurface_SetFont;
     thiz->GetFont = IDirectFBSurface_GetFont;
     thiz->DrawString = IDirectFBSurface_DrawString;
     thiz->DrawGlyph = IDirectFBSurface_DrawGlyph;
     thiz->SetEncoding = IDirectFBSurface_SetEncoding;

     thiz->GetSubSurface = IDirectFBSurface_GetSubSurface;

     thiz->GetGL = IDirectFBSurface_GetGL;

     thiz->Dump = IDirectFBSurface_Dump;
     thiz->DisableAcceleration = IDirectFBSurface_DisableAcceleration;
     thiz->ReleaseSource = IDirectFBSurface_ReleaseSource;

     thiz->SetRenderOptions = IDirectFBSurface_SetRenderOptions;
     thiz->SetMatrix        = IDirectFBSurface_SetMatrix;
     thiz->SetSourceMask    = IDirectFBSurface_SetSourceMask;

     thiz->MakeSubSurface = IDirectFBSurface_MakeSubSurface;

     thiz->Write = IDirectFBSurface_Write;
     thiz->Read  = IDirectFBSurface_Read;

     thiz->SetColors = IDirectFBSurface_SetColors;

     thiz->GetStereoEye = IDirectFBSurface_GetStereoEye;
     thiz->SetStereoEye = IDirectFBSurface_SetStereoEye;
     thiz->FlipStereo   = IDirectFBSurface_FlipStereo;

     thiz->SetWriteMaskBits = IDirectFBSurface_SetWriteMaskBits;
     thiz->SetRop = IDirectFBSurface_SetRop;

     thiz->SetSrcColorKeyExtended = IDirectFBSurface_SetSrcColorKeyExtended;
     thiz->SetDstColorKeyExtended = IDirectFBSurface_SetDstColorKeyExtended;

     thiz->DrawMonoGlyphs = IDirectFBSurface_DrawMonoGlyphs;

     thiz->SetSrcColorMatrix = IDirectFBSurface_SetSrcColorMatrix;
     thiz->SetSrcConvolution = IDirectFBSurface_SetSrcConvolution;

     thiz->GetID       = IDirectFBSurface_GetID;
     thiz->AllowAccess = IDirectFBSurface_AllowAccess;

     thiz->CreateEventBuffer = IDirectFBSurface_CreateEventBuffer;
     thiz->AttachEventBuffer = IDirectFBSurface_AttachEventBuffer;
     thiz->DetachEventBuffer = IDirectFBSurface_DetachEventBuffer;

     thiz->MakeClient = IDirectFBSurface_MakeClient;
     thiz->FrameAck   = IDirectFBSurface_FrameAck;

     dfb_surface_attach( surface,
                         IDirectFBSurface_listener, thiz, &data->reaction );

     return DFB_OK;
}


/* internal */

static ReactionResult
IDirectFBSurface_listener( const void *msg_data, void *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     IDirectFBSurface              *thiz         = ctx;
     IDirectFBSurface_data         *data         = thiz->priv;
     CoreSurface                   *surface      = data->surface;

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, msg_data, ctx );

     if (notification->flags & CSNF_DESTROY) {
          if (data->surface) {
               data->surface = NULL;
               dfb_surface_unref( surface );
          }
          return RS_REMOVE;
     }

     if (notification->flags & CSNF_SIZEFORMAT) {
          DFBRectangle rect = { 0, 0, surface->config.size.w, surface->config.size.h };

          dfb_rectangle_subtract( &rect, &data->area.insets );

          if (data->limit_set) {
               data->area.current = data->area.granted;

               dfb_rectangle_intersect( &data->area.current, &rect );
          }
          else
               data->area.wanted = data->area.granted = data->area.current = rect;

          /* Reset clip to avoid crashes caused by drawing out of bounds. */
          if (data->clip_set)
               thiz->SetClip( thiz, &data->clip_wanted );
          else
               thiz->SetClip( thiz, NULL );
     }

     if (notification->flags & CSNF_FRAME) {
          direct_mutex_lock( &data->back_buffer_lock );

          D_DEBUG_AT( Surface, "  -> frame ack %d\n", notification->flip_count );

          data->frame_ack = notification->flip_count;

          direct_waitqueue_broadcast( &data->back_buffer_wq );

          direct_mutex_unlock( &data->back_buffer_lock );
     }

     return RS_OK;
}

void
IDirectFBSurface_StopAll( IDirectFBSurface_data *data )
{
     if (!dfb_config->startstop)
          return;

     if (data->children_data) {
          IDirectFBSurface_data *child;

          pthread_mutex_lock( &data->children_lock );

          direct_list_foreach (child, data->children_data)
               IDirectFBSurface_StopAll( child );

          pthread_mutex_unlock( &data->children_lock );
     }

     /* Signal end of sequence of operations. */
     dfb_state_lock( &data->state );
     dfb_state_stop_drawing( &data->state );
     dfb_state_unlock( &data->state );
}

void
IDirectFBSurface_WaitForBackBuffer( IDirectFBSurface_data *data )
{
     CoreSurface *surface;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, data );

     surface = data->surface;
     CORE_SURFACE_ASSERT( surface );

     direct_mutex_lock( &data->back_buffer_lock );

     if (data->frame_ack) {
          while (surface->flips - data->frame_ack >= surface->num_buffers-1) {
               D_DEBUG_AT( Surface, "  -> waiting for back buffer... (surface %d, notify %d)\n",
                           surface->flips, data->frame_ack );

               direct_waitqueue_wait( &data->back_buffer_wq, &data->back_buffer_lock );
          }
     }
     else
          D_DEBUG_AT( Surface, "  -> no display notify received yet\n" );

     D_DEBUG_AT( Surface, "  -> OK\n" );

     direct_mutex_unlock( &data->back_buffer_lock );
}

