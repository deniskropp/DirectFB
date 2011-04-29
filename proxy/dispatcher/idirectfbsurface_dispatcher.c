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

#include <math.h>


#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <idirectfbsurface_requestor.h>

#include "idirectfbsurface_dispatcher.h"

static DFBResult Probe( void );
static DFBResult Construct( IDirectFBSurface *thiz,
                            IDirectFBSurface *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBSurface, Dispatcher )

/**************************************************************************************************/

/*
 * private data struct of IDirectFBSurface_Dispatcher
 */
typedef struct {
     int                    ref;      /* reference counter */

     IDirectFBSurface      *real;

     VoodooInstanceID       super;
     VoodooInstanceID       self;

     VoodooManager         *manager;

     VoodooInstanceID       remote;
} IDirectFBSurface_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBSurface_Dispatcher_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Dispatcher_data *data;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data = thiz->priv;

     voodoo_manager_unregister_local( data->manager, data->self );

     data->real->Release( data->real );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDirectFBSurface_Dispatcher_AddRef( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBSurface_Dispatcher_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (--data->ref == 0)
          IDirectFBSurface_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetCapabilities( IDirectFBSurface       *thiz,
                                             DFBSurfaceCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!caps)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetPosition( IDirectFBSurface *thiz,
                                         int              *x,
                                         int              *y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!x && !y)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetSize( IDirectFBSurface *thiz,
                                     int              *width,
                                     int              *height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!width && !height)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetVisibleRectangle( IDirectFBSurface *thiz,
                                                 DFBRectangle     *rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!rect)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetPixelFormat( IDirectFBSurface      *thiz,
                                            DFBSurfacePixelFormat *format )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!format)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetAccelerationMask( IDirectFBSurface    *thiz,
                                                 IDirectFBSurface    *source,
                                                 DFBAccelerationMask *mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!mask)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetPalette( IDirectFBSurface  *thiz,
                                        IDirectFBPalette **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!interface_ptr)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetPalette( IDirectFBSurface *thiz,
                                        IDirectFBPalette *palette )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!palette)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetAlphaRamp( IDirectFBSurface *thiz,
                                          u8 a0, u8 a1, u8 a2, u8 a3 )

{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Lock( IDirectFBSurface *thiz,
                                  DFBSurfaceLockFlags flags,
                                  void **ret_ptr, int *ret_pitch )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!flags || !ret_ptr || !ret_pitch)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetFramebufferOffset( IDirectFBSurface *thiz,
                                                  int              *offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!offset)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetPhysicalAddress( IDirectFBSurface *thiz,
                                                unsigned long    *addr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!addr)
          return DFB_INVARG;

      return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Unlock( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Flip( IDirectFBSurface    *thiz,
                                  const DFBRegion     *region,
                                  DFBSurfaceFlipFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetField( IDirectFBSurface    *thiz,
                                      int                  field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (field < 0 || field > 1)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Clear( IDirectFBSurface *thiz,
                                   u8 r, u8 g, u8 b, u8 a )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetClip( IDirectFBSurface *thiz,
                                     const DFBRegion  *clip )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetClip( IDirectFBSurface *thiz,
                                     DFBRegion        *clip )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!clip)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetColor( IDirectFBSurface *thiz,
                                      u8 r, u8 g, u8 b, u8 a )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetColorIndex( IDirectFBSurface *thiz,
                                           unsigned int      index )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetSrcBlendFunction( IDirectFBSurface        *thiz,
                                                 DFBSurfaceBlendFunction  src )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetDstBlendFunction( IDirectFBSurface        *thiz,
                                                 DFBSurfaceBlendFunction  dst )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetPorterDuff( IDirectFBSurface         *thiz,
                                           DFBSurfacePorterDuffRule  rule )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetSrcColorKey( IDirectFBSurface *thiz,
                                            u8                r,
                                            u8                g,
                                            u8                b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetSrcColorKeyIndex( IDirectFBSurface *thiz,
                                                 unsigned int      index )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetDstColorKey( IDirectFBSurface *thiz,
                                            u8                r,
                                            u8                g,
                                            u8                b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetDstColorKeyIndex( IDirectFBSurface *thiz,
                                                 unsigned int      index )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetFont( IDirectFBSurface *thiz,
                                     IDirectFBFont    *font )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetFont( IDirectFBSurface  *thiz,
                                     IDirectFBFont    **font )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetDrawingFlags( IDirectFBSurface       *thiz,
                                             DFBSurfaceDrawingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_FillRectangle( IDirectFBSurface *thiz,
                                           int x, int y, int w, int h )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_FillRectangles( IDirectFBSurface   *thiz,
                                            const DFBRectangle *rects,
                                            unsigned int        num_rects )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_FillTrapezoids( IDirectFBSurface   *thiz,
                                            const DFBTrapezoid *traps,
                                            unsigned int        num_traps )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_FillSpans( IDirectFBSurface *thiz,
                                       int               y,
                                       const DFBSpan    *spans,
                                       unsigned int      num_spans )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}


static DFBResult
IDirectFBSurface_Dispatcher_DrawLine( IDirectFBSurface *thiz,
                                      int x1, int y1, int x2, int y2 )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_DrawLines( IDirectFBSurface *thiz,
                                       const DFBRegion  *lines,
                                       unsigned int      num_lines )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!lines || !num_lines)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_DrawRectangle( IDirectFBSurface *thiz,
                                           int x, int y, int w, int h )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (w<=0 || h<=0)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_FillTriangle( IDirectFBSurface *thiz,
                                          int x1, int y1,
                                          int x2, int y2,
                                          int x3, int y3 )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetBlittingFlags( IDirectFBSurface        *thiz,
                                              DFBSurfaceBlittingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Blit( IDirectFBSurface   *thiz,
                                  IDirectFBSurface   *source,
                                  const DFBRectangle *rect,
                                  int                 x,
                                  int                 y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!source)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_TileBlit( IDirectFBSurface   *thiz,
                                      IDirectFBSurface   *source,
                                      const DFBRectangle *rect,
                                      int                 x,
                                      int                 y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!source)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_BatchBlit( IDirectFBSurface   *thiz,
                                       IDirectFBSurface   *source,
                                       const DFBRectangle *source_rects,
                                       const DFBPoint     *dest_points,
                                       int                 num )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!source || !source_rects || !dest_points || num < 1)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_StretchBlit( IDirectFBSurface   *thiz,
                                         IDirectFBSurface   *source,
                                         const DFBRectangle *source_rect,
                                         const DFBRectangle *destination_rect )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!source)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_TextureTriangles( IDirectFBSurface     *thiz,
                                              IDirectFBSurface     *source,
                                              const DFBVertex      *vertices,
                                              const int            *indices,
                                              int                   num,
                                              DFBTriangleFormation  formation )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!source || !vertices || num < 3)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_DrawString( IDirectFBSurface *thiz,
                                        const char *text, int bytes,
                                        int x, int y,
                                        DFBSurfaceTextFlags flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!text)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_DrawGlyph( IDirectFBSurface *thiz,
                                       unsigned int index, int x, int y,
                                       DFBSurfaceTextFlags flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!index)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetEncoding( IDirectFBSurface  *thiz,
                                         DFBTextEncodingID  encoding )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetSubSurface( IDirectFBSurface    *thiz,
                                           const DFBRectangle  *rect,
                                           IDirectFBSurface   **surface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!surface)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_GetGL( IDirectFBSurface   *thiz,
                                   IDirectFBGL       **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!interface_ptr)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_Dump( IDirectFBSurface   *thiz,
                                  const char         *directory,
                                  const char         *prefix )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     if (!directory || !prefix)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_DisableAcceleration( IDirectFBSurface   *thiz,
                                                 DFBAccelerationMask mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_ReleaseSource( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBSurface_Dispatcher_SetIndexTranslation( IDirectFBSurface *thiz,
                                                 const int        *indices,
                                                 int               num_indices )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Release( IDirectFBSurface *thiz, IDirectFBSurface *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     return thiz->Release( thiz );
}

static DirectResult
Dispatch_GetPosition( IDirectFBSurface *thiz, IDirectFBSurface *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DFBResult ret;
     DFBPoint  position;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     ret = real->GetPosition( real, &position.x, &position.y );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBPoint), &position,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetSize( IDirectFBSurface *thiz, IDirectFBSurface *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DFBResult    ret;
     DFBDimension dimension;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     ret = real->GetSize( real, &dimension.w, &dimension.h );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBDimension), &dimension,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetVisibleRectangle( IDirectFBSurface *thiz, IDirectFBSurface *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DFBResult    ret;
     DFBRectangle rect;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     ret = real->GetVisibleRectangle( real, &rect );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBRectangle), &rect,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetPixelFormat( IDirectFBSurface *thiz, IDirectFBSurface *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DFBResult             ret;
     DFBSurfacePixelFormat format;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     ret = real->GetPixelFormat( real, &format );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, format,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetAccelerationMask( IDirectFBSurface *thiz, IDirectFBSurface *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     void                *surface = NULL;
     DFBAccelerationMask  mask;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     if (instance != VOODOO_INSTANCE_NONE) {
          ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
          if (ret)
               return ret;
     }

     ret = real->GetAccelerationMask( real, surface, &mask );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, mask,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetPalette( IDirectFBSurface *thiz, IDirectFBSurface *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult      ret;
     IDirectFBPalette *palette;
     VoodooInstanceID  instance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     ret = real->GetPalette( real, &palette );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBPalette", palette,
                                        data->super, NULL, &instance, NULL );
     if (ret) {
          palette->Release( palette );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetPalette( IDirectFBSurface *thiz, IDirectFBSurface *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     void                *palette;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &palette );
     if (ret)
          return ret;

     real->SetPalette( real, palette );

     return DFB_OK;
}

static DirectResult
Dispatch_Flip( IDirectFBSurface *thiz, IDirectFBSurface *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const DFBRegion     *region;
     DFBSurfaceFlipFlags  flags;
     unsigned int         millis;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ODATA( parser, region );
     VOODOO_PARSER_GET_INT( parser, flags );
     if (data->remote)
          VOODOO_PARSER_GET_UINT( parser, millis );
     VOODOO_PARSER_END( parser );

     ret = real->Flip( real, region, flags );

     if (data->remote)
          voodoo_manager_request( manager, data->remote,
                                  IDIRECTFBSURFACE_REQUESTOR_METHOD_ID_FlipNotify, VREQ_NONE, NULL,
                                  VMBT_UINT, millis,
                                  VMBT_NONE );

     if (flags & DSFLIP_WAIT)
          return voodoo_manager_respond( manager, true, msg->header.serial,
                                         ret, VOODOO_INSTANCE_NONE,
                                         VMBT_NONE );

     return DFB_OK;
}

static DirectResult
Dispatch_Clear( IDirectFBSurface *thiz, IDirectFBSurface *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBColor      *color;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, color );
     VOODOO_PARSER_END( parser );

     real->Clear( real, color->r, color->g, color->b, color->a );

     return DFB_OK;
}

static DirectResult
Dispatch_SetClip( IDirectFBSurface *thiz, IDirectFBSurface *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBRegion     *clip;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ODATA( parser, clip );
     VOODOO_PARSER_END( parser );

     real->SetClip( real, clip );

     return DFB_OK;
}

static DirectResult
Dispatch_SetColor( IDirectFBSurface *thiz, IDirectFBSurface *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBColor      *color;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, color );
     VOODOO_PARSER_END( parser );

     real->SetColor( real, color->r, color->g, color->b, color->a );

     return DFB_OK;
}

static DirectResult
Dispatch_SetSrcColorKey( IDirectFBSurface *thiz, IDirectFBSurface *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBColor      *color;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, color );
     VOODOO_PARSER_END( parser );

     real->SetSrcColorKey( real, color->r, color->g, color->b );

     return DFB_OK;
}

static DirectResult
Dispatch_SetSrcColorKeyIndex( IDirectFBSurface *thiz, IDirectFBSurface *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     unsigned int        index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_END( parser );

     real->SetSrcColorKeyIndex( real, index );

     return DFB_OK;
}

static DirectResult
Dispatch_SetDstColorKey( IDirectFBSurface *thiz, IDirectFBSurface *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBColor      *color;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, color );
     VOODOO_PARSER_END( parser );

     real->SetDstColorKey( real, color->r, color->g, color->b );

     return DFB_OK;
}

static DirectResult
Dispatch_SetDstColorKeyIndex( IDirectFBSurface *thiz, IDirectFBSurface *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     unsigned int        index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_END( parser );

     real->SetDstColorKeyIndex( real, index );

     return DFB_OK;
}

static DirectResult
Dispatch_SetBlittingFlags( IDirectFBSurface *thiz, IDirectFBSurface *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser     parser;
     DFBSurfaceBlittingFlags flags;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, flags );
     VOODOO_PARSER_END( parser );

     real->SetBlittingFlags( real, flags );

     return DFB_OK;
}

static DirectResult
Dispatch_Blit( IDirectFBSurface *thiz, IDirectFBSurface *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     const DFBRectangle  *rect;
     const DFBPoint      *point;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_ODATA( parser, rect );
     VOODOO_PARSER_GET_DATA( parser, point );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->Blit( real, surface, rect, point->x, point->y );

     return DFB_OK;
}

static DirectResult
Dispatch_TileBlit( IDirectFBSurface *thiz, IDirectFBSurface *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     const DFBRectangle  *rect;
     const DFBPoint      *point;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_ODATA( parser, rect );
     VOODOO_PARSER_GET_DATA( parser, point );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->TileBlit( real, surface, rect, point->x, point->y );

     return DFB_OK;
}

static DirectResult
Dispatch_BatchBlit( IDirectFBSurface *thiz, IDirectFBSurface *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     unsigned int         num;
     const DFBRectangle  *source_rects;
     const DFBPoint      *dest_points;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_UINT( parser, num );
     VOODOO_PARSER_GET_DATA( parser, source_rects );
     VOODOO_PARSER_GET_DATA( parser, dest_points );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->BatchBlit( real, surface, source_rects, dest_points, num );

     return DFB_OK;
}

static DirectResult
Dispatch_StretchBlit( IDirectFBSurface *thiz, IDirectFBSurface *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     const DFBRectangle  *srect;
     const DFBRectangle  *drect;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_ODATA( parser, srect );
     VOODOO_PARSER_GET_ODATA( parser, drect );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->StretchBlit( real, surface, srect, drect );

     return DFB_OK;
}

static DirectResult
Dispatch_TextureTriangles( IDirectFBSurface *thiz, IDirectFBSurface *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     VoodooMessageParser   parser;
     VoodooInstanceID      instance;
     const DFBVertex      *vertices;
     const int            *indices;
     int                   num;
     DFBTriangleFormation  formation;
     void                 *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_DATA( parser, vertices );
     VOODOO_PARSER_GET_ODATA( parser, indices );
     VOODOO_PARSER_GET_INT( parser, num );
     VOODOO_PARSER_GET_INT( parser, formation );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->TextureTriangles( real, surface, vertices, indices, num, formation );

     return DFB_OK;
}

static DirectResult
Dispatch_SetDrawingFlags( IDirectFBSurface *thiz, IDirectFBSurface *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser    parser;
     DFBSurfaceDrawingFlags flags;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, flags );
     VOODOO_PARSER_END( parser );

     real->SetDrawingFlags( real, flags );

     return DFB_OK;
}

static DirectResult
Dispatch_FillRectangle( IDirectFBSurface *thiz, IDirectFBSurface *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBRectangle  *rect;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, rect );
     VOODOO_PARSER_END( parser );

     real->FillRectangle( real, rect->x, rect->y, rect->w, rect->h );

     return DFB_OK;
}

static DirectResult
Dispatch_FillRectangles( IDirectFBSurface *thiz, IDirectFBSurface *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     unsigned int         num_rects;
     const DFBRectangle  *rects;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, num_rects );
     VOODOO_PARSER_GET_DATA( parser, rects );
     VOODOO_PARSER_END( parser );

     real->FillRectangles( real, rects, num_rects );

     return DFB_OK;
}

static DirectResult
Dispatch_FillTrapezoids( IDirectFBSurface *thiz, IDirectFBSurface *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     unsigned int         num_traps;
     const DFBTrapezoid  *traps;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, num_traps );
     VOODOO_PARSER_GET_DATA( parser, traps );
     VOODOO_PARSER_END( parser );

     real->FillTrapezoids( real, traps, num_traps );

     return DFB_OK;
}

static DirectResult
Dispatch_FillSpans( IDirectFBSurface *thiz, IDirectFBSurface *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     int                  y;
     unsigned int         num_spans;
     const DFBSpan       *spans;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, y );
     VOODOO_PARSER_GET_UINT( parser, num_spans );
     VOODOO_PARSER_GET_DATA( parser, spans );
     VOODOO_PARSER_END( parser );

     real->FillSpans( real, y, spans, num_spans );

     return DFB_OK;
}

static DirectResult
Dispatch_DrawLine( IDirectFBSurface *thiz, IDirectFBSurface *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBRegion     *line;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, line );
     VOODOO_PARSER_END( parser );

     real->DrawLine( real, line->x1, line->y1, line->x2, line->y2 );

     return DFB_OK;
}

static DirectResult
Dispatch_DrawLines( IDirectFBSurface *thiz, IDirectFBSurface *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     unsigned int         num_lines;
     const DFBRegion     *lines;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, num_lines );
     VOODOO_PARSER_GET_DATA( parser, lines );
     VOODOO_PARSER_END( parser );

     real->DrawLines( real, lines, num_lines );

     return DFB_OK;
}

static DirectResult
Dispatch_DrawRectangle( IDirectFBSurface *thiz, IDirectFBSurface *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBRectangle  *rect;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, rect );
     VOODOO_PARSER_END( parser );

     real->DrawRectangle( real, rect->x, rect->y, rect->w, rect->h );

     return DFB_OK;
}

static DirectResult
Dispatch_FillTriangle( IDirectFBSurface *thiz, IDirectFBSurface *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBTriangle   *tri;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, tri );
     VOODOO_PARSER_END( parser );

     real->FillTriangle( real, tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3 );

     return DFB_OK;
}

static DirectResult
Dispatch_SetFont( IDirectFBSurface *thiz, IDirectFBSurface *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     void                *font;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     if (instance == VOODOO_INSTANCE_NONE) {
          real->SetFont( real, NULL );

          return DFB_OK;
     }

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &font );
     if (ret)
          return ret;

     real->SetFont( real, font );

     return DFB_OK;
}

static DirectResult
Dispatch_DrawString( IDirectFBSurface *thiz, IDirectFBSurface *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const char          *text;
     int                  bytes;
     const DFBPoint      *point;
     DFBSurfaceTextFlags  flags;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, text );
     VOODOO_PARSER_GET_INT( parser, bytes );
     VOODOO_PARSER_GET_DATA( parser, point );
     VOODOO_PARSER_GET_INT( parser, flags );
     VOODOO_PARSER_END( parser );

     real->DrawString( real, text, bytes, point->x, point->y, flags );

     return DFB_OK;
}

static DirectResult
Dispatch_DrawGlyph( IDirectFBSurface *thiz, IDirectFBSurface *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     unsigned int         index;
     const DFBPoint      *point;
     DFBSurfaceTextFlags  flags;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_GET_DATA( parser, point );
     VOODOO_PARSER_GET_INT( parser, flags );
     VOODOO_PARSER_END( parser );

     real->DrawGlyph( real, index, point->x, point->y, flags );

     return DFB_OK;
}

static DirectResult
Dispatch_SetEncoding( IDirectFBSurface *thiz, IDirectFBSurface *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     DFBTextEncodingID   encoding;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, encoding );
     VOODOO_PARSER_END( parser );

     real->SetEncoding( real, encoding );

     return DFB_OK;
}

static DirectResult
Dispatch_GetSubSurface( IDirectFBSurface *thiz, IDirectFBSurface *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     const DFBRectangle  *rect;
     IDirectFBSurface    *surface;
     VoodooInstanceID     instance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ODATA( parser, rect );
     VOODOO_PARSER_END( parser );

     ret = real->GetSubSurface( real, rect, &surface );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBSurface", surface,
                                        data->super, NULL, &instance, NULL );
     if (ret) {
          surface->Release( surface );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_DisableAcceleration( IDirectFBSurface *thiz, IDirectFBSurface *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     DFBAccelerationMask mask;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, mask );
     VOODOO_PARSER_END( parser );

     real->DisableAcceleration( real, mask );

     return DFB_OK;
}

static DirectResult
Dispatch_ReleaseSource( IDirectFBSurface *thiz, IDirectFBSurface *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     real->ReleaseSource( real );

     return DFB_OK;
}

#define RLE16_KEY   0xf001

static void
rle16_decode( const u16    *src,
              u16          *dst,
              unsigned int  num )
{
     unsigned int n = 0, last, count, out = 0;

     while (out < num) {
          last = src[n++];

          if (last == RLE16_KEY) {
               count = src[n++];

               if (count == RLE16_KEY) {
                    dst[out++] = RLE16_KEY;
               }
               else {
                    last = src[n++];

                    while (count >= 4) {
                         dst[out+0] =
                         dst[out+1] =
                         dst[out+2] =
                         dst[out+3] = last;

                         out   += 4;
                         count -= 4;
                    }

                    while (count >= 2) {
                         dst[out+0] =
                         dst[out+1] = last;

                         out   += 2;
                         count -= 2;
                    }

                    while (count--)
                         dst[out++] = last;
               }
          }
          else
               dst[out++] = last;
     }

     D_ASSERT( out == num );
}

#define RLE32_KEY   0xf0012345

static void
rle32_decode( const u32    *src,
              u32          *dst,
              unsigned int  num )
{
     unsigned int n = 0, last, count, out = 0;

     while (out < num) {
          last = src[n++];

          if (last == RLE32_KEY) {
               count = src[n++];

               if (count == RLE32_KEY) {
                    dst[out++] = RLE32_KEY;
               }
               else {
                    last = src[n++];

                    while (count >= 4) {
                         dst[out+0] =
                         dst[out+1] =
                         dst[out+2] =
                         dst[out+3] = last;

                         out   += 4;
                         count -= 4;
                    }

                    while (count >= 2) {
                         dst[out+0] =
                         dst[out+1] = last;

                         out   += 2;
                         count -= 2;
                    }

                    while (count--)
                         dst[out++] = last;
               }
          }
          else
               dst[out++] = last;
     }

     D_ASSERT( out == num );
}

static DirectResult
Dispatch_Write( IDirectFBSurface *thiz, IDirectFBSurface *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     unsigned int         encoded;
     const DFBRectangle  *rect;
     const void          *ptr;
     int                  pitch;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, encoded );
     VOODOO_PARSER_GET_DATA( parser, rect );
     VOODOO_PARSER_GET_DATA( parser, ptr );
     VOODOO_PARSER_GET_INT( parser, pitch );
     VOODOO_PARSER_END( parser );

     if (encoded) {
          switch (encoded) {
               case 2: {
                    if (rect->w > 2048) {
                         u16 *buf = D_MALLOC( rect->w * 2 );

                         if (buf) {
                              rle16_decode( ptr, buf, rect->w );

                              real->Write( real, rect, buf, pitch );

                              D_FREE( buf );
                         }
                         else
                              D_OOM();
                    }
                    else {
                         u16 buf[2048];

                         rle16_decode( ptr, buf, rect->w );

                         real->Write( real, rect, buf, pitch );
                    }
                    break;
               }

               case 4: {
                    if (rect->w > 1024) {
                         u32 *buf = D_MALLOC( rect->w * 4 );

                         if (buf) {
                              rle32_decode( ptr, buf, rect->w );

                              real->Write( real, rect, buf, pitch );

                              D_FREE( buf );
                         }
                         else
                              D_OOM();
                    }
                    else {
                         u32 buf[1024];

                         rle32_decode( ptr, buf, rect->w );

                         real->Write( real, rect, buf, pitch );
                    }
                    break;
               }

               default:
                    D_UNIMPLEMENTED();
                    break;
          }
     }
     else
          real->Write( real, rect, ptr, pitch );

     return DFB_OK;
}

#define RLE16_KEY   0xf001

static bool
rle16_encode( const u16    *src,
              u16          *dst,
              unsigned int  num,
              unsigned int *ret_num )
{
     unsigned int n, last, count = 0, out = 0;

     for (n=0; n<num; n++) {
          if (out + 3 > num) {
               *ret_num = num;
               return false;
          }

          if (count > 0) {
               D_ASSERT( src[n] == last );

               count++;
          }
          else {
               count = 1;
               last  = src[n];
          }

          if (n == num-1 || src[n+1] != last) {
               if (count > 2 || (count > 1 && last == RLE16_KEY)) {
                    dst[out++] = RLE16_KEY;
                    dst[out++] = count;
                    dst[out++] = last;
               }
               else {
                    if (count > 1 || last == RLE16_KEY)
                         dst[out++] = last;

                    dst[out++] = last;
               }

               count = 0;
          }
     }

     *ret_num = out;

     return true;
}

#define RLE32_KEY   0xf0012345

static bool
rle32_encode( const u32    *src,
              u32          *dst,
              unsigned int  num,
              unsigned int *ret_num )
{
     unsigned int n, last, count = 0, out = 0;

     for (n=0; n<num; n++) {
          if (out + 3 > num) {
               *ret_num = num;
               return false;
          }

          if (count > 0) {
               D_ASSERT( src[n] == last );

               count++;
          }
          else {
               count = 1;
               last  = src[n];
          }

          if (n == num-1 || src[n+1] != last) {
               if (count > 2 || (count > 1 && last == RLE32_KEY)) {
                    dst[out++] = RLE32_KEY;
                    dst[out++] = count;
                    dst[out++] = last;
               }
               else {
                    if (count > 1 || last == RLE32_KEY)
                         dst[out++] = last;

                    dst[out++] = last;
               }

               count = 0;
          }
     }

     *ret_num = out;

     return true;
}

static DirectResult
Dispatch_Read( IDirectFBSurface *thiz, IDirectFBSurface *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult           ret;
     VoodooMessageParser    parser;
     const DFBRectangle    *rect;
     int                    len;
     int                    y;
     void                  *buf;
     DFBSurfacePixelFormat  format;
     unsigned int           encoded;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, rect );
     VOODOO_PARSER_END( parser );

     real->GetPixelFormat( real, &format );

     len = DFB_BYTES_PER_LINE( format, rect->w );
     buf = alloca( len );


     switch (format) {
          case DSPF_RGB16: {
               u16 tmp[rect->w];

               for (y=0; y<rect->h; y++) {
                    unsigned int num;
                    DFBRectangle r = { rect->x, rect->y + y, rect->w, 1 };

                    real->Read( real, &r, buf, len );

                    encoded = rle16_encode( buf, tmp, rect->w, &num );

                    ret = voodoo_manager_respond( manager, y == rect->h - 1, msg->header.serial,
                                                  DFB_OK, VOODOO_INSTANCE_NONE,
                                                  VMBT_UINT, encoded ? 2 : 0,
                                                  VMBT_DATA, DFB_BYTES_PER_LINE( format, num ),
                                                             encoded ? tmp : buf,
                                                  VMBT_NONE );
                    if (ret)
                         break;
               }
               break;
          }

          case DSPF_RGB32:
          case DSPF_ARGB: {
               u32 tmp[rect->w];

               for (y=0; y<rect->h; y++) {
                    unsigned int num;
                    DFBRectangle r = { rect->x, rect->y + y, rect->w, 1 };

                    real->Read( real, &r, buf, len );

                    encoded = rle32_encode( buf, tmp, rect->w, &num );

                    ret = voodoo_manager_respond( manager, y == rect->h - 1, msg->header.serial,
                                                  DFB_OK, VOODOO_INSTANCE_NONE,
                                                  VMBT_UINT, encoded ? 4 : 0,
                                                  VMBT_DATA, DFB_BYTES_PER_LINE( format, num ),
                                                             encoded ? tmp : buf,
                                                  VMBT_NONE );
                    if (ret)
                         break;
               }
               break;
          }

          default:
               for (y=0; y<rect->h; y++) {
                    DFBRectangle r = { rect->x, rect->y + y, rect->w, 1 };

                    real->Read( real, &r, buf, len );

                    voodoo_manager_respond( manager, y == rect->h - 1, msg->header.serial,
                                            DFB_OK, VOODOO_INSTANCE_NONE,
                                            VMBT_UINT, 0,
                                            VMBT_DATA, len, buf,
                                            VMBT_NONE );
               }
               break;
     }

     return DFB_OK;
}

static DirectResult
Dispatch_SetRenderOptions( IDirectFBSurface *thiz, IDirectFBSurface *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser     parser;
     DFBSurfaceRenderOptions options;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, options );
     VOODOO_PARSER_END( parser );

     real->SetRenderOptions( real, options );

     return DFB_OK;
}

static DirectResult
Dispatch_SetMatrix( IDirectFBSurface *thiz, IDirectFBSurface *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const s32           *matrix;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, matrix );
     VOODOO_PARSER_END( parser );

     real->SetMatrix( real, matrix );

     return DFB_OK;
}

static DirectResult
Dispatch_SetRemoteInstance( IDirectFBSurface *thiz, IDirectFBSurface *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, data->remote );
     VOODOO_PARSER_END( parser );

     return voodoo_manager_respond( manager, true, msg->header.serial, DR_OK, VOODOO_INSTANCE_NONE, VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBSurface/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBSURFACE_METHOD_ID_Release:
               return Dispatch_Release( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetPosition:
               return Dispatch_GetPosition( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetSize:
               return Dispatch_GetSize( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetVisibleRectangle:
               return Dispatch_GetVisibleRectangle( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetPixelFormat:
               return Dispatch_GetPixelFormat( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetAccelerationMask:
               return Dispatch_GetAccelerationMask( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetPalette:
               return Dispatch_GetPalette( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetPalette:
               return Dispatch_SetPalette( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_Flip:
               return Dispatch_Flip( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_Clear:
               return Dispatch_Clear( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetClip:
               return Dispatch_SetClip( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetColor:
               return Dispatch_SetColor( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetSrcColorKey:
               return Dispatch_SetSrcColorKey( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetSrcColorKeyIndex:
               return Dispatch_SetSrcColorKeyIndex( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetDstColorKey:
               return Dispatch_SetDstColorKey( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetDstColorKeyIndex:
               return Dispatch_SetDstColorKeyIndex( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetBlittingFlags:
               return Dispatch_SetBlittingFlags( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_Blit:
               return Dispatch_Blit( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_TileBlit:
               return Dispatch_TileBlit( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_BatchBlit:
               return Dispatch_BatchBlit( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_StretchBlit:
               return Dispatch_StretchBlit( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_TextureTriangles:
               return Dispatch_TextureTriangles( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetDrawingFlags:
               return Dispatch_SetDrawingFlags( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_FillRectangle:
               return Dispatch_FillRectangle( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DrawLine:
               return Dispatch_DrawLine( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DrawLines:
               return Dispatch_DrawLines( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DrawRectangle:
               return Dispatch_DrawRectangle( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_FillTriangle:
               return Dispatch_FillTriangle( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetFont:
               return Dispatch_SetFont( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DrawString:
               return Dispatch_DrawString( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DrawGlyph:
               return Dispatch_DrawGlyph( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetEncoding:
               return Dispatch_SetEncoding( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_GetSubSurface:
               return Dispatch_GetSubSurface( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_FillRectangles:
               return Dispatch_FillRectangles( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_FillTrapezoids:
               return Dispatch_FillTrapezoids( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_FillSpans:
               return Dispatch_FillSpans( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_DisableAcceleration:
               return Dispatch_DisableAcceleration( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_ReleaseSource:
               return Dispatch_ReleaseSource( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_Write:
               return Dispatch_Write( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_Read:
               return Dispatch_Read( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetRenderOptions:
               return Dispatch_SetRenderOptions( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetMatrix:
               return Dispatch_SetMatrix( dispatcher, real, manager, msg );

          case IDIRECTFBSURFACE_METHOD_ID_SetRemoteInstance:
               return Dispatch_SetRemoteInstance( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBSurface *thiz,
           IDirectFBSurface *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref   = 1;
     data->real  = real;
     data->super = super;
     data->self    = *ret_instance;
     data->manager = manager;

     thiz->AddRef = IDirectFBSurface_Dispatcher_AddRef;
     thiz->Release = IDirectFBSurface_Dispatcher_Release;

     thiz->GetCapabilities = IDirectFBSurface_Dispatcher_GetCapabilities;
     thiz->GetPosition = IDirectFBSurface_Dispatcher_GetPosition;
     thiz->GetSize = IDirectFBSurface_Dispatcher_GetSize;
     thiz->GetVisibleRectangle = IDirectFBSurface_Dispatcher_GetVisibleRectangle;
     thiz->GetPixelFormat = IDirectFBSurface_Dispatcher_GetPixelFormat;
     thiz->GetAccelerationMask = IDirectFBSurface_Dispatcher_GetAccelerationMask;

     thiz->GetPalette = IDirectFBSurface_Dispatcher_GetPalette;
     thiz->SetPalette = IDirectFBSurface_Dispatcher_SetPalette;
     thiz->SetAlphaRamp = IDirectFBSurface_Dispatcher_SetAlphaRamp;

     thiz->Lock = IDirectFBSurface_Dispatcher_Lock;
     thiz->GetFramebufferOffset = IDirectFBSurface_Dispatcher_GetFramebufferOffset;
     thiz->GetPhysicalAddress = IDirectFBSurface_Dispatcher_GetPhysicalAddress;
     thiz->Unlock = IDirectFBSurface_Dispatcher_Unlock;
     thiz->Flip = IDirectFBSurface_Dispatcher_Flip;
     thiz->SetField = IDirectFBSurface_Dispatcher_SetField;
     thiz->Clear = IDirectFBSurface_Dispatcher_Clear;

     thiz->SetClip = IDirectFBSurface_Dispatcher_SetClip;
     thiz->GetClip = IDirectFBSurface_Dispatcher_GetClip;
     thiz->SetColor = IDirectFBSurface_Dispatcher_SetColor;
     thiz->SetColorIndex = IDirectFBSurface_Dispatcher_SetColorIndex;
     thiz->SetSrcBlendFunction = IDirectFBSurface_Dispatcher_SetSrcBlendFunction;
     thiz->SetDstBlendFunction = IDirectFBSurface_Dispatcher_SetDstBlendFunction;
     thiz->SetPorterDuff = IDirectFBSurface_Dispatcher_SetPorterDuff;
     thiz->SetSrcColorKey = IDirectFBSurface_Dispatcher_SetSrcColorKey;
     thiz->SetSrcColorKeyIndex = IDirectFBSurface_Dispatcher_SetSrcColorKeyIndex;
     thiz->SetDstColorKey = IDirectFBSurface_Dispatcher_SetDstColorKey;
     thiz->SetDstColorKeyIndex = IDirectFBSurface_Dispatcher_SetDstColorKeyIndex;

     thiz->SetBlittingFlags = IDirectFBSurface_Dispatcher_SetBlittingFlags;
     thiz->Blit = IDirectFBSurface_Dispatcher_Blit;
     thiz->TileBlit = IDirectFBSurface_Dispatcher_TileBlit;
     thiz->BatchBlit = IDirectFBSurface_Dispatcher_BatchBlit;
     thiz->StretchBlit = IDirectFBSurface_Dispatcher_StretchBlit;
     thiz->TextureTriangles = IDirectFBSurface_Dispatcher_TextureTriangles;

     thiz->SetDrawingFlags = IDirectFBSurface_Dispatcher_SetDrawingFlags;
     thiz->FillRectangle = IDirectFBSurface_Dispatcher_FillRectangle;
     thiz->FillRectangles = IDirectFBSurface_Dispatcher_FillRectangles;
     thiz->FillTrapezoids = IDirectFBSurface_Dispatcher_FillTrapezoids;
     thiz->FillSpans = IDirectFBSurface_Dispatcher_FillSpans;
     thiz->DrawLine = IDirectFBSurface_Dispatcher_DrawLine;
     thiz->DrawLines = IDirectFBSurface_Dispatcher_DrawLines;
     thiz->DrawRectangle = IDirectFBSurface_Dispatcher_DrawRectangle;
     thiz->FillTriangle = IDirectFBSurface_Dispatcher_FillTriangle;

     thiz->SetFont = IDirectFBSurface_Dispatcher_SetFont;
     thiz->GetFont = IDirectFBSurface_Dispatcher_GetFont;
     thiz->DrawString = IDirectFBSurface_Dispatcher_DrawString;
     thiz->DrawGlyph = IDirectFBSurface_Dispatcher_DrawGlyph;
     thiz->SetEncoding = IDirectFBSurface_Dispatcher_SetEncoding;

     thiz->GetSubSurface = IDirectFBSurface_Dispatcher_GetSubSurface;

     thiz->GetGL = IDirectFBSurface_Dispatcher_GetGL;

     thiz->Dump = IDirectFBSurface_Dispatcher_Dump;

     thiz->DisableAcceleration = IDirectFBSurface_Dispatcher_DisableAcceleration;

     thiz->ReleaseSource = IDirectFBSurface_Dispatcher_ReleaseSource;

     thiz->SetIndexTranslation = IDirectFBSurface_Dispatcher_SetIndexTranslation;

     return DFB_OK;
}

