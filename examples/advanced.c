#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fusionsound.h>

typedef struct fmtChunk {
     __u16     encoding;
     __u16     channels;		/* 1 = mono, 2 = stereo */
     __u32     frequency;		/* One of 11025, 22050, or 44100 Hz */
     __u32     byterate;		/* Average bytes per second */
     __u16     blockalign;		/* Bytes per sample block */
     __u16     bitspersample;		/* One of 8, 12, 16, or 4 for ADPCM */
} fmtChunk;

#ifdef WORDS_BIGENDIAN

#define SWAP16(a) (((a)>>8) | ((a) << 8))
#define SWAP32(a) (((a) >> 24) | (((a) & 0x00ff0000) >>  8) | (((a) & 0x0000ff00) <<  8) | (((a) << 24)))

static void fixup_fmtchunk(struct fmtChunk *fmtchunk)
{
     fmtchunk->encoding = SWAP16(fmtchunk->encoding);
     fmtchunk->channels = SWAP16(fmtchunk->channels);
     fmtchunk->frequency = SWAP32(fmtchunk->frequency);
     fmtchunk->byterate = SWAP32(fmtchunk->byterate);
     fmtchunk->blockalign = SWAP16(fmtchunk->blockalign);
     fmtchunk->bitspersample = SWAP16(fmtchunk->bitspersample);
}

static void fixup_sampledata(__u16 *data, int len)
{
    len/=2;
    while (len--) {
         __u16 tmp = *data;
         *data++ = SWAP16(tmp);
    }
}
#endif

static DFBResult
read_file_header (int fd)
{
     char buf[12];

     if (read (fd, buf, 12) < 12) {
          fprintf (stderr, "Could not read at least 12 bytes!\n");
          return DFB_IO;
     }

     if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F') {
          fprintf (stderr, "No RIFF header found!\n");
          return DFB_UNSUPPORTED;
     }

     if (buf[8] != 'W' || buf[9] != 'A' || buf[10] != 'V' || buf[11] != 'E') {
          fprintf (stderr, "Not a WAVE!\n");
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static int
read_chunk_header (int fd, char *magic)
{
     unsigned char buf[8];

     if (read (fd, buf, 8) < 8) {
          fprintf (stderr, "Could not read 8 bytes of chunk header!\n");
          return -1;
     }

     strncpy (magic, buf, 4);

     return buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
}

static IFusionSoundBuffer *
load_sample (IFusionSound *sound, const char *filename)
{
     DFBResult            ret;
     int                  fd;
     FSBufferDescription  desc;
     IFusionSoundBuffer  *buffer;
     void                *data;
     fmtChunk             fmt;
     int                  len;

     fd = open (filename, O_RDONLY);
     if (fd < 0) {
          perror (filename);
          return NULL;
     }

     if (read_file_header (fd)) {
          close (fd);
          return NULL;
     }

     while (DFB_TRUE) {
          char magic[4];

          len = read_chunk_header (fd, magic);
          if (len <= 0) {
               fprintf (stderr, "Could not find format chunk!\n");
               close (fd);
               return NULL;
          }

          if (magic[0] == 'f' || magic[1] == 'm' || magic[2] == 't') {
               if (len < sizeof(fmtChunk)) {
                    fprintf (stderr, "Format chunk has invalid size (%d/%d)!\n",
                             len, sizeof(fmtChunk));
                    close (fd);
                    return NULL;
               }

               if (read (fd, &fmt, len) < len) {
                    fprintf (stderr, "Could not read format chunk!\n");
                    close (fd);
                    return NULL;
               }

               if (lseek (fd, len - sizeof(fmtChunk), SEEK_CUR) == (off_t) -1) {
                    fprintf (stderr, "Could not seek past chunk!\n");
                    close (fd);
                    return NULL;
               }
               
               break;
          }
          else {
               if (lseek (fd, len, SEEK_CUR) == (off_t) -1) {
                    fprintf (stderr, "Could not seek past chunk!\n");
                    close (fd);
                    return NULL;
               }
          }
     }

#ifdef WORDS_BIGENDIAN
     fixup_fmtchunk( &fmt );
#endif

     if (fmt.encoding != 1) {
          fprintf (stderr, "Only PCM supported, yet!\n");
          close (fd);
          return NULL;
     }

     if (fmt.bitspersample != 16 && fmt.bitspersample != 8) {
          fprintf (stderr, "Only 16 or 8 bit supported, yet!\n");
          close (fd);
          return NULL;
     }
     

     desc.flags        = FSBDF_LENGTH | FSBDF_CHANNELS |
                         FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE;
     desc.channels     = fmt.channels;
     desc.sampleformat = (fmt.bitspersample == 8) ? FSSF_U8 : FSSF_S16;
     desc.samplerate   = fmt.frequency;
     
     while (DFB_TRUE) {
          char magic[4];
          
          len = read_chunk_header (fd, magic);
          if (len <= 0) {
               fprintf (stderr, "Could not find data chunk!\n");
               close (fd);
               return NULL;
          }
               
          if (magic[0] == 'd' && magic[1] == 'a' &&
              magic[2] == 't' && magic[3] == 'a')
          {
               desc.length = len / fmt.blockalign;
               break;
          }
          else {
               if (lseek (fd, len, SEEK_CUR) == (off_t) -1) {
                    fprintf (stderr, "Could not seek past chunk!\n");
                    close (fd);
                    return NULL;
               }
          }
     }
     
     ret = sound->CreateBuffer (sound, &desc, &buffer);
     if (ret) {
          DirectFBError ("IFusionSound::CreateBuffer", ret);
          close (fd);
          return NULL;
     }

     buffer->Lock (buffer, &data);
     
     if (read (fd, data, len) < len)
          fprintf (stderr, "Warning: Could not read all data bytes!\n");

#ifdef WORDS_BIGENDIAN
     if (fmt.bitspersample == 16)
          fixup_sampledata (data, len);
#endif

     close (fd);

     buffer->Unlock (buffer);

     return buffer;
}


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
     
     for (i=0; i<500; i++) {
          TEST(playback->SetPitch (playback, i*i/25000.0f ));
          
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

