/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef __GFXCARD_H__
#define __GFXCARD_H__

#include <linux/fb.h>


typedef enum {
     CCF_CLIPPING
} CardCapabilitiesFlags;


typedef struct _GfxDriver GfxDriver;

/*
 * return value for hardware accelerated card functions
 */
typedef enum {
     CR_OK = 0,
     CR_FALLBACK
} CardResult;

/*
 * stuct for graphics cards
 */
struct _GfxCard {
     /* fbdev fixed screeninfo, contains infos about memory and type of card */
     struct fb_fix_screeninfo fix;

     /* DirectFB driver info */
     struct {
          char driver_name[60];
          char driver_vendor[40];
          struct {
               int major;
               int minor;
          } driver_version;

          GfxDriver *driver;
     } info;

     /* hardware acceleration capabilities */
     struct {
          CardCapabilitiesFlags   flags;

          DFBAccelerationMask     accel;
          DFBSurfaceBlittingFlags blitting;
          DFBSurfaceDrawingFlags  drawing;
     } caps;

     /* framebuffer address and size */
     struct {
          int length;
          void *base;
     } framebuffer;

     pthread_mutex_t     lock;

     /* surface manager stores the offset of the surface heap here */
     unsigned int        heap_offset;

     /* card limitations for surface offsets and their pitch */
     unsigned int        byteoffset_align;
     unsigned int        pixelpitch_align;

     /*
      * function that is called after variable screeninfo is changed
      * (used for buggy fbdev drivers, that reinitialize something when
      * calling FBIO_PUT_VSCREENINFO)
      */
     void (*AfterSetVar)();

     /*
      * makes sure that graphics hardware has finished all operations
      */
     void (*EngineSync)();

     /*
      * after the video memory has been written to by the CPU (e.g. modification
      * of a texture) make sure the accelerator won't use cached texture data
      */
     void (*FlushTextureCache)();

     /*
      * state handling
      */
     CardState           *state;
     int                  source_locked;

     void (*CheckState)( CardState *state, DFBAccelerationMask accel );
     void (*SetState  )( CardState *state, DFBAccelerationMask accel );

     /*
      * drawing functions
      */
     void (*FillRectangle) ( DFBRectangle *rect );

     void (*DrawRectangle) ( DFBRectangle *rect );

     void (*DrawLine) ( DFBRegion *line );

     void (*FillTriangle) ( DFBTriangle *tri );

     void (*Blit)( DFBRectangle *rect, int dx, int dy );

     void (*StretchBlit) ( DFBRectangle *srect, DFBRectangle *drect );

};

extern GfxCard *card;


struct _GfxDriver
{
     int      (*Probe)( int fd, GfxCard *card ); /* if it returns 1,
                                                    the driver is suitable */
     int      (*Init)( int fd, GfxCard *card );  /* initialize gfx driver */
     void     (*InitLayers)();                   /* initialize additional
                                                    layers, if supported */
     void     (*DeInit)();                       /* deinitialize */
};

/*
 * loads/probes/unloads one driver module after another until a suitable
 * driver is found and returns its symlinked functions
 */
GfxDriver* gfxcards_find_card( int fd );


/*
 * initializes card struct, maps framebuffer, chooses accelerated driver
 */
DFBResult gfxcard_init();

/*
 * initializes card struct, maps framebuffer, chooses accelerated driver
 */
DFBResult gfxcard_init_layers();

int gfxcard_state_check( CardState *state, DFBAccelerationMask accel );
int gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel );
void gfxcard_state_release( CardState *state );

/*
 * drawing functions, lock source and destination surfaces,
 * handle clipping and drawing method (hardware/software)
 */
void gfxcard_fillrectangle( DFBRectangle *rect, CardState *state );

void gfxcard_drawrectangle( DFBRectangle *rect, CardState *state );

void gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state );

void gfxcard_filltriangle( DFBTriangle *tri, CardState *state );

void gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state );

void gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                          CardState *state );

void gfxcard_drawstring( const __u8 *text, int bytes, int x, int y,
                         CoreFont *font, CardState *state );


static inline void gfxcard_sync()
{
     if (card->EngineSync)
          card->EngineSync();
}

static inline void gfxcard_flush_texture_cache()
{
     if (card->FlushTextureCache)
          card->FlushTextureCache();
}

#endif

