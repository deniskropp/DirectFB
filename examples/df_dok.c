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

#include <time.h>     /* for `clock()'    */
#include <stdio.h>    /* for `fprintf()'  */
#include <stdlib.h>   /* for `rand()'     */
#include <unistd.h>   /* for `sleep()'    */
#include <math.h>     /* for `sqrt()'     */
#include <string.h>   /* for `memset()'   */

/* the super interface */
IDirectFB *dfb;

/* the primary surface (surface of primary layer) */
IDirectFBSurface *primary;

/* pixelformat of the primary surface */
DFBSurfacePixelFormat pixelformat;

/* our "Press any key..." screen */
IDirectFBSurface *intro;

/* some test images for blitting */
IDirectFBSurface *simple;
IDirectFBSurface *colorkeyed;
IDirectFBSurface *image32;
IDirectFBSurface *image32a;

IDirectFBDisplayLayer *layer;
IDirectFBFont *font;
static int fontheight;

/* Media super interface and the provider for our images/font */
IDirectFBImageProvider *provider;

/* Input interfaces: device and its buffer */
IDirectFBInputDevice *keyboard;
IDirectFBInputBuffer *key_events;

/* some defines for benchmark test size and duration */
#define SX 256
#define SY 256
#define SL sqrt(SX*SX+SY*SY)
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define DEMOTIME    3


/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                     \
               err = x;                                                    \
               if (err != DFB_OK) {                                        \
                    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
                    DirectFBErrorFatal( #x, err );                         \
               }

int SW, SH;

int quit = 0;
int with_intro = 0;
int selfrunning = 0;

struct {
     float drawstring;
     float fillrectangle;
     float fillrectangle_blend;
     float filltriangle;
     float filltriangle_blend;
     float drawrectangle;
     float drawrectangle_blend;
     float drawline;
     float drawline_blend;
     float blit;
     float blit_colorkey;
     float blit_32;
     float blit_alpha;
     float stretchblit;
} result;

void shutdown()
{
     /* release our interfaces to shutdown DirectFB */
     intro->Release( intro );
     simple->Release( simple );
     colorkeyed->Release( colorkeyed );
     image32->Release( image32 );
     image32a->Release( image32a );
     primary->Release( primary );
     key_events->Release( key_events );
     keyboard->Release( keyboard );
     layer->Release( layer );
     dfb->Release( dfb );
}

void showMessage( const char *msg )
{
     DFBInputEvent ev;
     int err;

     while (key_events->GetEvent( key_events, &ev ) == DFB_OK) {
          if (ev.type == DIET_KEYPRESS  &&  ev.keycode == DIKC_ESCAPE) {
               shutdown();
               exit( 42 );
          }
     }
     
     if (with_intro) {
          primary->SetBlittingFlags( primary, DSBLIT_NOFX );
          DFBCHECK(primary->Blit( primary, intro, NULL, 0, 0 ));

          primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF );
          DFBCHECK(primary->DrawString( primary, msg, -1, SW/2, SH/2, DSTF_CENTER ));


          if (selfrunning) {
               usleep(1500000);
          }
          else {
               key_events->Reset( key_events );
               key_events->WaitForEvent( key_events );
          }
     }

     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     primary->SetColor( primary, 0, 0, 0, 0 );
     primary->FillRectangle( primary, 0, 0, SW, SH+fontheight );
}

void showResult()
{
     IDirectFBSurface       *meter;
     IDirectFBImageProvider *provider;
     DFBSurfaceDescription   dsc;
     DFBRectangle            dest;
     int   i;
     char  rate[32];
     float factor = (SW-80) / 500.0f;

     if (dfb->CreateImageProvider( dfb, DATADIR"/examples/meter.png", &provider ))
         return;

     provider->GetSurfaceDescription( provider, &dsc );
     dfb->CreateSurface( dfb, &dsc, &meter );
     provider->RenderTo( provider, meter );
     provider->Release ( provider );


     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     primary->SetColor( primary, 0, 0, 0, 0 );
     primary->FillRectangle( primary, 0, 0, SW, SH+fontheight );
     
     primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF );
     primary->DrawString( primary, "Results", -1, SW/2, 10, DSTF_TOPCENTER );

     dest.x = 40; dest.y = 70; dest.h = dsc.height;

     dest.w = (int)( result.drawstring * factor );
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.fillrectangle * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.fillrectangle_blend * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.filltriangle * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.filltriangle_blend * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.drawrectangle * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.drawrectangle_blend * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.drawline * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.drawline_blend * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.blit * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.blit_colorkey * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.blit_32 * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.blit_alpha * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     dest.w = (int)( result.stretchblit * factor ); 
     dest.y += 40;
     primary->StretchBlit( primary, meter, NULL, &dest );

     meter->Release( meter );


     primary->SetDrawingFlags( primary, DSDRAW_BLEND );
     primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0x40 );
     for (i=0; i<13; i++) {
          primary->DrawLine( primary, 40, 89 + 40*i, SW-40, 89 + 40*i );
     }
     
     primary->SetColor( primary, 0xCC, 0xCC, 0xCC, 0xFF );
     primary->DrawString( primary, "Anti-aliased Text", -1, 20, 75, DSTF_LEFT );
     primary->DrawString( primary, "Fill Rectangles", -1, 20, 115, DSTF_LEFT );
     primary->DrawString( primary, "Fill Rectangles (blend)", -1, 20, 155, DSTF_LEFT );
     primary->DrawString( primary, "Fill Triangles", -1, 20, 195, DSTF_LEFT );
     primary->DrawString( primary, "Fill Triangles (blend)", -1, 20, 235, DSTF_LEFT );
     primary->DrawString( primary, "Draw Rectangles", -1, 20, 275, DSTF_LEFT );
     primary->DrawString( primary, "Draw Rectangles (blend)", -1, 20, 315, DSTF_LEFT );
     primary->DrawString( primary, "Draw Lines", -1, 20, 355, DSTF_LEFT );
     primary->DrawString( primary, "Draw Lines (blend)", -1, 20, 395, DSTF_LEFT );
     primary->DrawString( primary, "Blit", -1, 20, 435, DSTF_LEFT );
     primary->DrawString( primary, "Blit colorkeyed", -1, 20, 475, DSTF_LEFT );
     primary->DrawString( primary, "Blit with format conversion", -1, 20, 515, DSTF_LEFT );
     primary->DrawString( primary, "Blit from 32bit (alphachannel blend)", -1, 20, 555, DSTF_LEFT );
     primary->DrawString( primary, "Stretched Blit", -1, 20, 595, DSTF_LEFT );
     
     primary->SetColor( primary, 0xAA, 0xAA, 0xAA, 0xFF );
     sprintf( rate, "%.2f KChar/sec", result.drawstring );
     primary->DrawString( primary, rate, -1, SW-40, 80, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.fillrectangle );
     primary->DrawString( primary, rate, -1, SW-40, 120, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.fillrectangle_blend );
     primary->DrawString( primary, rate, -1, SW-40, 160, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.filltriangle );
     primary->DrawString( primary, rate, -1, SW-40, 200, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.filltriangle_blend );
     primary->DrawString( primary, rate, -1, SW-40, 240, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.drawrectangle );
     primary->DrawString( primary, rate, -1, SW-40, 280, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.drawrectangle_blend );
     primary->DrawString( primary, rate, -1, SW-40, 320, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.drawline );
     primary->DrawString( primary, rate, -1, SW-40, 360, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.drawline_blend );
     primary->DrawString( primary, rate, -1, SW-40, 400, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.blit );
     primary->DrawString( primary, rate, -1, SW-40, 440, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.blit_colorkey );
     primary->DrawString( primary, rate, -1, SW-40, 480, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.blit_32 );
     primary->DrawString( primary, rate, -1, SW-40, 520, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.blit_alpha );
     primary->DrawString( primary, rate, -1, SW-40, 560, DSTF_RIGHT );
     sprintf( rate, "%.2f MPixel/sec", result.stretchblit );
     primary->DrawString( primary, rate, -1, SW-40, 600, DSTF_RIGHT );


     key_events->Reset( key_events );
     key_events->WaitForEvent( key_events );
}

void showStatus( const char *msg )
{
     int err;

     primary->SetColor( primary, 0x40, 0x80, 0xFF, 0xFF );
     DFBCHECK(primary->DrawString( primary, "DirectFB Benchmarking Demo:", -1,
                                   0, SH, DSTF_TOP ));

     primary->SetColor( primary, 0xFF, 0x00, 0x00, 0xFF );
     DFBCHECK(primary->DrawString( primary, msg, -1, SW-1, SH, DSTF_TOPRIGHT ));
}

int main( int argc, char *argv[] )
{
     DFBResult err;
     DFBSurfaceDescription dsc;

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

     DFBCHECK(dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer ));

     /* set our desired video mode */
     DFBCHECK(layer->GetSize( layer, &SW, &SH ));

     /* get the primary surface, i.e. the surface of the primary
        layer we have exclusive access to */
     memset( &dsc, 0, sizeof(DFBSurfaceDescription) );

     dsc.flags = DSDESC_CAPS;
     dsc.caps = DSCAPS_PRIMARY;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
     primary->GetPixelFormat( primary, &pixelformat );

     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = 22;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          DFBCHECK(font->GetHeight( font, &fontheight ));
          DFBCHECK(primary->SetFont( primary, font ));
     }

     primary->FillRectangle( primary, 0, 0, SW, SH );
     primary->SetColor( primary, 0xA0, 0xA0, 0xA0, 0xFF );
     primary->DrawString( primary, "Preparing...", -1, SW/2, SH/2, DSTF_CENTER );

     /* create a surface and render an image to it */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/melted.png",
                                        &provider ));
     DFBCHECK(provider->GetSurfaceDescription( provider, &dsc ));

     dsc.width = SX;
     dsc.height = SY;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &simple ));
     DFBCHECK(provider->RenderTo( provider, simple ));
     provider->Release( provider );

     /* create a surface and render an image to it */
     
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/colorkeyed.png",
                                        &provider ));
     DFBCHECK(provider->GetSurfaceDescription( provider, &dsc ));

     dsc.width = SX;
     dsc.height = SY;
 
     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &colorkeyed ));
     DFBCHECK(provider->RenderTo( provider, colorkeyed ));
     provider->Release( provider );

     /* create a surface and render an image to it */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/pngtest.png",
                                        &provider ));
     DFBCHECK(provider->GetSurfaceDescription( provider, &dsc ));

     dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc.width = SX;
     dsc.height = SY;
     dsc.pixelformat = BYTES_PER_PIXEL(pixelformat) == 2 ? 
                       DSPF_RGB32 : DSPF_RGB16;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &image32 ));
     DFBCHECK(provider->RenderTo( provider, image32 ));
     provider->Release( provider );

     /* create a surface and render an image to it */
     memset( &dsc, 0, sizeof(DFBSurfaceDescription) );

     dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc.width = SX;
     dsc.height = SY;
     dsc.pixelformat = DSPF_ARGB;
     
     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &image32a ));
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/pngtest2.png",
                                        &provider ));
     DFBCHECK(provider->RenderTo( provider, image32a ));
     provider->Release( provider );

     /* create a surface and render an image to it */
     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/intro.png",
                                        &provider ));
     DFBCHECK(provider->GetSurfaceDescription( provider, &dsc ));
     
     dsc.width = SW;
     dsc.height = SH;
     
     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &intro ));
     
     DFBCHECK(provider->RenderTo( provider, intro ));
     provider->Release( provider );

     SH -= fontheight;

     printf( "\nBenchmarking in %dbit mode... (%dbit)\n\n",
             BITS_PER_PIXEL(pixelformat), BYTES_PER_PIXEL(pixelformat)*8 );

     sync();
     sleep(2);

     {
          int i, j;
          unsigned long t, pixels;
          float dt;
          unsigned int stringwidth;

          font->GetStringWidth( font, "DirectX is dead, this is DirectFB", -1,
                                &stringwidth );
          
          
          showMessage( "This is the DirectFB benchmarking tool, "
                       "let's start with some text!" );

          showStatus( "Anti-aliased Text" );
          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, 0xFF );
               primary->DrawString( primary, "DirectX is dead, this is DirectFB", -1,
                                    rand()%(SW-stringwidth),
                                    rand()%(SH-fontheight),
                                    DSTF_TOPLEFT );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.drawstring = 36*(i/dt)/1000.0f;
          printf( "DrawString:             %6.2f secs (%6.2f KChars/sec)\n",
                  dt, result.drawstring );

          
          
          showMessage( "Ok, we'll go on with some opaque filled rectangles!" );

          showStatus( "Rectangle Filling" );
          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, 0xFF );
	       primary->FillRectangle( primary,
				       rand()%(SW-SX), rand()%(SH-SY), SX, SY );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.fillrectangle = SX*SY*(i/dt)/1000000.0f;
          printf( "FillRectangle:          %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.fillrectangle );
          
          
          
          showMessage( "What about alpha blended rectangles?" );

          showStatus( "Alpha Blended Rectangle Filling" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetDrawingFlags( primary, DSDRAW_BLEND );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, rand()%0x64 );
	       primary->FillRectangle( primary,
				       rand()%(SW-SX), rand()%(SH-SY), SX, SY );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.fillrectangle_blend = SX*SY*(i/dt)/1000000.0f;
          printf( "FillRectangle (blend):  %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.fillrectangle_blend );



          showMessage( "Ok, we'll go on with some opaque filled triangles!" );

          showStatus( "Triangle Filling" );
          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               int x = rand()%(SW-SX);
               int y = rand()%(SH-SY);

               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, 0xFF );
	       primary->FillTriangle( primary, x, y, x+SX-1, y+SY/2, x, y+SY-1 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.filltriangle = SX*SY*(i/dt)/1000000.0f / 2.0f;
          printf( "FillTriangle:           %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.filltriangle );
          
          
          
          showMessage( "What about alpha blended triangles?" );

          showStatus( "Alpha Blended Triangle Filling" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetDrawingFlags( primary, DSDRAW_BLEND );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               int x = rand()%(SW-SX);
               int y = rand()%(SH-SY);

               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, rand()%0x64 );
	       primary->FillTriangle( primary, x, y, x+SX-1, y+SY/2, x, y+SY-1 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.filltriangle_blend = SX*SY*(i/dt)/1000000.0f / 2.0f;
          printf( "FillTriangle (blend):   %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.filltriangle_blend );


          
          showMessage( "Now pass over to non filled rectangles!" );

          showStatus( "Rectangle Outlines" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, 0xFF );
               primary->DrawRectangle( primary, rand()%(SW-SX), rand()%(SH-SY),
                                       SX, SY );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.drawrectangle = (SX*2+SY*2-4)*(i/dt)/1000000.0f;
          printf( "DrawRectangle:          %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.drawrectangle );

          
          
          showMessage( "Again, we want it with alpha blending!" );

          showStatus( "Alpha Blended Rectangle Outlines" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetDrawingFlags( primary, DSDRAW_BLEND );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, rand()%0x64 );
               primary->DrawRectangle( primary, rand()%(SW-SX), rand()%(SH-SY),
                                       SX, SY );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.drawrectangle_blend = (SX*2+SY*2-4)*(i/dt)/1000000.0f;
          printf( "DrawRectangle (blend):  %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.drawrectangle_blend );


          
          showMessage( "Can we have some opaque lines, please?" );

          showStatus( "Line Drawing" );

          sync();
          pixels = 0;
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i<i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               int x = rand()%(SW-SX) + SX/2;
               int y = rand()%(SH-SY) + SY/2;
               int dx = rand()%(2*SX) - SX;
               int dy = rand()%(2*SY) - SY;
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, 0xFF );
               primary->DrawLine( primary, 
                                  x - dx/2, y - dy/2, x + dx/2, y + dy/2 );
               pixels += MAX( abs (dx), abs (dy) );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.drawline = (pixels/dt)/1000000.0f;
          printf( "DrawLine:               %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.drawline );

          
          
          showMessage( "So what? Where's the blending?" );

          showStatus( "Alpha Blended Line Drawing" );

          sync();
          pixels = 0;
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetDrawingFlags( primary, DSDRAW_BLEND );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               int x = rand()%(SW-SX) + SX/2;
               int y = rand()%(SH-SY) + SY/2;
               int dx = rand()%(2*SX) - SX;
               int dy = rand()%(2*SY) - SY;
               
               primary->SetColor( primary, rand()%0xFF, rand()%0xFF,
                                  rand()%0xFF, rand()%0x64 );
               primary->DrawLine( primary, 
                                  x - dx/2, y - dy/2, x + dx/2, y + dy/2 );
               pixels += MAX( abs (dx), abs (dy) );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.drawline_blend = (pixels/dt) / 1000000.0f;
          printf( "DrawLine (blend):       %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.drawline_blend );


          
          showMessage( "Now lead to some blitting demos! "
                       "The simplest one comes first..." );

          showStatus( "Simple BitBlt" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->Blit( primary, simple, NULL,
                              (SW!=SX) ? rand()%(SW-SX) : 0,
                              (SH-SY) ? rand()%(SH-SY) : 0 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.blit = SX*SY*(i/dt)/1000000.0f;
          printf( "Blit %dbit:             %6.2f secs (%6.2f MPixel/sec)\n",
                  BYTES_PER_PIXEL(pixelformat)*8, dt, result.blit );

          
          
          showMessage( "Color keying would be nice..." );

          showStatus( "BitBlt with Color Keying" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetBlittingFlags( primary, DSBLIT_SRC_COLORKEY );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->Blit( primary, colorkeyed, NULL,
                              (SW!=SX) ? rand()%(SW-SX) : 0,
                              (SY-SH) ? rand()%(SH-SY) : 0 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.blit_colorkey = SX*SY*(i/dt)/1000000.0f;
          printf( "Blit %dbit (colorkey):  %6.2f secs (%6.2f MPixel/sec)\n",
                  BYTES_PER_PIXEL(pixelformat)*8, dt, result.blit_colorkey );

          
          showMessage( "What if the source surface has another format?" );

          showStatus( "BitBlt with on-the-fly format conversion" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetBlittingFlags( primary, DSBLIT_NOFX );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->Blit( primary, image32, NULL,
                              (SW!=SX) ? rand()%(SW-SX) : 0,
                              (SY-SH) ? rand()%(SH-SY) : 0 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.blit_32 = SX*SY*(i/dt)/1000000.0f;
          printf( "Blit %dbit:             %6.2f secs (%6.2f MPixel/sec)\n",
                  BYTES_PER_PIXEL(pixelformat) == 2 ? 32 : 16, dt, result.blit_32 );

          
          
          showMessage( "Here we go with alpha again!" );

          showStatus( "BitBlt with Alpha Channel" );

          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetBlittingFlags( primary, DSBLIT_BLEND_ALPHACHANNEL );
          for (i=0; i%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); i++) {
               primary->Blit( primary, image32a, NULL,
                              (SW!=SX) ? rand()%(SW-SX) : 0,
                              (SY-SH) ? rand()%(SH-SY) : 0 );
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.blit_alpha = SX*SY*(i/dt)/1000000.0f;
          printf( "Blit 32bit with alpha:  %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.blit_alpha );

          
          
          showMessage( "Stretching!!!!!" );

          showStatus( "Stretched Blit" );

          pixels = 0;
          sync();
          dfb->WaitIdle( dfb );
          t = clock();
          primary->SetBlittingFlags( primary, DSBLIT_NOFX );
          for (j=1; j%100 || clock()<(t+CLOCKS_PER_SEC*DEMOTIME); j++) {
               if (j>SH) {
                    j = 10;
               }
               for (i=10; i<SH; i+=j) {
                    DFBRectangle dr = { SW/2-i/2, SH/2-i/2, i, i };

                    primary->StretchBlit( primary, simple, NULL, &dr );

                    pixels += dr.w * dr.h;
               }
          }
          dfb->WaitIdle( dfb );
          dt = (clock() - t) / (float)CLOCKS_PER_SEC;
          result.stretchblit = (pixels/dt)/1000000.0f;
          printf( "StretchBlit:            %6.2f secs (%6.2f MPixel/sec)\n",
                  dt, result.stretchblit );

          
          
          showResult();
     }

     printf( "\n" );

     shutdown();

     return 0;
}

