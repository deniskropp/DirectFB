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

#define SELFRUNNING

#include <directfb.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define PI 3.1415926536f

IDirectFB *dfb;
IDirectFBSurface *primary;

IDirectFBInputDevice *keyboard;
IDirectFBInputBuffer *keybuffer;

typedef struct _Particle
{
     float w;
     int sw, sh;
     int size;
     int launch;
     struct _Particle *next;
} Particle;

Particle *particles = NULL;
Particle *last_particle = NULL;

static float f = 0;

int sx, sy;

void spawn_particle()
{
     Particle *new_particle = (Particle*)malloc( sizeof(Particle) );

     new_particle->w = 0.05f;
     new_particle->sw = rand()%(int)(sx/3.2f) + (int)(sx/3.2f)*sin(f)
                                              + (int)(sx/3.2f);// + 40*sin(f*5);
     new_particle->sh = rand()%100 + sy-130;// + 40*cos(f*5);
     new_particle->size = rand()%(sx/160) +2;
     new_particle->launch = rand()%(sx/70);
     new_particle->next = NULL;

     if (!particles) {
          particles = new_particle;
          last_particle = new_particle;
     }
     else {
          last_particle->next = new_particle;
          last_particle = new_particle;
     }
}

void draw_particles()
{
     Particle *p = particles;

     while (p) {
          primary->SetColor( primary, 0xA0+rand()%0x50, 0xA0+rand()%0x50, 0xFF, 0x25 );
          primary->FillRectangle( primary, p->launch + sin(p->w/2)*(p->sw),
                                  sy - sin(p->w)*p->sh, p->w*p->size+1,
                                  p->w*p->size+1 );

          p->w += PI/500 * sqrt(p->w) * sx/640.0f;

          if (p->w > PI) {
               particles = p->next;
               free(p);
               p = particles;
               if (!p)
                    last_particle = NULL;
          }
          else {
               p = p->next;
          }
     }
}

void destroy_particles()
{
     Particle *p = particles;

     while (p) {
          particles = p->next;
          free(p);
          p = particles;
     }
}

int main( int argc, char *argv[] )
{
     int i;
     int quit = 0;
     int spawn = 0;
     int right = 0;
     int left = 0;
     DFBResult err;
     DFBCardCapabilities caps;

     srand( (long)time( 0 ) );

     if (DirectFBCreate( &argc, &argv, &dfb )  !=  DFB_OK)
          return 1;

     dfb->GetCardCapabilities( dfb, &caps );

     err = dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
     if (err != DFB_OK) {
          DirectFBError( "Failed requesting exclusive access", err );
          dfb->Release( dfb );
          return 1;
     }

     err = dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard );
     if (err != DFB_OK) {
          DirectFBError( "CreateInputDevice for keyboard failed", err );
          dfb->Release( dfb );
          return 1;
     }

     err = keyboard->CreateInputBuffer( keyboard, &keybuffer );
     if (err != DFB_OK) {
          DirectFBError( "CreateInputBuffer for keyboard failed", err );
          keyboard->Release( keyboard );
          dfb->Release( dfb );
          return 1;
     }

#if 0
     err = dfb->SetVideoMode( dfb, sx, sy, 16 );
     if (err != DFB_OK) {
          DirectFBError( "Failed setting video mode", err );
          keybuffer->Release( keybuffer );
          keyboard->Release( keyboard );
          dfb->Release( dfb );
          return 1;
     }
#endif

     {
          DFBSurfaceDescription dsc;

          memset( &dsc, 0, sizeof(DFBSurfaceDescription) );

          dsc.flags = DSDESC_CAPS;
          dsc.caps = (caps.drawing_flags & DSDRAW_BLEND) ?
                         DSCAPS_PRIMARY | DSCAPS_FLIPPING :
                         DSCAPS_PRIMARY | DSCAPS_FLIPPING | DSCAPS_SYSTEMONLY;

          err = dfb->CreateSurface( dfb, &dsc, &primary );
          if (err != DFB_OK) {
               DirectFBError( "Failed creating primary surface", err );
               keybuffer->Release( keybuffer );
               keyboard->Release( keyboard );
               dfb->Release( dfb );
               return 1;
          }

          primary->GetSize( primary, &sx, &sy );
     }

     primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF );
     primary->FillRectangle( primary, 0, 0, sx, sy );
     err = primary->Flip( primary, NULL, 0 );
     if (err != DFB_OK) {
          DirectFBError( "Failed flipping the primary surface", err );
          primary->Release( primary );
          keybuffer->Release( keybuffer );
          keyboard->Release( keyboard );
          dfb->Release( dfb );
          return 1;
     }

     sleep(2);

     for (i=254; i>=0; i-=4) {
          primary->SetColor( primary, i, i, i, 0xFF );
          primary->FillRectangle( primary, 0, 0, sx, sy );

          err = primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC );
          if (err != DFB_OK) {
               DirectFBError( "Failed flipping the primary surface", err );
               primary->Release( primary );
               keybuffer->Release( keybuffer );
               keyboard->Release( keyboard );
               dfb->Release( dfb );
               return 1;
          }
     }

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );

     while (!quit) {
          DFBInputEvent evt;

          primary->SetColor( primary, 0, 0, 0, 0x17 );
          primary->FillRectangle( primary, 0, 0, sx, sy );

          {
#ifdef SELFRUNNING
               if (!(rand()%50))
                    left = !left;

#else
               keyboard->GetKeyState( keyboard, DIKC_LEFT, &left );
#endif
               if (left)
                    f -= 0.02f;

               if (f < -PI/2)
                    f = -PI/2;
          }

          {
#ifdef SELFRUNNING
               if (!(rand()%50))
                    right = !right;
#else
               keyboard->GetKeyState( keyboard, DIKC_RIGHT, &right );
#endif
               if (right)
                    f += 0.02f;

               if (f > PI/2)
                    f = PI/2;
          }
#ifdef SELFRUNNING
          spawn = 100;
#else
          keyboard->GetKeyState( keyboard, DIKC_SPACE, &spawn );
#endif
          if (spawn) {
               spawn = sx >> 7;
               while (spawn--)
                    spawn_particle();
          }

          draw_particles();

          err = primary->Flip( primary, NULL, DSFLIP_BLIT | DSFLIP_WAITFORSYNC );
          if (err != DFB_OK) {
               DirectFBError( "Failed flipping the primary surface", err );
               break;
          }

          while (keybuffer->GetEvent( keybuffer, &evt ) == DFB_OK) {
               if (evt.type == DIET_KEYPRESS  &&  evt.keycode == DIKC_ESCAPE)
                    quit = 1;
          }
     }

     destroy_particles();

     primary->SetColor( primary, 0, 0, 0, 10 );

     for (i=0; i<70; i++) {
          primary->FillRectangle( primary, 0, 0, sx, sy );

          err = primary->Flip( primary, NULL, DSFLIP_BLIT | DSFLIP_WAITFORSYNC );
          if (err != DFB_OK) {
               DirectFBError( "Failed flipping the primary surface", err );
               break;
          }
     }

     primary->Release( primary );
     keybuffer->Release( keybuffer );
     keyboard->Release( keyboard );
     dfb->Release( dfb );

     return 42;
}

