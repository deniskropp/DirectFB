/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <string.h>

#include <config.h>
#include <directfb.h>

#include <malloc.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "gfxcard.h"
#include "core.h"
#include "coredefs.h"
#include "fbdev.h"
#include "surfacemanager.h"

#include <gfx/generic/generic.h>
#include <misc/gfx_util.h>


GfxCard *card = NULL;

#ifdef USE_MMX
unsigned int intel_cpu_features();
#endif


GfxDriver* gfxcard_find_driver( int fd )
{
     GfxDriver *d;
     DIR     *dir;
     struct dirent *entry;
     char *driver_dir = LIBDIR"/gfxdrivers";

     if (config->software_only)
          return NULL;

     dir = opendir( driver_dir );

     if (!dir) {
          PERRORMSG( "DirectFB/core/gfxcards: "
                     "Could not open driver directory `%s'!\n", driver_dir );
          return NULL;
     }

     d = malloc( sizeof(GfxDriver) );
     memset( d, 0, sizeof(GfxDriver) );

     while ( (entry = readdir(dir) ) != NULL ) {
          void *handle;
          char buf[4096];

          if (entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          sprintf( buf, "%s/%s", driver_dir, entry->d_name );

          handle = dlopen( buf, RTLD_LAZY );
          if (handle) {
               d->Probe  = dlsym( handle, "driver_probe"  );
               if (d->Probe) {
                    if ( d->Probe( fd, card )) {
                         d->Init       = dlsym( handle, "driver_init"   );
                         d->InitLayers = dlsym( handle, "driver_init_layers" );
                         d->DeInit     = dlsym( handle, "driver_deinit" );

                         if (d->Init  &&  d->DeInit) {
                              closedir( dir );
                              return d;
                         }
                         else
                              DLERRORMSG( "DirectFB/core/gfxcards: "
                                          "Probe succeeded but Init/DeInit "
                                          "symbols not found in `%s'!\n", buf );
                    }
               }
               else
                    DLERRORMSG( "DirectFB/core/gfxcards: "
                                "Could not link probe function of `%s'!\n",
                                buf );

               dlclose( handle );
          }
          else
               DLERRORMSG( "DirectFB/core/gfxcards: Unable to dlopen `%s'!\n",
                           buf );

     }

     closedir( dir );

     free( d );

     return NULL;
}

static void gfxcard_deinit()
{
     if (!card) {
          BUG( "gfxcard_deinit without gfxcard_init!?" );
          return;
     }

     gfxcard_sync();

     if (card->info.driver && card->info.driver->DeInit)
          card->info.driver->DeInit();

     munmap( (char*)card->framebuffer.base, card->framebuffer.length );

     if (card->info.driver)
          free( card->info.driver );
     
     free( card );
     card = NULL;
}

DFBResult gfxcard_init()
{
     if (card) {
          BUG( "gfxcard_init() called twice" );
          return DFB_BUG;
     }

     card = (GfxCard*)malloc( sizeof(GfxCard) );
     memset( card, 0, sizeof(GfxCard) );

     sprintf( card->info.driver_name, "Generic" );

#ifdef USE_MMX
     if  (intel_cpu_features() & (1 << 23)) {
          if (config->no_mmx) {
               INITMSG( "MMX detected, but disabled by --no-mmx \n");
          }
          else {
               sprintf( card->info.driver_name, "Generic (MMX)" );
               gInit_MMX();
               INITMSG( "MMX detected and enabled\n");
          }
     }
     else {
          INITMSG( "No MMX detected\n" );
     }
#endif

     sprintf( card->info.driver_vendor, "convergence integrated media GmbH" );

     card->info.driver_version.major = 0;
     card->info.driver_version.minor = 3;

     if (ioctl( display->fd, FBIOGET_FSCREENINFO, &card->fix ) < 0) {
          PERRORMSG( "DirectFB/core/gfxcard: "
                     "Could not get fixed screen information!\n" );
          free( card );
          card = NULL;
          return DFB_INIT;
     }

     card->framebuffer.length = card->fix.smem_len;
     card->framebuffer.base = mmap( NULL, card->fix.smem_len,
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    display->fd, 0 );
     if ((int)(card->framebuffer.base) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: "
                     "Could not mmap the framebuffer!\n");
          free( card );
          card = NULL;
          return DFB_INIT;
     }

     memset( card->framebuffer.base, 0, card->fix.smem_len );

     {
          GfxDriver *driver = gfxcard_find_driver( display->fd );

          if (driver) {
               int ret = driver->Init( display->fd, card );

               if (ret) {
                    munmap( (char*)card->framebuffer.base,
                            card->framebuffer.length );
                    free( driver );
                    free( card );
                    card = NULL;
                    return ret;
               }

               card->info.driver = driver;
          }
     }

     INITMSG( "DirectFB/GfxCard: %s %d.%d (%s)\n", card->info.driver_name,
              card->info.driver_version.major, card->info.driver_version.minor,
              card->info.driver_vendor );

     /* so now that we have initialized the graphics driver and examined
        the framebuffer size fire up the surface manager! */
     surfacemanager_init_heap();

     pthread_mutex_init( &card->lock, NULL );

     core_cleanup_push( gfxcard_deinit );

     return DFB_OK;
}

DFBResult gfxcard_init_layers()
{
     if (card->info.driver && card->info.driver->InitLayers)
          card->info.driver->InitLayers();
     
     return DFB_OK;
}

int gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     if (!card->CheckState)
          return 0;

     if (state->modified & SMF_DESTINATION) {
          state->checked = 0;
          if (!state->destination) {
               BUG("state check: no destination");
               return 0;
          }

          if (state->destination->back_buffer->policy == CSP_SYSTEMONLY) {
               state->modified &= ~SMF_DESTINATION;
               state->accel = 0;
               return 0;
          }
     }
     if (state->modified & SMF_SOURCE) {
          state->checked &= 0xFFFF;
          if (!state->source  &&  accel & 0xFFFF0000) {
               BUG("state check: no source");
               return 0;
          }

          if (state->source &&
              state->source->front_buffer->policy == CSP_SYSTEMONLY) {
               state->modified &= ~SMF_SOURCE;
               state->accel &= 0xFFFF;
               if (accel & 0xFFFF0000)
                    return 0;
          }
     }

     if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND)) {
          state->checked = 0;
     }
     else {
          if (state->modified & SMF_DRAWING_FLAGS)
               state->checked &= 0xFFFF0000;

          if (state->modified & SMF_BLITTING_FLAGS)
               state->checked &= 0xFFFF;
     }

     if (!(state->checked & accel)) {
          state->accel &= ~accel;
          card->CheckState( state, accel );
          state->checked |= accel;
     }

     return (state->accel & accel);
}

int gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel )
{
     int source_locked;
     DFBSurfaceLockFlags lock_flags;

     if (!state->destination) {
          BUG("state check: no destination");
          return 0;
     }
     if (!state->source  &&  accel & 0xFFFF0000) {
          BUG("state check: no source");
          return 0;
     }
     
     if (accel & 0xFFFF0000)
          lock_flags = state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                DSBLIT_BLEND_COLORALPHA   |
                                                DSBLIT_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;
     else
          lock_flags = state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;

     if (surface_hard_lock( state->destination, lock_flags, 0 ))
          return 0;

     if (accel & 0xFFFF0000) {
          if (surface_hard_lock( state->source, DSLF_READ, 1 )) {
               surface_unlock( state->destination, 0 );
               return 0;
          }
          
          source_locked = 1;
     }
     else
          source_locked = 0;

     pthread_mutex_lock( &card->lock );

     card->source_locked = source_locked;

     if (!(state->set & accel)  ||
         state != card->state   ||
         state->modified)
     {
          card->SetState( state, accel );
     }

     return 1;
}

void gfxcard_state_release( CardState *state )
{
     surface_unlock( state->destination, 0 );

     if (card->source_locked)
          surface_unlock( state->source, 1 );

     pthread_mutex_unlock( &card->lock );
}

/** DRAWING FUNCTIONS **/

void gfxcard_fillrectangle( DFBRectangle *rect, CardState *state )
{
     if (gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
         gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
     {
          if (card->caps.flags & CCF_CLIPPING  ||
          clip_rectangle( &state->clip, rect ))
          {
              card->FillRectangle( rect );
          }
          gfxcard_state_release( state );
     }
     else
     {
          if (clip_rectangle( &state->clip, rect ) &&
          gAquire( state, DFXL_FILLRECTANGLE ))
      {
               gFillRectangle( rect );
               gRelease( state );
          }
     }
}

void gfxcard_drawrectangle( DFBRectangle *rect, CardState *state )
{
     if (gfxcard_state_check( state, DFXL_DRAWRECTANGLE ) &&
         gfxcard_state_acquire( state, DFXL_DRAWRECTANGLE ))
     {
          if (card->caps.flags & CCF_CLIPPING  ||
          clip_rectangle( &state->clip, rect ))
          {
              card->DrawRectangle( rect );
          }
          gfxcard_state_release( state );
     }
     else
     {
          unsigned int edges = clip_rectangle (&state->clip, rect);

      if (edges)
      {
          if (gAquire( state, DFXL_DRAWLINE))
          {
          DFBRegion line;

          if (edges & 1)
          {
              line.x1 = line.x2 = rect->x;
                      line.y1 = rect->y + (edges & 2 ? 1 : 0);
              line.y2 = rect->y + rect->h - 1;
              gDrawLine( &line );
          }
          if (edges & 2)
          {
              line.x1 = rect->x;
              line.x2 = rect->x + rect->w - (edges & 4 ? 2 : 1);
              line.y1 = line.y2 = rect->y;
              gDrawLine( &line );
          }
          if (edges & 4)
          {
              line.x1 = line.x2 = rect->x + rect->w - 1;
                      line.y1 = rect->y;
              line.y2 = rect->y + rect->h - (edges & 8 ? 2 : 1);
              gDrawLine( &line );
          }
          if (edges & 8)
          {
              line.x1 = rect->x + (edges & 1 ? 1 : 0);
              line.x2 = rect->x + rect->w - 1;
              line.y1 = line.y2 = rect->y + rect->h - 1;
              gDrawLine( &line );
          }

          gRelease (state);
          }
      }
     }
}

void gfxcard_drawline( DFBRegion *line, CardState *state )
{
     if (gfxcard_state_check( state, DFXL_DRAWLINE ) &&
         gfxcard_state_acquire( state, DFXL_DRAWLINE ))
     {
          if (card->caps.flags & CCF_CLIPPING  ||
               clip_line( &state->clip, line ))
          {
               card->DrawLine( line );
          }
          gfxcard_state_release( state );
     }
     else
     {
          if (clip_line (&state->clip, line) &&
               gAquire( state, DFXL_DRAWLINE ))
          {
               gDrawLine( line );
               gRelease( state );
          }
     }
}

void gfxcard_filltriangle( DFBTriangle *tri, CardState *state )
{
     if (gfxcard_state_check( state, DFXL_FILLTRIANGLE ) &&
         gfxcard_state_acquire( state, DFXL_FILLTRIANGLE ))
     {
          /*  FIXME: do real clipping  */
          if (card->caps.flags & CCF_CLIPPING ||
              clip_triangle_precheck( &state->clip, tri ))
               card->FillTriangle( tri );

          gfxcard_state_release( state );
     }
     else
     {
          /*  FIXME: do real clipping  */
          if (clip_triangle_precheck( &state->clip, tri ) &&
              gAquire( state, DFXL_FILLTRIANGLE ))
          {
               gFillTriangle( tri );
               gRelease( state );
          }
     }
}

void gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state )
{
     if (!clip_blit_precheck( &state->clip, rect->w, rect->h, dx, dy ))
          /* no work at all */
          return;

     if (gfxcard_state_check( state, DFXL_BLIT ) &&
         gfxcard_state_acquire( state, DFXL_BLIT ))
     {
          if (!(card->caps.flags & CCF_CLIPPING))
               clip_blit( &state->clip, rect, &dx, &dy );

          card->Blit( rect, dx, dy );
          gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_BLIT )) {
               clip_blit( &state->clip, rect, &dx, &dy );
               gBlit( rect, dx, dy );
               gRelease( state );
          }
     }
}

void gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                          CardState *state )
{
     if (!clip_blit_precheck( &state->clip, drect->w, drect->h,
                               drect->x, drect->y ))
          return;

     if (gfxcard_state_check( state, DFXL_STRETCHBLIT ) &&
         gfxcard_state_acquire( state, DFXL_STRETCHBLIT ))
     {
          if (!(card->caps.flags & CCF_CLIPPING))
               clip_stretchblit( &state->clip, srect, drect );

          card->StretchBlit( srect, drect );
          gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_STRETCHBLIT )) {
               clip_stretchblit( &state->clip, srect, drect );
               gStretchBlit( srect, drect );
               gRelease( state );
          }
     }
}

#define FONT_BLITTINGFLAGS   (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE)

void gfxcard_drawstring( const __u8 *text, int bytes, 
                         int x, int y,
                         CoreFontData *font, CardState *state )
{
     CoreGlyphData *data;
     CoreSurface   *surface;
     DFBRectangle   rect;
     
     unichar prev = 0;
     unichar current;
     
     int hw_clipping = (card->caps.flags & CCF_CLIPPING);
     int kerning;
     int offset;
     int blit = 0;

     int restore_blittingflags = 0;
     DFBSurfaceBlittingFlags original_blittingflags = 0;

     /* simple prechecks */
     if (y + font->height <= state->clip.y1)
          return;
     if (y > state->clip.y2)
          return;

     state_set_source( state, NULL );
     
     if (state->blittingflags != FONT_BLITTINGFLAGS) {
          restore_blittingflags = 1;
          original_blittingflags = state->blittingflags;
          state->blittingflags = FONT_BLITTINGFLAGS;
          state->modified |= SMF_BLITTING_FLAGS;
     }
     
     for (offset = 0; offset < bytes; offset += utf8_skip[text[offset]]) {
          current = utf8_get_char (&text[offset]);

          if (fonts_get_glyph_data (font, current, &data) == DFB_OK) {
               if (prev && font->GetKerning && 
                   (* font->GetKerning) (font, prev, current, &kerning) == DFB_OK) {
                    x += kerning;
               }

               rect.x = data->start % font->row_width;
               rect.y = 0;
               rect.w = data->width;
               rect.h = data->height;
         
               if (rect.w > 0) {
                    int xx = x + data->left;
                    int yy = y + data->top;
           
                    surface = font->surfaces[data->start / font->row_width];
           
                    if (state->source != surface) {
                         switch (blit) {
                         case 1:
                              gfxcard_state_release( state );
                              break;
                         case 2:
                              gRelease( state );
                              break;
                         default:
                              break;
                         }
                         state->source = surface;
                         state->modified |= SMF_SOURCE;

                         if (gfxcard_state_check( state, DFXL_BLIT ) &&
                             gfxcard_state_acquire( state, DFXL_BLIT ))
                              blit = 1;
                         else if (gAquire( state, DFXL_BLIT ))
                              blit = 2;
                         else
                              blit = 0;
                    }

                    if (clip_blit_precheck( &state->clip,
                                            rect.w, rect.h, xx, yy )) {
                         switch (blit) {
                         case 1:
                              if (!hw_clipping)
                                   clip_blit( &state->clip, &rect, &xx, &yy );
                              card->Blit( &rect, xx, yy );
                              break;
                         case 2:
                              clip_blit( &state->clip, &rect, &xx, &yy );
                              gBlit( &rect, xx, yy );
                              break;
                         default:
                              break;
                         }
                    } 
               }
               x += data->advance;
               prev = current;
          }
     }

     switch (blit) {
     case 1:
          gfxcard_state_release( state );
          break;
     case 2:
          gRelease( state );
          break;
     default:
          break;
     }

     state->source = NULL;
     state->modified |= SMF_SOURCE;

     if (restore_blittingflags) {
          state->blittingflags = original_blittingflags;
          state->modified |= SMF_BLITTING_FLAGS;
     }
}

