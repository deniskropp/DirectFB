#include <stdio.h>
#include <math.h>
#include <libgen.h>
#include <pthread.h>

#include <lite/label.h>
#include <lite/lite.h>
#include <lite/slider.h>
#include <lite/button.h>
#include <lite/window.h>

#include <fusionsound.h>

#include "loader.h"


static float values[4] = { 0.25f, 0.5f, 0.5f, 0 };

static const char *channels[4] = { "Pitch", "Volume", "Pan", "Position" };

static IFusionSound         *sound;
static IFusionSoundBuffer   *buffer;
static IFusionSoundPlayback *playback;
static int                   sample_length;


static DFBResult
create_playback( const char *filename )
{
     DFBResult ret;

     ret = FusionSoundCreate( &sound );
     if (ret) {
          DirectFBError( "FusionSoundCreate() failed", ret );
          return ret;
     }

     buffer = load_sample( sound, filename );
     if (buffer) {
          FSBufferDescription desc;

          buffer->GetDescription( buffer, &desc );

          sample_length = desc.length;

          ret = buffer->CreatePlayback( buffer, &playback );
          if (ret) {
               DirectFBError( "CreatePlayback() failed", ret );

               buffer->Release( buffer );
          }
          else {
               playback->Start( playback, 0, -1 );
               return DFB_OK;
          }
     }

     sound->Release( sound );

     return DFB_FAILURE;
}

static void
destroy_playback()
{
     playback->Release( playback );
     buffer->Release( buffer );
     sound->Release( sound );
}

static void
slider_update( LiteSlider *slider, float pos, void *ctx )
{
     int i = (int) ctx;

     values[i] = pos;

     switch (i) {
          case 0:
               playback->SetPitch( playback, pos * 4.0f );
               break;
          case 1:
               playback->SetVolume( playback, pos * 2.0f );
               break;
          case 2:
               playback->SetPan( playback, pos * 2.0f - 1.0f );
               break;
          case 3:
               playback->Start( playback, pos * sample_length, -1 );
               break;
          default:
               break;
     }
}

static void
button_pressed( LiteButton *button, void *ctx )
{
     static DFBBoolean stopped;

     if (stopped) {
          playback->Continue( playback );

          stopped = DFB_FALSE;
     }
     else {
          playback->Stop( playback );

          stopped = DFB_TRUE;
     }
}

int
main (int argc, char *argv[])
{
     int         i;
     DFBResult   ret;
     LiteLabel  *label[4];
     LiteSlider *slider[4];
     LiteButton *playbutton;
     LiteWindow *window;

     ret = DirectFBInit( &argc, &argv );
     if (ret)
          DirectFBErrorFatal( "DirectFBInit() failed", ret );

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          DirectFBErrorFatal( "FusionSoundInit() failed", ret );

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     /* initialize */
     if (lite_open( &argc, &argv ))
          return 1;

     /* init sound */
     if (create_playback( argv[1] )) {
          lite_close();
          return 2;
     }

     /* create a window */
     ret = lite_new_window( &window,
                             NULL,
                             LITE_CENTER_HORIZONTALLY,
                             LITE_CENTER_VERTICALLY,
                             330, 170,
                             DWCAPS_ALPHACHANNEL, basename(argv[1]) );

     /* setup the labels */
     for (i=0; i<4; i++) {
          ret = lite_new_label( &label[i], LITE_BOX(window),
                                     10, 10 + i * 25, 85, 18 );

          lite_set_label_text( label[i], channels[i] );
     }

     /* setup the sliders */
     for (i=0; i<4; i++) {
          ret = lite_new_slider( &slider[i], LITE_BOX(window),
                                       100, 10 + i * 25, 220, 20 );

          lite_set_slider_pos( slider[i], values[i] );

          lite_on_slider_update( slider[i], slider_update, (void*) i );
     }

     /* setup the play/pause button */
     playbutton = lite_new_button( LITE_BOX(window), 150, 110, 50, 50 );
     lite_set_button_image( playbutton, BS_NORMAL, "stop.png" );
     lite_set_button_image( playbutton, BS_DISABLED, "stop_disabled.png" );
     lite_set_button_image( playbutton, BS_HILITE, "stop_highlighted.png" );
     lite_set_button_image( playbutton, BS_PRESSED, "stop_pressed.png" );
     lite_on_button_press( playbutton, button_pressed, window );

     /* show the window */
     lite_set_window_opacity( window, 0xff );

     /* run the event loop with a timeout */
     while (lite_window_event_loop( window, 20 ) == DFB_TIMEOUT) {
          int position = 0;

          playback->GetStatus( playback, NULL, &position );

          lite_set_slider_pos( slider[3], position / (float) sample_length );
     }

     /* destroy the window with all this children and resources */
     lite_destroy_window( window );

     /* deinit sound */
     destroy_playback();

     /* deinitialize */
     lite_close();

     return 0;
}
