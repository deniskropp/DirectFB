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

//#define VIDEO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <directfb.h>

IDirectFB *dfb;
IDirectFBDisplayLayer *layer;

IDirectFBImageProvider *provider;
#ifdef VIDEO
IDirectFBVideoProvider *videoprovider;
#endif

IDirectFBSurface *bgsurface;

IDirectFBWindow *window1;
IDirectFBSurface *window_surface1;

IDirectFBWindow *window2;
IDirectFBSurface *window_surface2;

IDirectFBInputDevice *keyboard;
IDirectFBInputDevice *mouse;
IDirectFBInputBuffer *mouse_events;

IDirectFBFont     *font;
static int ascender;
static int descender;
static int fontheight;

int err;
int SW, SH;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
        {                                                                      \
           err = x;                                                            \
           if (err != DFB_OK) {                                                \
              fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );           \
              DirectFBErrorFatal( #x, err );                                   \
           }                                                                   \
        }

int enum_layers_callback( unsigned int id, unsigned int caps, void *data )
{
     printf( "\ndf_window: Found Layer %d\n", id );

     if (id == DLID_PRIMARY)
          DFBCHECK( dfb->GetDisplayLayer( dfb, id, &layer ) );

     layer->EnableCursor( layer, 0 );

     return 0;
}

int main( int argc, char *argv[] )
{
     DFBCardCapabilities    caps;
     DFBInputDeviceKeyState quit = DIKS_UP;

     DFBCHECK(DirectFBCreate( &argc, &argv, &dfb ));

     dfb->GetCardCapabilities( dfb, &caps );

     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard));

     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_MOUSE, &mouse ));
     DFBCHECK(mouse->CreateInputBuffer( mouse, &mouse_events ));

     dfb->EnumDisplayLayers( dfb, enum_layers_callback, NULL );

     if (!((caps.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
           (caps.blitting_flags & DSBLIT_BLEND_COLORALPHA  )))
     {
          layer->SetBufferMode( layer, DLBM_BACKSYSTEM );
     }

     layer->GetSize( layer, &SW, &SH );

     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = SW/50;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));

          DFBCHECK(font->GetAscender( font, &ascender ));
          DFBCHECK(font->GetDescender( font, &descender ));
          DFBCHECK(font->GetHeight( font, &fontheight ));
     }

     {
          DFBSurfaceDescription desc;

          DFBCHECK(dfb->CreateImageProvider( dfb,
                                             DATADIR"/examples/desktop.png",
                                             &provider ));
          DFBCHECK (provider->GetSurfaceDescription (provider, &desc));

          desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
          desc.width = SW;
          desc.height = SH;

          DFBCHECK(dfb->CreateSurface( dfb, &desc, &bgsurface ) );

          DFBCHECK(provider->RenderTo( provider, bgsurface ));
          provider->Release( provider );

          DFBCHECK(bgsurface->SetFont( bgsurface, font ));

          bgsurface->SetColor( bgsurface, 0xCF, 0xCF, 0xFF, 0xFF );
          DFBCHECK(bgsurface->DrawString( bgsurface,
                            "Move the mouse and the active window will follow!",
                             0, 0, DSTF_LEFT | DSTF_TOP ));

          bgsurface->SetColor( bgsurface, 0xCF, 0xDF, 0xCF, 0xFF );
          DFBCHECK(bgsurface->DrawString( bgsurface,
                            "Press left mouse button for switching the active"
                            " window and raising it!", 0, fontheight,
                            DSTF_LEFT | DSTF_TOP ));

          bgsurface->SetColor( bgsurface, 0xCF, 0xEF, 0x9F, 0xFF );
          DFBCHECK(bgsurface->DrawString( bgsurface,
                            "Press middle mouse button to switch only!",
                             0, fontheight*2,
                             DSTF_LEFT | DSTF_TOP ));

          bgsurface->SetColor( bgsurface, 0xCF, 0xFF, 0x6F, 0xFF );
          DFBCHECK(bgsurface->DrawString( bgsurface,
                            "Press right mouse button when you are done!",
                             0, fontheight*3,
                             DSTF_LEFT | DSTF_TOP ));

          layer->SetBackgroundImage( layer, bgsurface );
          layer->SetBackgroundMode( layer, DLBM_IMAGE );
     }

     {
          DFBWindowDescription desc;

#ifdef VIDEO
          desc.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT;
#else
          desc.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH |
                       DWDESC_HEIGHT | DWDESC_CAPS;
          desc.caps = DWCAPS_ALPHACHANNEL;
#endif
          desc.posx = 0;
          desc.posy = 100;
          desc.width = 768/2;
          desc.height = 576/2;

          DFBCHECK( layer->CreateWindow( layer, &desc, &window2 ) );
          DFBCHECK( window2->GetSurface( window2, &window_surface2 ) );

          window2->SetOpacity( window2, 0xFF );

#ifdef VIDEO
          window_surface2->SetColor( window_surface2, 0x00, 0x00, 0x00, 0xB0 );
          window_surface2->FillRectangle( window_surface2, 0, 0,
                                          desc.width, desc.height );
          DFBCHECK(dfb->CreateVideoProvider( dfb, "/home/dok/Wargames.avi",
                                             &videoprovider ));
          DFBCHECK(videoprovider->PlayTo( videoprovider, window_surface2,
                                          NULL, NULL, NULL ));
#else
          window_surface2->SetColor( window_surface2, 0x00, 0x00, 0x20, 0x40 );
          window_surface2->DrawRectangle( window_surface2, 0, 0,
                                          desc.width, desc.height );
          window_surface2->SetColor( window_surface2, 0x00, 0x00, 0x60, 0x70 );
          window_surface2->FillRectangle( window_surface2, 1, 1,
                                          desc.width-2, desc.height-2 );
#endif

          window_surface2->Flip( window_surface2, NULL, 0 );
     }

     {
          DFBWindowDescription desc;

          desc.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH |
                       DWDESC_HEIGHT | DWDESC_CAPS;
          desc.posx = 200;
          desc.posy = 200;
          desc.width = 512;
          desc.height = 145;
          desc.caps = DWCAPS_ALPHACHANNEL;

          DFBCHECK(layer->CreateWindow( layer, &desc, &window1 ) );
          DFBCHECK(window1->GetSurface( window1, &window_surface1 ) );

          DFBCHECK(dfb->CreateImageProvider( dfb,
                                             DATADIR"/examples/dfblogo.png",
                                             &provider ));
          DFBCHECK(provider->RenderTo( provider, window_surface1 ));

          window_surface1->SetColor( window_surface1, 0xFF, 0x20, 0x20, 0x90 );
          window_surface1->DrawRectangle( window_surface1, 0, 0,
                                          desc.width, desc.height );

          window_surface1->Flip( window_surface1, NULL, 0 );

#ifdef DIRTY_SUBSURFACE_TEST
          {
               DFBRectangle rect = { 10, 10, 200, 70 };

               DFBCHECK(window_surface1->GetSubSurface( window_surface1, &rect,
                                                        &window_surface1 ) );
               DFBCHECK(provider->RenderTo( provider, window_surface1 ));

               window_surface1->SetColor( window_surface1, 0xFF, 0, 0, 0xFF );
               window_surface1->FillRectangle( window_surface1, 0, 0, 800, 5);
          }
          {
               DFBRectangle rect = { 10, 10, 200, 70 };

               DFBCHECK(window_surface1->GetSubSurface( window_surface1, &rect,
                                                        &window_surface1 ) );
               DFBCHECK(provider->RenderTo( provider, window_surface1 ));

               window_surface1->SetColor( window_surface1, 0xFF, 0, 0, 0xFF );
               window_surface1->FillRectangle( window_surface1, 0, 0, 800, 5);
          }
          {
               DFBRectangle rect = { 10, 10, 200, 70 };

               DFBCHECK(window_surface1->GetSubSurface( window_surface1, &rect,
                                                        &window_surface1 ) );
               DFBCHECK(provider->RenderTo( provider, window_surface1 ));

               window_surface1->SetColor( window_surface1, 0xFF, 0, 0, 0xFF );
               window_surface1->FillRectangle( window_surface1, 0, 0, 800, 5);
          }
          {
               DFBRectangle rect = { 10, 10, 200, 70 };

               DFBCHECK(window_surface1->GetSubSurface( window_surface1, &rect,
                                                        &window_surface1 ) );
               DFBCHECK(provider->RenderTo( provider, window_surface1 ));

               window_surface1->SetColor( window_surface1, 0xFF, 0, 0, 0xFF );
               window_surface1->FillRectangle( window_surface1, 0, 0, 800, 5);
          }
          {
               DFBRectangle rect = { 10, 10, 200, 50 };

               DFBCHECK(window_surface1->GetSubSurface( window_surface1, &rect,
                                                        &window_surface1 ) );
               DFBCHECK(provider->RenderTo( provider, window_surface1 ));

               window_surface1->SetColor( window_surface1, 0xFF, 0, 0, 0xFF );
               window_surface1->FillRectangle( window_surface1, 0, 0, 800, 5);
          }
#endif

          provider->Release( provider );

          window1->SetOpacity( window1, 0xFF );
     }

     while (quit == DIKS_UP) {
          DFBInputEvent ev;
          static IDirectFBWindow* movw = NULL;
          int movx = 0;
          int movy = 0;
          static int wdown = 0;

          if (!movw)
               movw = window1;

          keyboard->GetKeyState( keyboard, DIKC_ESCAPE, &quit );

          while (mouse_events->GetEvent( mouse_events, &ev ) == DFB_OK) {
               if (ev.type == DIET_AXISMOTION) {
                    switch (ev.axis) {
                         case DIAI_X:
                              movx += ev.axisrel;
                              break;
                         case DIAI_Y:
                              movy += ev.axisrel;
                              break;
                         default:
                              break;
                    }
               } else
               if (ev.type == DIET_BUTTONRELEASE) {
                    switch (ev.button) {
                         case DIBI_LEFT:
                              if (!wdown)
                                   movw->Lower( movw );
                              movw = (movw == window2) ? window1 : window2;
                              wdown = 0;
                              break;
                         case DIBI_MIDDLE:
                              wdown = !wdown;//movw->MoveUp( movw );
                              movw = (movw == window2) ? window1 : window2;
                              break;
                         case DIBI_RIGHT:
                              quit = DIKS_DOWN;
                              break;
                         default:
                              break;
                    }
               }
          }

          if (0) {
               static int wx = 0;
               static int wy = 0;
               static int dirx = 1;//16/3;
               static int diry = 1;//9/3;

               wx += dirx;
               wy += diry;

               if (wx >= SW-768/2  ||  wx <= 0)
                    dirx *= -1;
               if (wy >= SH-576/2  ||  wy <= 0)
                    diry *= -1;

               window2->Move( window2, dirx, diry );
          }
#ifdef VIDEO
          else
               window_surface2->Flip( window_surface2, NULL, 0 );
#endif

          if (movx || movy)
               movw->Move( movw, movx, movy );
          else
               movw->SetOpacity( movw, (((int)(sin( clock()/500000.0 ) * 127) + 127)/4)*4 );

#ifdef VIDEO
//          usleep(20000);
#endif
     }

     bgsurface->Release( bgsurface );

     sleep(2);

     window_surface2->Release( window_surface2 );
     window_surface1->Release( window_surface1 );
     window2->Release( window2 );
     window1->Release( window1 );
     layer->Release( layer );
     dfb->Release( dfb );

     return 42;
}

