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

//#define SELFRUNNING

#include <directfb.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

/* height and width of the penguin surface (more that one penguin on it) */
#define XTUXSIZE 400
#define YTUXSIZE 240

/* height and width of one sprite */
#define XSPRITESIZE 40
#define YSPRITESIZE 60

/* new animation frame is set every ANIMATION_DELAY steps */
#define ANIMATION_DELAY 5

/* resolution of the destionation mask */
int DESTINATION_MASK_WIDTH;
int DESTINATION_MASK_HEIGHT;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                    \
     {                                                                    \
          err = x;                                                        \
          if (err != DFB_OK) {                                            \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );     \
               DirectFBErrorFatal( #x, err );                             \
          }                                                               \
     }

/* DirectFB interfaces needed by df_andi */
IDirectFB               *dfb;
IDirectFBSurface        *primary;
IDirectFBDisplayLayer   *layer;
IDirectFBInputDevice    *keyboard;
IDirectFBInputBuffer    *keybuffer;
IDirectFBImageProvider  *provider;
IDirectFBFont           *font;

/* DirectFB surfaces used by df_andi */
IDirectFBSurface *tuximage;
IDirectFBSurface *background;
IDirectFBSurface *destination_mask;

/* values on which placement of penguins and text depends */
int xres;
int yres;
int fontheight;


/* anmation frame */
typedef struct _PenguinFrame {
     DFBRectangle rect;
     struct _PenguinFrame *next;
} PenguinFrame;

/* linked lists of animation frames */
PenguinFrame* left_frames = NULL;
PenguinFrame* right_frames = NULL;
PenguinFrame* up_frames = NULL;
PenguinFrame* down_frames = NULL;

typedef enum {
     DIR_LEFT,
     DIR_RIGHT,
     DIR_UP,
     DIR_DOWN
} Direction;

/* penguin struct, needed for each penguin on the screen */
typedef struct _Penguin {
     int x, y;
     int x_togo;
     int y_togo;
     int moving;
     int sprite_nr;
     int delay;
     PenguinFrame *left_frame;
     PenguinFrame *right_frame;
     PenguinFrame *up_frame;
     PenguinFrame *down_frame;
     PenguinFrame **frameset;
     struct _Penguin *next;
} Penguin;

/* needed for the penguin linked list */
Penguin *penguins     = NULL;
Penguin *last_penguin = NULL;

/* number of penguins currently on screen */
int population = 0;

/* number of destination coordinates in coords array */
int nr_coords = 0;

/* coors array has hardcoded maximum possible length */
int *coords;

/*
 * adds one penguin to the list, and sets initial state
 */
void spawn_penguin()
{
     Penguin *new_penguin = (Penguin*)malloc( sizeof(Penguin) );

     new_penguin->x = xres/2;
     new_penguin->y = yres/2;
     new_penguin->x_togo = 0;
     new_penguin->y_togo = 0;
     new_penguin->moving = 1;

     new_penguin->delay = 5;

     new_penguin->left_frame = left_frames;
     new_penguin->right_frame = right_frames;
     new_penguin->up_frame = up_frames;
     new_penguin->down_frame = down_frames;

     new_penguin->next = NULL;

     if (!penguins) {
          penguins = new_penguin;
          last_penguin = new_penguin;
     }
     else {
          last_penguin->next = new_penguin;
          last_penguin = new_penguin;
     }
     population++;
}

/*
 * removes one penguin (the first) from the list
 */
void destroy_penguin()
{
     Penguin *first_penguin = penguins;

     if (penguins) {
          penguins = penguins->next;
          free (first_penguin);
          population--;
     }
}

/*
 * removes a given number of penguins
 */
void destroy_penguins(int number)
{
     while (number--)
       destroy_penguin();
}


/*
 * adds a given number of penguins
 */
void spawn_penguins(int number)
{
     while (number--)
          spawn_penguin();
}

/*
 * blits all penguins to the screen
 */
void draw_penguins()
{
     Penguin *p = penguins;
     DFBResult err;

     primary->SetBlittingFlags( primary, DSBLIT_SRC_COLORKEY );

     while (p) {
          DFBCHECK(primary->Blit( primary,tuximage, &((*(p->frameset))->rect),
                                  p->x, p->y ));
          p = p->next;
     }
}

/*
 *  moves and clipps all penguins, penguins that are in formation shiver,
 *  penguins not in formation "walk"
 */
void move_penguins()
{
     Penguin *p = penguins;

     while (p) {
          if (p->x_togo == 0 && p->y_togo == 0) {
               /* walking penguins get new destination if they have reached
                  their destination */
               if (p->moving) {
                    p->x_togo = (rand()%100) - 50;
                    p->y_togo = (rand()%100) - 50;
               }
               /* penguins that have reached their
                  formation point jitter */
               else {
                    p->frameset  = &p->down_frame;
                    if (rand()%3) {
                         p->x+=rand()%3 - 1;
                         p->y+=rand()%3 - 1;
                    }
               }
          }
          /* increase/decrease coordinates and to go variables */
          if (p->x_togo > 0) {
               p->x--;
               p->x_togo--;

               p->frameset  = &p->left_frame;
          }
          if (p->x_togo < 0) {
               p->x++;
               p->x_togo++;

               p->frameset  = &p->right_frame;
          }
          if (p->y_togo > 0) {
               p->y--;
               p->y_togo--;

               p->frameset  = &p->up_frame;
          }
          if (p->y_togo < 0) {
               p->y++;
               p->y_togo++;

               p->frameset  = &p->down_frame;
          }

          /* clip penguin */
          if (p->x < 0)
               p->x = 0;

          if (p->y < 0)
               p->y = 0;

          if (p->x > xres - XSPRITESIZE)
               p->x = xres - XSPRITESIZE;

          if (p->y > yres - YSPRITESIZE)
               p->y = yres - YSPRITESIZE;

          if (p->delay == 0) {
               *(p->frameset) = (*(p->frameset))->next;
               p->delay = 5;
          }
          else {
               p->delay--;
          }

          p = p->next;
     }
}

/*
 * searches a destination point in the coords array for each penguin
 */
void penguins_search_destination() {
     Penguin *p = penguins;
     if (nr_coords) {
          while (p) {
               int entry = (rand()%nr_coords) * 2;

               p->x_togo= p->x - coords[entry]   * xres/1000.0f;
               p->y_togo= p->y - coords[entry+1] * yres/1000.0f;
               p->moving = 0;

               p = p->next;
          }
     }
}

/*
 * removes all penguins
 */
void destroy_all_penguins()
{
     Penguin *p = penguins;

     while (p) {
          penguins = p->next;
          free(p);
          p = penguins;
     }
}

/*
 * revives all penguins, penguins that are in formation move again
 */
void revive_penguins()
{
     Penguin *p = penguins;

     while (p) {
          p->moving = 1;
          p = p->next;
     }
}

/*
 * interprets the destination mask from the destination_mask surface, all back
 * pixels become formation points, and are stored in the coords array
 */
void read_destination_mask()
{
     unsigned int x,y,pitch;
     unsigned int *src;
     unsigned int skip;

     destination_mask->Lock(destination_mask, DSLF_READ, (void**)&src, &pitch);
     skip = pitch/4 - DESTINATION_MASK_WIDTH;

     coords = malloc( sizeof(int) * DESTINATION_MASK_WIDTH * DESTINATION_MASK_HEIGHT );

     for (y=0;y<DESTINATION_MASK_HEIGHT;y++) {
          for (x=0;x<DESTINATION_MASK_WIDTH;x++) {
               if ((*src & 0x00FFFFFF) == 0) {
                    coords[nr_coords*2  ] = (x *(1000/DESTINATION_MASK_WIDTH));
                    coords[nr_coords*2+1] = (y *(1000/DESTINATION_MASK_HEIGHT));
                    nr_coords++;
                    printf("X");
               }
               else {
                    printf("O");
               }
               src++;
          }
          src += skip;
          printf("\n");
     }
     destination_mask->Unlock(destination_mask);
}

/*
 * initializes the animation frames for a specified direction at yoffset
 */
void initialize_direction_frames(PenguinFrame **direction_frames, int yoffset)
{
     PenguinFrame* new_frame = NULL;
     PenguinFrame* last_frame = NULL;

     if (!*direction_frames) {
          int i;

          for (i = 0; i < (XTUXSIZE/XSPRITESIZE - 1) ;i++) {
               new_frame = (PenguinFrame*) malloc( sizeof(PenguinFrame) ); //FIXME: leak

               new_frame->rect.x = i*XSPRITESIZE;
               new_frame->rect.y = YSPRITESIZE*yoffset;
               new_frame->rect.w = XSPRITESIZE;
               new_frame->rect.h = YSPRITESIZE;

               if (!*direction_frames) {
                    *direction_frames = new_frame;
               }
               else {
                    last_frame->next = new_frame;
               }
               last_frame = new_frame;
          }
          last_frame->next = *direction_frames;
     }
}

/*
 * initialize all animation frames
 */
void initialize_animation()
{
     initialize_direction_frames( &down_frames,  0 );
     initialize_direction_frames( &left_frames,  1 );
     initialize_direction_frames( &up_frames,    2 );
     initialize_direction_frames( &right_frames, 3 );
}

/*
 * set up DirectFB and load resources
 */
void init_resources( int argc, char *argv[] )
{
     DFBResult err;
     DFBSurfaceDescription dsc;

     srand((long)time(0));

     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* get an interface to the primary keyboard and create an
        input buffer for it */
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ));
     DFBCHECK(keyboard->CreateInputBuffer( keyboard, &keybuffer ));

     /* set our cooperative level to DFSCL_FULLSCREEN for exclusive access to
        the primary layer */
     err = dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
     if (err)
       DirectFBError( "Failed to get exclusive access", err );

     DFBCHECK(dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer ));

     /* get the primary surface, i.e. the surface of the primary layer we have
        exclusive access to */
     memset( &dsc, 0, sizeof(DFBSurfaceDescription) );
     dsc.flags = DSDESC_CAPS;
     dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
     DFBCHECK(primary->GetSize( primary, &xres, &yres ));

     /* load font */
     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = 24;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(font->GetHeight( font, &fontheight ));
          DFBCHECK(primary->SetFont( primary, font ));
     }

     /* load penguin animation */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/tux.png",
                                        &provider ));

     DFBCHECK (provider->GetSurfaceDescription (provider, &dsc));
     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &tuximage ));

     DFBCHECK(provider->RenderTo( provider, tuximage ));
     provider->Release( provider );

     /* load the background image */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/wood_andi.jpg",
                                        &provider ));

     DFBCHECK (provider->GetSurfaceDescription (provider, &dsc));
     dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
     dsc.width = xres;
     dsc.height = yres;
     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &background ));

     DFBCHECK(provider->RenderTo( provider, background ));
     provider->Release( provider );

     /* load the penguin destination mask */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR
                                        "/examples/destination_mask.png",
                                        &provider ));

     DFBCHECK(provider->GetSurfaceDescription( provider, &dsc ));

     dsc.pixelformat = DSPF_RGB32;

     DESTINATION_MASK_WIDTH  = dsc.width;
     DESTINATION_MASK_HEIGHT = dsc.height;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &destination_mask ));

     DFBCHECK(provider->RenderTo( provider, destination_mask ));
     provider->Release( provider );
}

/*
 * deinitializes resources and DirectFB
 */
void deinit_resources()
{
     free( coords );

     destroy_all_penguins();

     tuximage->Release( tuximage );
     background->Release( background );
     destination_mask->Release( destination_mask );
     primary->Release( primary );
     keybuffer->Release( keybuffer );
     keyboard->Release( keyboard );
     layer->Release( layer );
     dfb->Release( dfb );

}

int main( int argc, char *argv[] )
{
     DFBResult err;
     int quit = 0;
     int clipping = 0;

     init_resources( argc, argv );

     read_destination_mask();

     initialize_animation();

#ifdef SELFRUNNING
     /* begin with 500 penguins in selfrunning mode */
     spawn_penguins( 500 );
#else
     spawn_penguins( 200 );
#endif

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );

     /* main loop */
     while (!quit) {
          DFBInputEvent evt;
          char population_string[10];

#ifdef SELFRUNNING
          /* in selfrunning mode change is 200:1 that the
             penguins will go into formation in every frame */
          if (rand()%200 == 0)
               penguins_search_destination();
#endif
          /* move the penguins 3 times, thats faster ;-) */
          move_penguins();
          move_penguins();

          /* draw the background image */
          primary->SetBlittingFlags( primary, DSBLIT_NOFX );
          DFBCHECK(primary->Blit( primary, background, NULL, 0, 0 ));

          /* draw all penguins */
          draw_penguins();

          /* draw the population string in upper left corner */
          primary->SetColor( primary, 0, 0, 60, 0xa0 );
          primary->FillRectangle( primary, 0, 0, 300, fontheight+5 );

          primary->SetColor( primary, 200, 200, 255, 0xFF );
          primary->DrawString( primary, "Penguin Population:", -1,
                               10, 0, DSTF_LEFT | DSTF_TOP );

          sprintf( population_string, "%d",population );

          primary->DrawString( primary, population_string, -1,
                               290, 0, DSTF_RIGHT | DSTF_TOP );

          /* flip display */
          DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

          /* process keybuffer */
          while (keybuffer->GetEvent( keybuffer, &evt) == DFB_OK) {
               if (evt.type == DIET_KEYPRESS) {
                    switch (evt.keycode) {
                         case DIKC_ESCAPE:
                              /* quit main loop */
                              quit = 1;
                              break;
                         case DIKC_SPACE:
                         case DIKC_ENTER:
                              /* penguins go in formation */
                              penguins_search_destination();
                              break;
                         case DIKC_S:
                              /* add another penguin */
                              spawn_penguins(10);
                              break;
                         case DIKC_R:
                              /* penguins in formation will walk around again */
                              revive_penguins();
                              break;
                         case DIKC_D:
                              /* removes one penguin */
                              destroy_penguins(10);
                              break;
                         case DIKC_C:
                              /*toggle clipping*/
                              clipping=!clipping;
                              if (clipping) {
                                   DFBRegion clipregion = { 100,100, xres-100, yres-100 };
                                   primary->SetClip( primary, &clipregion );
                              }
                              else
                                   primary->SetClip( primary, NULL );
                              break;
                         default:
                              break;
                    }
               }
          }
     }

     deinit_resources();
     return 42;
}
