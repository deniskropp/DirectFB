#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fusionsound.h>

#include "loader.h"


static IFusionSoundPlayback *
prepare_test( IFusionSoundBuffer *buffer,
              const char         *name )
{
     DFBResult             ret;
     IFusionSoundPlayback *playback;
     
     ret = buffer->CreatePlayback (buffer, &playback);
     if (ret) {
          DirectFBError ("IFusionSoundBuffer::GetPlayback", ret);
          return NULL;
     }

     sleep( 1 );

     fprintf( stderr, "Testing %-30s   ", name );

     return playback;
}

#define BEGIN_TEST(name)                                              \
     IFusionSoundPlayback *playback;                                  \
                                                                      \
     playback = prepare_test( buffer, name );                         \
     if (!playback)                                                   \
          return
          
#define END_TEST()                                                    \
     do {                                                             \
          fprintf( stderr, "OK\n" );                                  \
          playback->Release (playback);                               \
     } while (0)

#define TEST(x...)                                                    \
     do {                                                             \
          DFBResult ret = (x);                                        \
          if (ret) {                                                  \
               fprintf( stderr, "FAILED!\n\n" );                      \
               DirectFBError (#x, ret);                               \
               playback->Release (playback);                          \
               return;                                                \
          }                                                           \
     } while (0)

void
test_simple_playback (IFusionSoundBuffer *buffer)
{
     BEGIN_TEST( "Simple Playback" );

     TEST(playback->Start (playback, 0, 0));
     
     TEST(playback->Wait (playback));

     END_TEST();
}

void
test_positioned_playback (IFusionSoundBuffer *buffer)
{
     FSBufferDescription desc;

     BEGIN_TEST( "Positioned Playback" );

     TEST(buffer->GetDescription (buffer, &desc));
     
     TEST(playback->Start (playback, desc.length * 1/3, desc.length * 1/4));
     
     TEST(playback->Wait (playback));

     END_TEST();
}

void
test_looping_playback (IFusionSoundBuffer *buffer)
{
     BEGIN_TEST( "Looping Playback" );

     TEST(playback->Start (playback, 0, -1));
     
     sleep (5);
     
     TEST(playback->Stop (playback));

     END_TEST();
}

void
test_stop_continue_playback (IFusionSoundBuffer *buffer)
{
     int i;

     BEGIN_TEST( "Stop/Continue Playback" );

     TEST(playback->Start (playback, 0, -1));
     
     for (i=0; i<5; i++) {
          usleep (500000);

          TEST(playback->Stop (playback));
          
          usleep (200000);

          TEST(playback->Continue (playback));
     }

     END_TEST();
}

void
test_volume_level (IFusionSoundBuffer *buffer)
{
     int i;

     BEGIN_TEST( "Volume Level" );

     TEST(playback->Start (playback, 0, 0));
     
     for (i=0; i<150; i++) {
          TEST(playback->SetVolume (playback, sin (i/3.0) / 3.0f + 0.6f ));
          
          usleep (20000);
     }
     
     END_TEST();
}

void
test_pan_value (IFusionSoundBuffer *buffer)
{
     int i;

     BEGIN_TEST( "Pan Value" );

     TEST(playback->Start (playback, 0, 0));
     
     for (i=0; i<150; i++) {
          TEST(playback->SetPan (playback, sin (i/3.0)));
          
          usleep (20000);
     }
     
     END_TEST();
}

void
test_pitch_value (IFusionSoundBuffer *buffer)
{
     int i;

     BEGIN_TEST( "Pitch Value" );

     TEST(playback->Start (playback, 0, -1));
     
     for (i=500; i<1500; i++) {
          TEST(playback->SetPitch (playback, i/1000.0f));
          
          usleep (20000);
     }
     
     END_TEST();
}

static void
do_playback_tests (IFusionSoundBuffer *buffer)
{
     fprintf( stderr, "\nRunning tests...\n\n" );
     
     test_simple_playback( buffer );
     test_positioned_playback( buffer );
     test_looping_playback( buffer );
     test_stop_continue_playback( buffer );
     test_volume_level( buffer );
     test_pan_value( buffer );
     test_pitch_value( buffer );
}

int main (int argc, char *argv[])
{
     DFBResult           ret;
     IDirectFB          *dfb;
     IFusionSound       *sound;
     IFusionSoundBuffer *buffer;

     ret = DirectFBInit (&argc, &argv);
     if (ret)
          DirectFBErrorFatal ("DirectFBInit", ret);

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     ret = DirectFBCreate (&dfb);
     if (ret)
          DirectFBErrorFatal ("DirectFBCreate", ret);

     ret = dfb->GetInterface (dfb, "IFusionSound", NULL, NULL, (void**) &sound);
     if (ret)
          DirectFBErrorFatal ("IDirectFB::GetInterface", ret);

     buffer = load_sample (sound, argv[1]);
     if (buffer) {
          do_playback_tests (buffer);
          
          buffer->Release (buffer);
     }

     sound->Release (sound);
     dfb->Release (dfb);

     return 0;
}

