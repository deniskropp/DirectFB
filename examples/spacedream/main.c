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

#include "math3d.h"

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
     Vector pos;
} Star;

Matrix *projection;
Matrix *camera;

#define STARFIELD_SIZE 5000

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

     view->SetBlittingFlags( view, DSBLIT_SRC_COLORKEY | DSBLIT_COLORIZE );

     while (started_rendering()) {
          int i;

          pthread_testcancel();

          view->SetColor( view, 0, 0, 0, 0 );
          view->FillRectangle( view, 0, 0, xres, yres );

          for (i=0; i<STARFIELD_SIZE; i++) {
               int map = (int)(t_starfield[i].pos.v[Z]) >> 8;
               int light = 0xFF - ((int)(t_starfield[i].pos.v[Z] * t_starfield[i].pos.v[Z]) >> 12);

               if (map >= 0 && light > 0) {
                    if (map >= NUM_STARS)
                         map = NUM_STARS - 1;

                    view->SetColor( view, light, light, light, 0xff );
                    view->Blit( view, stars[map], NULL,
                                (int)(t_starfield[i].pos.v[X]),
                                (int)(t_starfield[i].pos.v[Y]) );
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
          
          stars[i]->SetSrcColorKey( stars[i], 0xFF, 0x00, 0xFF );
     }
}

void generate_starfield()
{
     int i;

     for (i=0; i<STARFIELD_SIZE; i++) {
          starfield[i].pos.v[X] = rand()%3001 - 1500;
          starfield[i].pos.v[Y] = rand()%3001 - 1500;
          starfield[i].pos.v[Z] = rand()%3001 - 1500;
          starfield[i].pos.v[W] = 1;
     }
}

void transform_starfield()
{
     int    i;
     Matrix m;

     memcpy( &m, camera, sizeof(Matrix) );
     matrix_multiply( &m, projection );

     for (i=0; i<STARFIELD_SIZE; i++) {
          matrix_transform( &m, &starfield[i].pos, &t_starfield[i].pos );

          if (t_starfield[i].pos.v[W]) {
               t_starfield[i].pos.v[X] /= t_starfield[i].pos.v[W];
               t_starfield[i].pos.v[Y] /= t_starfield[i].pos.v[W];
          }

          t_starfield[i].pos.v[X] += xres/2;
          t_starfield[i].pos.v[Y] += yres/2;
     }
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
     IDirectFBInputDevice *mouse;
     IDirectFBInputBuffer *mousebuffer;

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

     /* get an interface to the primary mouse and create an
        input buffer for it */
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_MOUSE, &mouse ));
     DFBCHECK(mouse->CreateInputBuffer( mouse, &mousebuffer ));

     /* set our cooperative level to DFSCL_FULLSCREEN for exclusive access to
        the primary layer */
     DFBCHECK(dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN ));

     /* get the primary surface, i.e. the surface of the primary layer we have
        exclusive access to */
     dsc.flags = DSDESC_CAPS;
     dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));

     DFBCHECK(primary->GetSize( primary, &xres, &yres ));

     /* load font */
     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = yres/10;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(primary->SetFont( primary, font ));
     }

     projection = matrix_new_perspective( 400 );
     camera = matrix_new_identity();

     load_stars();

     generate_starfield();

     pthread_mutex_lock( &render_start );
     pthread_mutex_lock( &render_finish );
     pthread_create( &render_loop_thread, NULL, render_loop, (void*)primary );

     /* main loop */
     while (!quit) {
          static float  translation[3] = { 0, 0, 0 };
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

                         case DIKC_LEFT:
                              translation[0] =  10;
                              break;

                         case DIKC_RIGHT:
                              translation[0] = -10;
                              break;

                         case DIKC_UP:
                              translation[2] = -10;
                              break;

                         case DIKC_DOWN:
                              translation[2] =  10;
                              break;

                         default:
                              break;
                    }
               } else
               if (evt.type == DIET_KEYRELEASE) {
                    switch (evt.keycode) {
                         case DIKC_LEFT:
                         case DIKC_RIGHT:
                              translation[0] = 0;
                              break;

                         case DIKC_UP:
                         case DIKC_DOWN:
                              translation[2] = 0;
                              break;

                         default:
                              break;
                    }
               }
          }

          /* process mousebuffer */
          while (mousebuffer->GetEvent( mousebuffer, &evt) == DFB_OK) {
               if (evt.type == DIET_AXISMOTION  && (evt.flags & DIEF_AXISREL)) {
                    switch (evt.axis) {
                         case DIAI_X:
                              matrix_rotate( camera, Y, -evt.axisrel/80.0f );
                              break;

                         case DIAI_Y:
                              matrix_rotate( camera, X,  evt.axisrel/80.0f );
                              break;

                         default:
                              break;
                    }
               }
          }

          matrix_translate( camera,
                            translation[0], translation[1], translation[2] );

          /* finish rendering before retransforming the world */
          finish_rendering();
     }

     pthread_cancel( render_loop_thread );
     pthread_mutex_unlock( &render_start );
     pthread_join( render_loop_thread, NULL );
     render_loop_thread = -1;


     unload_stars();

     free( camera );
     free( projection );

     keybuffer->Release( keybuffer );
     keyboard->Release( keyboard );
     primary->Release( primary );
     dfb->Release( dfb );

     return 0;
}
