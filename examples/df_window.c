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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <directfb.h>


/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
     {                                                                \
          err = x;                                                    \
          if (err != DFB_OK) {                                        \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
               DirectFBErrorFatal( #x, err );                         \
          }                                                           \
     }

int main( int argc, char *argv[] )
{
IDirectFB              *dfb;
IDirectFBDisplayLayer  *layer;

IDirectFBImageProvider *provider;
IDirectFBVideoProvider *video_provider;

IDirectFBSurface       *bgsurface;

IDirectFBWindow        *window1;
IDirectFBWindow        *window2;
IDirectFBSurface       *window_surface1;
IDirectFBSurface       *window_surface2;

IDirectFBInputDevice   *keyboard;

IDirectFBFont          *font;

int fontheight;
int err;
int SW, SH;

     DFBCardCapabilities    caps;
     DFBInputDeviceKeyState quit = DIKS_UP;
     IDirectFBWindow*       upper;

     DFBCHECK(DirectFBInit( &argc, &argv ));

     DFBCHECK(DirectFBCreate( &dfb ));

     dfb->GetCardCapabilities( dfb, &caps );

     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard));

     dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );

     if (!((caps.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
           (caps.blitting_flags & DSBLIT_BLEND_COLORALPHA  )))
     {
          layer->SetBufferMode( layer, DLBM_BACKSYSTEM );
     }

     layer->GetSize( layer, &SW, &SH );
     layer->EnableCursor ( layer, 1 );

     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = SW/50;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          font->GetHeight( font, &fontheight );
     }

     if (argc < 2 ||
         dfb->CreateVideoProvider( dfb, argv[1], &video_provider ) != DFB_OK)
     {
          video_provider = NULL;
     }

     {
          DFBSurfaceDescription desc;

          DFBCHECK(dfb->CreateImageProvider( dfb,
                                             DATADIR"/examples/desktop.png",
                                             &provider ));

          desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
          desc.width = SW;
          desc.height = SH;

          DFBCHECK(dfb->CreateSurface( dfb, &desc, &bgsurface ) );

          provider->RenderTo( provider, bgsurface );
          provider->Release( provider );

          DFBCHECK(bgsurface->SetFont( bgsurface, font ));

          bgsurface->SetColor( bgsurface, 0xCF, 0xCF, 0xFF, 0xFF );
          bgsurface->DrawString( bgsurface,
                                 "Move the mouse over a window to activate it.",
                                 -1, 0, 0, DSTF_LEFT | DSTF_TOP );

          bgsurface->SetColor( bgsurface, 0xCF, 0xDF, 0xCF, 0xFF );
          bgsurface->DrawString( bgsurface,
                                 "Press left mouse button and drag to move the window.",
                                 -1, 0, fontheight, DSTF_LEFT | DSTF_TOP );

          bgsurface->SetColor( bgsurface, 0xCF, 0xEF, 0x9F, 0xFF );
          bgsurface->DrawString( bgsurface,
                                 "Press middle mouse button to raise/lower the window.",
                                 -1, 0, fontheight * 2, DSTF_LEFT | DSTF_TOP );

          bgsurface->SetColor( bgsurface, 0xCF, 0xFF, 0x6F, 0xFF );
          bgsurface->DrawString( bgsurface,
                                 "Press right mouse button when you are done.", -1,
                                 0, fontheight * 3,
                                 DSTF_LEFT | DSTF_TOP );

          layer->SetBackgroundImage( layer, bgsurface );
          layer->SetBackgroundMode( layer, DLBM_IMAGE );
     }

     {
          DFBWindowDescription desc;

          desc.flags = ( DWDESC_POSX | DWDESC_POSY |
                         DWDESC_WIDTH | DWDESC_HEIGHT );

          if (!video_provider) {
               desc.caps = DWCAPS_ALPHACHANNEL;
               desc.flags |= DWDESC_CAPS;
          }

          desc.posx = 20;
          desc.posy = 120;
          desc.width = 768/2;
          desc.height = 576/2;

          DFBCHECK( layer->CreateWindow( layer, &desc, &window2 ) );
          window2->GetSurface( window2, &window_surface2 );

          window2->SetOpacity( window2, 0xFF );

          if (video_provider)
          {
               video_provider->PlayTo( video_provider, window_surface2,
                                       NULL, NULL, NULL );
          }
          else
          {
               window_surface2->SetColor( window_surface2,
                                          0x00, 0x30, 0x10, 0xc0 );
               window_surface2->DrawRectangle( window_surface2, 0, 0,
                                               desc.width, desc.height );
               window_surface2->SetColor( window_surface2,
                                          0x80, 0xa0, 0x00, 0x90 );
               window_surface2->FillRectangle( window_surface2, 1, 1,
                                               desc.width-2, desc.height-2 );
          }

          window_surface2->Flip( window_surface2, NULL, 0 );
     }

     {
          DFBWindowDescription desc;

          desc.flags = ( DWDESC_POSX | DWDESC_POSY |
                         DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS );
          desc.posx = 200;
          desc.posy = 200;
          desc.width = 512;
          desc.height = 145;
          desc.caps = DWCAPS_ALPHACHANNEL;

          DFBCHECK(layer->CreateWindow( layer, &desc, &window1 ) );
          window1->GetSurface( window1, &window_surface1 );

          DFBCHECK(dfb->CreateImageProvider( dfb,
                                             DATADIR"/examples/dfblogo.png",
                                             &provider ));
          provider->RenderTo( provider, window_surface1 );

          window_surface1->SetColor( window_surface1, 0xFF, 0x20, 0x20, 0x90 );
          window_surface1->DrawRectangle( window_surface1, 0, 0,
                                          desc.width, desc.height );

          window_surface1->Flip( window_surface1, NULL, 0 );

          provider->Release( provider );

          window1->SetOpacity( window1, 0xFF );
     }

     window1->RequestFocus( window1 );
     window1->RaiseToTop( window1 );
     upper = window1;

     while (quit == DIKS_UP) {

          static IDirectFBWindow* active = NULL;
          static IDirectFBWindow* window = NULL;
          static int grabbed = 0;
          static int startx = 0;
          static int starty = 0;
          static int endx = 0;
          static int endy = 0;
          DFBWindowEvent evt;

          keyboard->GetKeyState( keyboard, DIKC_ESCAPE, &quit );

          if (!window)
               window = active ? active : window1;

          while (window->GetEvent( window, &evt ) == DFB_OK)
          {
               if (active) {
                    switch (evt.type) {

                    case DWET_BUTTONDOWN:
                         if (!grabbed && evt.button == DIBI_LEFT) {
                              grabbed = 1;
                              layer->GetCursorPosition( layer,
                                                        &startx, &starty );
                              window->GrabPointer( window );
                         }
                         break;

                    case DWET_BUTTONUP:
                         switch (evt.button) {
                         case DIBI_LEFT:
                              if (grabbed) {
                                   window->UngrabPointer( window );
                                   grabbed = 0;
                              }
                              break;
                         case DIBI_MIDDLE:
                              upper->LowerToBottom( upper );
                              upper = (upper == window1) ? window2 : window1;
                              break;
                         case DIBI_RIGHT:
                              quit = DIKS_DOWN;
                              break;
                         default:
                              break;
                         }
                         break;

                    case DWET_KEYDOWN:
                         if (grabbed)
                              break;
                         switch (evt.keycode) {
                         case DIKC_RIGHT:
                              active->Move (active, 1, 0);
                              break;
                         case DIKC_LEFT:
                              active->Move (active, -1, 0);
                              break;
                         case DIKC_UP:
                              active->Move (active, 0, -1);
                              break;
                         case DIKC_DOWN:
                              active->Move (active, 0, 1);
                              break;
                         default:
                              break;
                         }
                         break;

                    case DWET_LOSTFOCUS:
                         if (!grabbed)
                              active = NULL;
                         break;

                    default:
                         break;

                    }
               }
               else if (evt.type == DWET_GOTFOCUS)
                    active = window;

               if (evt.type == DWET_MOTION) {
                    endx = evt.cx;
                    endy = evt.cy;
               }
          }

          if (video_provider)
               window_surface2->Flip( window_surface2, NULL, 0 );

          if (active) {
               if (grabbed) {
                    active->Move( active, endx - startx, endy - starty);
                    startx = endx;
                    starty = endy;
               }
               active->SetOpacity( active,
                                   (sin( clock()/500000.0 ) * 100) + 155 );
          }
          else {
               window = (window == window1) ? window2 : window1;
          }
     }

     if (video_provider)
          video_provider->Release( video_provider );

     window_surface2->Release( window_surface2 );
     window_surface1->Release( window_surface1 );
     window2->Release( window2 );
     window1->Release( window1 );
     layer->Release( layer );
     bgsurface->Release( bgsurface );
     dfb->Release( dfb );

     return 42;
}
