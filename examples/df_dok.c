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

#include <sys/time.h> /* for gettimeofday() */
#include <stdio.h>    /* for fprintf()      */
#include <stdlib.h>   /* for rand()         */
#include <unistd.h>   /* for sleep()        */
#include <string.h>   /* for memset()       */

/* the super interface */
IDirectFB *dfb;

/* the primary surface (surface of primary layer) */
IDirectFBSurface *primary;

/* our "Press any key..." screen */
IDirectFBSurface *intro;

/* some test images for blitting */
IDirectFBSurface *simple;
IDirectFBSurface *colorkeyed;
IDirectFBSurface *image32;
IDirectFBSurface *image32a;

IDirectFBDisplayLayer *layer;
IDirectFBFont         *font;
int stringwidth;
int fontheight;

/* Media super interface and the provider for our images/font */
IDirectFBImageProvider *provider;

/* Input interfaces: event buffer */
IDirectFBInputBuffer *key_events;

int SW, SH;

int with_intro  = 0;
int selfrunning = 0;


/* some defines for benchmark test size and duration */
#define SX 256
#define SY 256
#define MAX(a,b) ((a) > (b) ? (a) : (b))


#define DEMOTIME 3000  /* milliseconds */


/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                     \
               err = x;                                                    \
               if (err != DFB_OK) {                                        \
                    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
                    DirectFBErrorFatal( #x, err );                         \
               }


/* the benchmarks */

static unsigned long long  draw_string          ( long t );
static unsigned long long  fill_rect            ( long t );
static unsigned long long  fill_rect_blend      ( long t );
static unsigned long long  fill_triangle        ( long t );
static unsigned long long  fill_triangle_blend  ( long t );
static unsigned long long  draw_rect            ( long t );
static unsigned long long  draw_rect_blend      ( long t );
static unsigned long long  draw_lines           ( long t );
static unsigned long long  draw_lines_blend     ( long t );
static unsigned long long  blit                 ( long t );
static unsigned long long  blit_colorkeyed      ( long t );
static unsigned long long  blit_convert         ( long t );
static unsigned long long  blit_blend           ( long t );
static unsigned long long  stretch_blit         ( long t );


typedef struct {
     char    * desc;
     char    * message;
     char    * status;
     char    * option;
     int       requested;
     long      result;
     char    * unit;
     unsigned long long (* func) ( long );
} Demo;

static Demo demos[] = {
  { "Anti-aliased Text",
    "This is the DirectFB benchmarking tool, let's start with some text!",
    "Anti-aliased Text", "draw-string",
    0, 0, "KChars/sec",  draw_string },
  { "Fill Rectangles",
    "Ok, we'll go on with some opaque filled rectangles!",
    "Rectangle Filling", "fill-rect",
    0, 0, "MPixel/sec", fill_rect },
  { "Fill Rectangles (blend)",
    "What about alpha blended rectangles?",
    "Alpha Blended Rectangle Filling", "fill-rect-blend",
    0, 0, "MPixel/sec", fill_rect_blend },
  { "Fill Triangles",
    "Ok, we'll go on with some opaque filled triangles!",
    "Triangle Filling", "fill-triangle",
    0, 0, "MPixel/sec", fill_triangle },
  { "Fill Triangles (blend)",
    "What about alpha blended triangles?",
    "Alpha Blended Triangle Filling", "fill-triangle-blend",
    0, 0, "MPixel/sec", fill_triangle_blend },
  { "Draw Rectangles",
    "Now pass over to non filled rectangles!",
    "Rectangle Outlines", "draw-rect",
    0, 0, "MPixel/sec", draw_rect },
  { "Draw Rectangles (blend)",
    "Again, we want it with alpha blending!",
    "Alpha Blended Rectangle Outlines", "draw-rect-blend",
    0, 0, "MPixel/sec", draw_rect_blend },
  { "Draw Lines",
    "Can we have some opaque lines, please?",
    "Line Drawing", "draw-line",
    0, 0, "MPixel/sec", draw_lines },
  { "Draw Lines (blend)",
    "So what? Where's the blending?",
    "Alpha Blended Line Drawing", "draw-line-blend",
    0, 0, "MPixel/sec", draw_lines_blend },
  { "Blit",
    "Now lead to some blitting demos! The simplest one comes first...",
    "Simple BitBlt", "blit",
    0, 0, "MPixel/sec", blit },
  { "Blit colorkeyed",
    "Color keying would be nice...",
    "BitBlt with Color Keying", "blit-colorkeyed",
    0, 0, "MPixel/sec", blit_colorkeyed },
  { "Blit with format conversion",
    "What if the source surface has another format?",
    "BitBlt with on-the-fly format conversion", "blit-convert",
    0, 0, "MPixel/sec", blit_convert },
  { "Blit from 32bit (alphachannel blend)",
    "Here we go with alpha again!",
    "BitBlt with Alpha Channel", "blit-blend",
    0, 0, "MPixel/sec", blit_blend },
  { "Stretch Blit",
    "Stretching!!!!!",
    "Stretch Blit", "stretch-blit",
    0, 0, "MPixel/sec", stretch_blit }
};
static int num_demos = sizeof( demos ) / sizeof (demos[0]);


static unsigned int rand_pool = 0x12345678;

static inline unsigned int myrand()
{
     rand_pool ^= ((rand_pool << 7) | (rand_pool >> 25));
     rand_pool += 0x87654321;

     return rand_pool;
}


static inline long myclock()
{
  struct timeval tv;

  gettimeofday (&tv, NULL);
  return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void print_usage()
{
     int i;

     printf ("DirectFB Benchmarking Demo\n\n");
     printf ("Usage: df_dok [options]\n\n");
     printf ("Options:\n");
     for (i = 0; i < num_demos; i++) {
          printf ("  --%-20s %s\n", demos[i].option, demos[i].desc);
     }
     printf ("\nIf no options are given, all benchmarks are run.\n");
}

static void shutdown()
{
     /* release our interfaces to shutdown DirectFB */
     font->Release( font );
     intro->Release( intro );
     simple->Release( simple );
     colorkeyed->Release( colorkeyed );
     image32->Release( image32 );
     image32a->Release( image32a );
     primary->Release( primary );
     key_events->Release( key_events );
     layer->Release( layer );
     dfb->Release( dfb );
}

static void showMessage( const char *msg )
{
     DFBInputEvent ev;
     int err;

     while (key_events->GetEvent( key_events, &ev ) == DFB_OK) {
          if (ev.type == DIET_KEYPRESS) {
               switch (ev.keycode) {
                    case DIKC_ESCAPE:
                    case DIKC_Q:
                    case DIKC_BACK:
                    case DIKC_STOP:
                         shutdown();
                         exit( 42 );
                         break;
                    default:
                         break;
               }
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

static void showResult()
{
     IDirectFBSurface       *meter;
     IDirectFBImageProvider *provider;
     DFBSurfaceDescription   dsc;
     DFBRectangle            dest;
     int   i, y;
     char  rate[32];
     char  format[32];
     double factor = (SW-80) / 500000.0;

     if (dfb->CreateImageProvider( dfb,
                                   DATADIR"/examples/meter.png", &provider ))
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
     primary->SetColor( primary, 0x66, 0x66, 0x66, 0xFF );

     for (i = 0; i < num_demos; i++) {
          if (!demos[i].requested)
               continue;

          dest.w = (double) demos[i].result * factor;
          primary->StretchBlit( primary, meter, NULL, &dest );
          if (dest.w < SW-80)
               primary->DrawLine( primary,
                                  40 + dest.w, dest.y + 18,
                                  SW-40, dest.y + 18 );
          dest.y += 38;
     }

     meter->Release( meter );

     y = 75;
     for (i = 0; i < num_demos; i++) {
          if (!demos[i].requested)
               continue;

          primary->SetColor( primary, 0xCC, 0xCC, 0xCC, 0xFF );
          primary->DrawString( primary, demos[i].desc, -1, 20, y, DSTF_LEFT );

          sprintf( format, "%%.2f %s", demos[i].unit );
          sprintf( rate, format, (double) demos[i].result / 1000.0);
          primary->SetColor( primary, 0xAA, 0xAA, 0xAA, 0xFF );
          primary->DrawString( primary, rate, -1, SW-40, y + 5, DSTF_RIGHT );

          y += 38;
     }

     key_events->Reset( key_events );
     key_events->WaitForEvent( key_events );
}

static void showStatus( const char *msg )
{
     int err;

     primary->SetColor( primary, 0x40, 0x80, 0xFF, 0xFF );
     DFBCHECK(primary->DrawString( primary, "DirectFB Benchmarking Demo:", -1,
                                   0, SH, DSTF_TOP ));

     primary->SetColor( primary, 0xFF, 0x00, 0x00, 0xFF );
     DFBCHECK(primary->DrawString( primary, msg, -1, SW-1, SH, DSTF_TOPRIGHT ));
}

static unsigned long long draw_string( long t )
{
     long i;

     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          primary->SetColor( primary, myrand()%0xFF, myrand()%0xFF,
                             myrand()%0xFF, 0xFF );
          primary->DrawString( primary,
                               "DirectX is dead, this is DirectFB", -1,
                               myrand()%(SW-stringwidth),
                               myrand()%(SH-fontheight),
                               DSTF_TOPLEFT );
     }
     return 36*(unsigned long long)i*1000;
}

static unsigned long long fill_rect( long t )
{
     long i;

     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF, 0xFF );
          primary->FillRectangle( primary,
                                  myrand()%(SW-SX), myrand()%(SH-SY), SX, SY );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long fill_rect_blend( long t )
{
     long i;

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF,
                             myrand()%0x64 );
          primary->FillRectangle( primary,
                                  myrand()%(SW-SX), myrand()%(SH-SY), SX, SY );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long fill_triangle( long t )
{
     long i, x, y;

     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          x = myrand()%(SW-SX);
          y = myrand()%(SH-SY);

          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF, 0xFF );
          primary->FillTriangle( primary, x, y, x+SX-1, y+SY/2, x, y+SY-1 );
     }
     return SX*SY*(unsigned long long)i/2;
}

static unsigned long long fill_triangle_blend( long t )
{
     long i, x, y;

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          x = myrand()%(SW-SX);
          y = myrand()%(SH-SY);

          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF,
                             myrand()%0x64 );
          primary->FillTriangle( primary, x, y, x+SX-1, y+SY/2, x, y+SY-1 );
     }
     return SX*SY*(unsigned long long)i/2;
}

static unsigned long long draw_rect( long t )
{
     long i;

     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF, 0xFF );
          primary->DrawRectangle( primary,
                                  myrand()%(SW-SX), myrand()%(SH-SY), SX, SY );
     }
     return (SX*2+SY*2-4)*(unsigned long long)i;
}

static unsigned long long draw_rect_blend( long t )
{
     long i;

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF,
                             myrand()%0x64 );
          primary->DrawRectangle( primary,
                                  myrand()%(SW-SX), myrand()%(SH-SY), SX, SY );
     }
     return (SX*2+SY*2-4)*(unsigned long long)i;
}

static unsigned long long draw_lines( long t )
{
     long i, l, x, y, dx, dy;
     unsigned long long pixels = 0;
     DFBRegion lines[10];

     primary->SetDrawingFlags( primary, DSDRAW_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {

          for (l=0; l<10; l++) {
               x = myrand()%(SW-SX) + SX/2;
               y = myrand()%(SH-SY) + SY/2;
               dx = myrand()%(2*SX) - SX;
               dy = myrand()%(2*SY) - SY;
               pixels += MAX( abs (dx), abs (dy) );

               lines[l].x1 = x - dx/2;
               lines[l].y1 = y - dy/2;
               lines[l].x2 = x + dx/2;
               lines[l].y2 = y + dy/2;
          }

          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF, 0xFF );
          primary->DrawLines( primary, lines, 10 );
     }
     return pixels;
}

static unsigned long long draw_lines_blend( long t )
{
     long i, l, x, y, dx, dy;
     unsigned long long pixels = 0;
     DFBRegion lines[10];

     primary->SetDrawingFlags( primary, DSDRAW_BLEND );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {

          for (l=0; l<10; l++) {
               x = myrand()%(SW-SX) + SX/2;
               y = myrand()%(SH-SY) + SY/2;
               dx = myrand()%(2*SX) - SX;
               dy = myrand()%(2*SY) - SY;
               pixels += MAX( abs (dx), abs (dy) );

               lines[l].x1 = x - dx/2;
               lines[l].y1 = y - dy/2;
               lines[l].x2 = x + dx/2;
               lines[l].y2 = y + dy/2;
          }

          primary->SetColor( primary,
                             myrand()%0xFF, myrand()%0xFF, myrand()%0xFF,
                             myrand()%0x64 );
          primary->DrawLines( primary, lines, 10 );
     }
     return pixels;
}

static unsigned long long blit( long t )
{
     long i;

     primary->SetBlittingFlags( primary, DSBLIT_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
       primary->Blit( primary, simple, NULL,
                      (SW!=SX) ? myrand()%(SW-SX) : 0,
                      (SH-SY) ? myrand()%(SH-SY) : 0 );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long blit_colorkeyed( long t )
{
     long i;

     primary->SetSrcColorKey( primary, 0);
     primary->SetBlittingFlags( primary, DSBLIT_SRC_COLORKEY );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
       primary->Blit( primary, colorkeyed, NULL,
                      (SW!=SX) ? myrand()%(SW-SX) : 0,
                      (SY-SH)  ? myrand()%(SH-SY) : 0 );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long blit_convert( long t )
{
     long i;

     primary->SetBlittingFlags( primary, DSBLIT_NOFX );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
       primary->Blit( primary, image32, NULL,
                      (SW!=SX) ? myrand()%(SW-SX) : 0,
                      (SY-SH) ? myrand()%(SH-SY) : 0 );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long blit_blend( long t )
{
     long i;

     primary->SetBlittingFlags( primary, DSBLIT_BLEND_ALPHACHANNEL );
     for (i=0; i%100 || myclock()<(t+DEMOTIME); i++) {
       primary->Blit( primary, image32a, NULL,
                      (SW!=SX) ? myrand()%(SW-SX) : 0,
                      (SY-SH)  ? myrand()%(SH-SY) : 0 );
     }
     return SX*SY*(unsigned long long)i;
}

static unsigned long long stretch_blit( long t )
{
     long i, j;
     unsigned long long pixels = 0;

     primary->SetBlittingFlags( primary, DSBLIT_NOFX );
     for (j=1; myclock()<(t+DEMOTIME); j++) {
          if (j>SH) {
               j = 10;
          }
          for (i=10; i<SH; i+=j) {
            DFBRectangle dr = { SW/2-i/2, SH/2-i/2, i, i };

            primary->StretchBlit( primary, simple, NULL, &dr );

            pixels += dr.w * dr.h;
          }
     }
     return pixels;
}

int main( int argc, char *argv[] )
{
     DFBResult err;
     DFBSurfacePixelFormat pixelformat;
     DFBSurfaceDescription dsc;
     int i;

     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* parse command line */
     if (argc <= 1) {
          for (i = 0; i < num_demos; i++) {
               demos[i].requested = 1;
          }
     }
     while (--argc > 0) {
          if (strncmp (argv[argc], "--", 2) == 0) {
               for (i = 0; i < num_demos; i++) {
                    if (strcmp (argv[argc] + 2, demos[i].option) == 0) {
                         demos[i].requested = 1;
                         break;
                    }
               }
               if (i == num_demos) {
                    print_usage();
                    return 1;
               }
          }
          else {
               print_usage();
               return 1;
          }
     }

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* create an input buffer for key events */
     DFBCHECK(dfb->CreateInputBuffer( dfb, DICAPS_KEYS, &key_events ));

     /* set our cooperative level to DFSCL_FULLSCREEN
        for exclusive access to the primary layer */
     err = dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
     if (err)
          DirectFBError( "Failed to get exclusive access", err );

     DFBCHECK(dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer ));

     /* get the primary surface, i.e. the surface of the primary
        layer we have exclusive access to */
     dsc.flags = DSDESC_CAPS;
     dsc.caps = DSCAPS_PRIMARY;

     DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
     primary->GetPixelFormat( primary, &pixelformat );
     primary->GetSize( primary, &SW, &SH );

     {
          DFBFontDescription desc;

          desc.flags = DFDESC_HEIGHT;
          desc.height = 22;

          DFBCHECK(dfb->CreateFont( dfb, FONT, &desc, &font ));
          font->GetHeight( font, &fontheight );
          primary->SetFont( primary, font );
          font->GetStringWidth( font, "DirectX is dead, this is DirectFB", -1,
                                &stringwidth );
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

     for (i = 0; i < num_demos; i++) {
           long t, dt;
           unsigned long long pixels;

           if (!demos[i].requested)
                continue;

           showMessage( demos[i].message );
           showStatus( demos[i].status );

           sync();
           dfb->WaitIdle( dfb );
           t = myclock();
           pixels = (* demos[i].func)(t);
           dfb->WaitIdle( dfb );
           dt = myclock() - t;
           demos[i].result = pixels / (unsigned long long)dt;
           printf( "%-36s %6.2f secs (%8.2f %s)\n",
                   demos[i].desc,
                   dt / 1000.0, demos[i].result / 1000.0, demos[i].unit);
     }

     showResult();

     printf( "\n" );

     shutdown();

     return 0;
}

