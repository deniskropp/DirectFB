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
          err = x;                                                        \
          if (err != DFB_OK) {                                            \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );     \
               DirectFBErrorFatal( #x, err );                             \
          }                                                               \
     }

/* DirectFB interfaces needed by df_andi */
IDirectFB               *dfb;
IDirectFBSurface        *primary;
IDirectFBInputDevice    *keyboard;
IDirectFBInputBuffer    *keybuffer;
IDirectFBImageProvider  *provider;
IDirectFBFont           *font;

/* DirectFB surfaces used by df_andi */
IDirectFBSurface *smokey_light;

int xres;
int yres;

static int intro()
{
  DFBResult err;

  int   l       = 0;
  int   jitter1 = yres/100 + 1;
  int   jitter2 = (jitter1 - 1) / 2;
  char *lines[] = {
    "3",
    "2",
    "1",
    NULL
  };

  primary->SetDrawingFlags( primary, DSDRAW_NOFX );
  primary->SetBlittingFlags( primary, DSBLIT_NOFX );

  while (lines[l])
    {
      int frames = 200;

      while (frames--)
	{
	  primary->SetColor( primary, 0, 0, 0, 0 );
	  primary->FillRectangle( primary, 0, 0, xres, yres );

	  primary->SetColor( primary,
			     0x40 + rand()%0xC0, 0x80 + rand()%0x80, 0x80 + rand()%0x80, 0xff );
	  primary->DrawString( primary, lines[l], -1,
			       xres/2 + rand()%jitter1-jitter2,
			       yres/2 + rand()%jitter1-jitter2, DSTF_CENTER );
      
	  /* flip display */
	  DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

	  pthread_testcancel();
	}

      ++l;
    }

  return 0;
}

static int demo1()
{
  int i;
  int frames = 400;
  DFBResult err;
  double b = 0;

  primary->SetDrawingFlags( primary, DSDRAW_NOFX );
  primary->SetBlittingFlags( primary, DSBLIT_NOFX );

  primary->SetColor( primary, 0xff, 0xff, 0xff, 0xff );
  primary->FillRectangle( primary, 0, 0, xres, yres );

  /* flip display */
  DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

  pthread_testcancel();


  primary->SetColor( primary, 0, 0, 0, 0 );
  primary->FillRectangle( primary, 0, 0, xres, yres );
  
  for (i=0; i<30; i++)
    {
      usleep( 40000 );

      /* flip display */
      DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

      pthread_testcancel();
    }

  while (frames--)
    {
      double f;
      DFBRectangle rect;

      primary->FillRectangle( primary, 0, 0, xres, yres );

      f = cos(b) * 30  +  sin(b+0.5) * 40;

      rect.w = (int)((sin(f*cos(f/10.0))/2 + 1.2)*800);
      rect.h = (int)((sin(f*sin(f/10.0)) + 1.2)*300);

      rect.x = (xres - rect.w) / 2;
      rect.y = (yres - rect.h) / 2;

      primary->StretchBlit( primary, smokey_light, NULL, &rect );

      b += .001;

      /* flip display */
      DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

      pthread_testcancel();
    }

  return 0;
}

static int demo2()
{
  DFBResult err;
  int frames = 400;
  int xres2 = xres/2;
  int yres2 = yres/2;
  double b = 0;

  primary->SetDrawingFlags( primary, DSDRAW_BLEND );
  primary->SetBlittingFlags( primary, DSBLIT_NOFX );

  while (frames--)
    {
      double w;

      primary->SetColor( primary, 0, 0, 0, 0x10 );
      primary->FillRectangle( primary, 0, 0, xres, yres );

      for (w=b; w<=b+6.29; w+=.05)
	{
	  primary->SetColor( primary,
			     sin(1*w+b) *127+127,
			     sin(2*w-b) *127+127,
			     sin(3*w+b) *127+127,
			     sin(4*w-b) *127+127 );
	  primary->DrawLine( primary, xres2, yres2,
			     xres2 + cos(w)*xres2, yres2 + sin(w)*yres2 );
	}

      b += .02;

      /* flip display */
      DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

      pthread_testcancel();
    }

  primary->SetColor( primary, 0, 0, 0, 0x10 );

  for (frames=0; frames<75; frames++)
    {
      primary->FillRectangle( primary, 0, 0, xres, yres );

      /* flip display */
      DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC | DSFLIP_BLIT ));

      pthread_testcancel();
    }

  return 0;
}

static int (*demos[])() = { intro, demo1, demo2, NULL };

static void* demo_loop (void *arg)
{
  DFBResult err;
  int d = 0;

  while (demos[d])
    {
      if (demos[d]())
	break;

      ++d;
    }

  primary->SetColor( primary, 0, 0, 0, 0 );
  primary->FillRectangle( primary, 0, 0, xres, yres );

  primary->SetColor( primary, 0xff, 0xff, 0xff, 0xff );
  primary->DrawString( primary, "The End", -1, xres/2, yres/2, DSTF_CENTER );
      
  /* flip display */
  DFBCHECK(primary->Flip( primary, NULL, DSFLIP_WAITFORSYNC ));

  return NULL;
}

int main( int argc, char *argv[] )
{
  pthread_t demo_loop_thread = -1;
  DFBResult err;
  int quit = 0;

  DFBCardCapabilities   caps;
  DFBSurfaceDescription dsc;

  srand((long)time(0));

  DFBCHECK(DirectFBInit( &argc, &argv ));

  /* create the super interface */
  DFBCHECK(DirectFBCreate( &dfb ));

  DFBCHECK(dfb->GetCardCapabilities( dfb, &caps ));

  if (!(caps.drawing_flags & DSDRAW_BLEND))
    {
      printf( "\n\tpss requires a graphics card with alpha blending support!\n\n" );
      return 1;
    }

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
  dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING | DSCAPS_VIDEOONLY;
  
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

  /* load smokey_light */
  DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/smokey_light.jpg",
				     &provider ));

  DFBCHECK(provider->GetSurfaceDescription (provider, &dsc));
  DFBCHECK(dfb->CreateSurface( dfb, &dsc, &smokey_light ));

  DFBCHECK(provider->RenderTo( provider, smokey_light ));
  provider->Release( provider );


  /* main loop */
  while (!quit)
    {
      DFBInputEvent evt;

      if (demo_loop_thread == -1)
	pthread_create( &demo_loop_thread, NULL, demo_loop, NULL );

      keybuffer->WaitForEvent( keybuffer );

      /* process keybuffer */
      while (keybuffer->GetEvent( keybuffer, &evt) == DFB_OK)
	{
	  if (evt.type == DIET_KEYPRESS)
	    {
	      switch (evt.keycode)
		{
		case DIKC_ESCAPE:
		  /* quit main loop */
		  quit = 1;

		  /* fall through */

		default:
		  pthread_cancel( demo_loop_thread );
		  pthread_join( demo_loop_thread, NULL );
		  demo_loop_thread = -1;
		  break;
		}
	    }
	}
    }

  smokey_light->Release( smokey_light );
  keybuffer->Release( keybuffer );
  keyboard->Release( keyboard );
  primary->Release( primary );
  dfb->Release( dfb );

  return 0;
}
