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

#include <stdio.h>
#include <string.h>

#include <directfb.h>

/* the super interface */
IDirectFB *dfb;

/* the primary surface (surface of primary layer) */
IDirectFBSurface *primary;

/* provider for our images/font */
IDirectFBFont               *font;

/* Input interfaces: device and its buffer */
IDirectFBInputDevice *keyboard;
IDirectFBInputBuffer *key_events;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
        {                                                                      \
           err = x;                                                            \
           if (err != DFB_OK) {                                                \
              fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );           \
              DirectFBErrorFatal( #x, err );                                   \
           }                                                                   \
        }

static char *rules[] = { "CLEAR", "SRC", "SRC OVER", "DST OVER",
                         "SRC IN", "DST IN", "SRC OUT", "DST OUT" };
static int num_rules = sizeof( rules ) / sizeof( rules[0] );

static int screen_width, screen_height;

int main( int argc, char *argv[] )
{
     int i;
     DFBResult err;

     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* get an interface to the primary keyboard
        and create an input buffer for it */
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ));
     DFBCHECK(keyboard->CreateInputBuffer( keyboard, &key_events ));

     /* set our cooperative level to DFSCL_FULLSCREEN
        for exclusive access to the primary layer */
     DFBCHECK(dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN ));

     /* get the primary surface, i.e. the surface of the
        primary layer we have exclusive access to */
     {
          DFBSurfaceDescription dsc;

          memset( &dsc, 0, sizeof(DFBSurfaceDescription) );

          dsc.flags = DSDESC_CAPS;
          dsc.caps = DSCAPS_PRIMARY | DSCAPS_VIDEOONLY | DSCAPS_ALPHA;

          DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
     }

     DFBCHECK(primary->GetSize( primary, &screen_width, &screen_height ));

     primary->SetColor( primary, 30, 40, 50, 50 );
     DFBCHECK(primary->FillRectangle( primary, 0, 0,
                                      screen_width, screen_height ));

     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;

          desc.height = screen_width/32;
          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(primary->SetFont( primary, font ));
          DFBCHECK(primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF ));
          DFBCHECK(primary->DrawString( primary, "Porter/Duff Demo",
                                        screen_width/2, 50, DSTF_CENTER ));
          DFBCHECK(font->Release( font ));

          desc.height = screen_width/42;
          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(primary->SetFont( primary, font ));
     }

     for (i=0; i<num_rules; i++) {
          DFBCHECK(primary->SetDrawingFlags( primary, DSDRAW_NOFX ));
          DFBCHECK(primary->SetColor( primary, 50, 250, 50, 200 ));
          DFBCHECK(primary->FillRectangle( primary, i*(screen_width-40)/num_rules + 40,
                                           130, 50, 70 ));

          DFBCHECK(primary->SetPorterDuff( primary, i+1 ));
          DFBCHECK(primary->SetColor( primary, 250, 50, 50, 100 ));
          DFBCHECK(primary->SetDrawingFlags( primary, DSDRAW_BLEND ));
          DFBCHECK(primary->FillRectangle( primary, i*(screen_width-40)/num_rules + 50,
                                           140, 50, 70 ));

          DFBCHECK(primary->SetPorterDuff( primary, DSPD_NONE ));
          DFBCHECK(primary->SetColor( primary, i*0x1F, i*0x10+0x7f, 0xFF, 0xFF ));
          DFBCHECK(primary->DrawString( primary, rules[i],
                                        i*(screen_width-40)/num_rules + 65, 220,
                                        DSTF_CENTER | DSTF_TOP ));
     }

     key_events->Reset( key_events );
     key_events->WaitForEvent( key_events );

     /* release our interfaces to shutdown DirectFB */
     primary->Release( primary );
     font->Release( font );
     key_events->Release( key_events );
     keyboard->Release( keyboard );
     dfb->Release( dfb );

     return 0;
}

