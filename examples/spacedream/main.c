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

#include <directfb.h>

#include <pthread.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                    \
     {                                                                    \
          DFBResult err = x;                                              \
          if (err != DFB_OK) {                                            \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );     \
               DirectFBErrorFatal( #x, err );                             \
          }                                                               \
     }

#define SGN(x)        (((x) > 0) ? 1 : -1)

#define NUM_STARS     4

IDirectFB            *dfb;
IDirectFBFont        *font;
IDirectFBSurface     *stars[NUM_STARS];

pthread_mutex_t       render_start  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t       render_finish = PTHREAD_MUTEX_INITIALIZER;

int xres;
int yres;

typedef struct {
     float x;
     float y;
     float z;
} Star;


#define STARFIELD_SIZE 500

static Star starfield[STARFIELD_SIZE];
static Star t_starfield[STARFIELD_SIZE];

/* for main thread */
static inline void start_rendering()
{
     pthread_mutex_unlock( &render_start );
}

static inline void finish_rendering()
{
     pthread_mutex_lock( &render_finish );
}

/* for render thread */
static inline int started_rendering()
{
     return !pthread_mutex_lock( &render_start );
}

static inline void finished_rendering()
{
     pthread_mutex_unlock( &render_finish );
}

static void* render_loop (void *arg)
{
     IDirectFBSurface *view = (IDirectFBSurface*)arg;

     view->SetBlittingFlags( view, DSBLIT_SRC_COLORKEY /*| DSBLIT_COLORIZE*/ );
     view->SetSrcColorKey( view, 0xF81F ); /* FIXME: format!!! */

     while (started_rendering()) {
          int i;

          pthread_testcancel();

          view->SetColor( view, 0, 0, 0, 0 );
          view->FillRectangle( view, 0, 0, xres, yres );

          for (i=0; i<STARFIELD_SIZE; i++) {
               int map = (int)t_starfield[i].z >> 6;
          //     int light = 0xFF - ((t_starfield[i].z & 0xF) << 3);

               if (map >= 0) {
                    if (map >= NUM_STARS)
                         map = NUM_STARS - 1;

               //     view->SetColor( view, light, light, light, 0xff );
                    view->Blit( view, stars[map],
                                NULL, (int)t_starfield[i].x, (int)t_starfield[i].y );
               }
          }

          view->Flip( view, NULL, DSFLIP_WAITFORSYNC );

          finished_rendering();
     }

     pthread_testcancel();

     return NULL;
}

void load_stars()
{
     IDirectFBImageProvider *provider;
     DFBSurfaceDescription   dsc;

     int  i;
     char name[ strlen(DATADIR"/star.png") + 4 ];

     for (i=0; i<NUM_STARS; i++) {

          sprintf( name, DATADIR"/star%d.png", i+1 );

          DFBCHECK( dfb->CreateImageProvider( dfb, name, &provider ) );
          DFBCHECK( provider->GetSurfaceDescription (provider, &dsc) );
          DFBCHECK( dfb->CreateSurface( dfb, &dsc, &stars[i] ) );
          DFBCHECK( provider->RenderTo( provider, stars[i] ) );

          provider->Release( provider );
     }
}

void generate_starfield()
{
     int i;

     for (i=0; i<STARFIELD_SIZE; i++) {
          starfield[i].x = rand()%xres - xres/2;
          starfield[i].y = rand()%yres - yres/2;
          starfield[i].z = (rand()%(NUM_STARS<<7)) - (NUM_STARS<<6);

          starfield[i].x *= 2;
          starfield[i].y *= 2;
          starfield[i].z *= 2;
     }
}

void move_starfield()
{
     int i;
     int dx, dy, dz;

     dx = rand()%3-1;
     dy = rand()%3-1;
     dz = rand()%3-1;

     for (i=0; i<STARFIELD_SIZE; i++) {
          starfield[i].x += dx;
          starfield[i].y += dy;
          starfield[i].z += dz;
     }
}

void transform_starfield()
{
     static float rot = 0;
     int i;

     for (i=0; i<STARFIELD_SIZE; i++) {
          t_starfield[i].x = starfield[i].x * cos(rot) + starfield[i].z * sin(rot);
          t_starfield[i].y = starfield[i].y;
          t_starfield[i].z = starfield[i].x * -sin(rot) + starfield[i].z * cos(rot);

          if (t_starfield[i].z >= 0) {
               t_starfield[i].x /= (t_starfield[i].z / 300.0f) + 1.0f;
               t_starfield[i].y /= (t_starfield[i].z / 400.0f) + 1.0f;
          }

          t_starfield[i].x += xres/2;
          t_starfield[i].y += yres/2;
     }

     rot += 0.01f;
}

void unload_stars()
{
     int i;

     for (i=0; i<NUM_STARS; i++)
          stars[i]->Release( stars[i] );
}

int main( int argc, char *argv[] )
{
     int                   quit = 0;
     pthread_t             render_loop_thread = -1;

     IDirectFBSurface     *primary;
     IDirectFBInputDevice *keyboard;
     IDirectFBInputBuffer *keybuffer;

     DFBCardCapabilities   caps;
     DFBSurfaceDescription dsc;


     srand((long)time(0));

     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* get some information about the card */
     DFBCHECK(dfb->GetCardCapabilities( dfb, &caps ));

     /* get an interface to the primary keyboard and create an
        input buffer for it */
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ));
     DFBCHECK(keyboard->CreateInputBuffer( keyboard, &keybuffer ));

     /* set our cooperative level to DFSCL_FULLSCREEN for exclusive access to
        the primary layer */
     DFBCHECK(dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN ));

     /* get the primary surface, i.e. the surface of the primary layer we have
        exclusive access to */
     dsc.flags = DSDESC_CAPS;
     dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));

     /* set our desired video mode */
     DFBCHECK(primary->GetSize( primary, &xres, &yres ));

     /* load font */
     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = yres/10;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(primary->SetFont( primary, font ));
     }

     load_stars();

     generate_starfield();

     pthread_mutex_lock( &render_start );
     pthread_mutex_lock( &render_finish );
     pthread_create( &render_loop_thread, NULL, render_loop, (void*)primary );

     /* main loop */
     while (!quit) {
          DFBInputEvent evt;

          /* transform world to screen coordinates */
          transform_starfield();

          /* start rendering before waiting for events */
          start_rendering();


          //keybuffer->WaitForEvent( keybuffer );

          /* process keybuffer */
          while (keybuffer->GetEvent( keybuffer, &evt) == DFB_OK) {
               if (evt.type == DIET_KEYPRESS) {
                    switch (evt.keycode) {
                         case DIKC_ESCAPE:
                              /* quit main loop */
                              quit = 1;
                              break;

                         default:
                              move_starfield();
                              break;
                    }
               }
          }

//          move_starfield();

          /* finish rendering before retransforming the world */
          finish_rendering();
     }

     pthread_cancel( render_loop_thread );
     pthread_mutex_unlock( &render_start );
     pthread_join( render_loop_thread, NULL );
     render_loop_thread = -1;


     unload_stars();

     keybuffer->Release( keybuffer );
     keyboard->Release( keyboard );
     primary->Release( primary );
     dfb->Release( dfb );

     return 0;
}
