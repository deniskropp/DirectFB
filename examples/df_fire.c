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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

IDirectFB             *dfb;
IDirectFBSurface      *primary;
IDirectFBInputDevice  *keyboard;
IDirectFBInputBuffer  *keybuffer;

typedef struct
{
     int               width;
     int               height;
     IDirectFBSurface *surface;
} Fire;

Fire *fire_new( IDirectFBSurface *surface )
{
     Fire *fire = malloc( sizeof(Fire) );

     surface->GetSize( surface, &fire->width, &fire->height );

     surface->SetDrawingFlags( surface, DSDRAW_NOFX );
     surface->SetColor( surface, 0, 0, 0, 0 );
     surface->FillRectangle( surface, 0, 0, fire->width, fire->height );

     fire->surface = surface;

     return fire;
}

void fire_do_frame( Fire *fire )
{
     void     *data;
     int       pitch;
     int       w;
     int       y = fire->height - 1;
     DFBRegion region = { 0, 0, fire->width - 1, fire->height - 2 };

     if (fire->surface->Lock( fire->surface, DSLF_READ | DSLF_WRITE,
                              &data, &pitch ))
          return;

     while (y--) {
          __u16 *d = data + 2;
          __u16 *s = data + pitch;

          w = fire->width - 2;

          while (w--) {
               __u16 red, green, blue;

               red = ((((*d   & 0xF800) >> 11) +
                       ((s[0] & 0xF800) >> 11) +
                       ((s[1] & 0xF800) >> 11) +
                       ((s[2] & 0xF800) >> 11) + rand()%3 - 1) >> 2) << 11;
               green = ((((*d   & 0x07E0) >> 5) +
                         ((s[0] & 0x07E0) >> 5) +
                         ((s[1] & 0x07E0) >> 5) +
                         ((s[2] & 0x07E0) >> 5)) >> 2) << 5;
               blue = ((*d   & 0x001F) +
                       (s[0] & 0x001F) +
                       (s[1] & 0x001F) +
                       (s[2] & 0x001F)) >> 2;

               if (red == 0xF800) red = 0;

               *d++ = red | green | blue;
               s++;
          }

          data += pitch;
     }

     w = fire->width;

     while (w--) {
          *((__u16*)data)++ = (((rand()%0x18)+0x8) << 11) | (rand()%0x18 << 5) | rand()%0x8;
     }

     if (fire->surface->Unlock( fire->surface ) == DFB_OK)
          fire->surface->Flip( fire->surface, &region, DSFLIP_BLIT );
}

void fire_destroy( Fire *fire )
{
     free( fire );
}

int main( int argc, char *argv[] )
{
     DFBResult err;
     DFBInputDeviceKeyState quit = DIKS_UP;
     Fire *fire;

     srand( (long)time( 0 ) );

     if (DirectFBInit( &argc, &argv ) != DFB_OK)
          return 1;

     /* We want the fire window in system memory because we do
        many reads on its content. */
     DirectFBSetOption( "window-surface-policy", "systemonly" );

     if (DirectFBCreate( &dfb ) != DFB_OK)
          return 1;

     err = dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard );
     if (err)
          DirectFBErrorFatal( "CreateInputDevice for keyboard failed", err );

     err = keyboard->CreateInputBuffer( keyboard, &keybuffer );
     if (err)
          DirectFBErrorFatal( "CreateInputBuffer for keyboard failed", err );

     
     {
          IDirectFBDisplayLayer *layer;
          DFBDisplayLayerConfig config;

          dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
          layer->EnableCursor( layer, 1 );
          
          config.flags = DLCONF_PIXELFORMAT;
          config.pixelformat = DSPF_RGB16;

          err = layer->SetConfiguration( layer, &config );
          if (err)
               DirectFBErrorFatal( "Could not set 16bit video mode", err );

          layer->Release( layer );
     }

     {
          DFBSurfaceDescription dsc;

          dsc.flags = DSDESC_CAPS;
          dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

          err = dfb->CreateSurface( dfb, &dsc, &primary );
          if (err != DFB_OK) {
               DirectFBError( "Failed creating primary surface", err );
               keyboard->Release( keyboard );
               dfb->Release( dfb );
               return 1;
          }
     }

     fire = fire_new( primary );

     while (quit != DIKS_DOWN) {
          fire_do_frame( fire );
          keyboard->GetKeyState( keyboard, DIKC_ESCAPE, &quit );
     }

     fire_destroy( fire );

     primary->Release( primary );
     keyboard->Release( keyboard );
     dfb->Release( dfb );

     return 42;
}

