#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <linux/c64x.h>

#include <direct/clock.h>
#include <direct/messages.h>
#include <direct/system.h>
#include <direct/util.h>

#define C64X_DEVICE  "/dev/c64x"
#define C64X_DEVICE0 "/dev/c64x0"
#define C64X_QLEN   direct_page_align( sizeof(c64xTaskControl) )
#define C64X_MLEN   direct_page_align( 0x2000000 )

static const char *state_names[] = { "DONE", "ERROR", "TODO", "RUNNING" };

// auto
#define IDLE_MAX    (0)

// manual (examples)
//#define IDLE_MAX    (567087584/10)
//#define IDLE_MAX    (59457217)

int main (int argc, char *argv[])
{
     int              fd;
     void            *mem;
     c64xTaskControl *ctl;
     c64xTask        *queue;
     int              idle_max   = IDLE_MAX;
     uint32_t         idle_last  = 0;
     long long        stamp_last = 0;

     fd = direct_try_open( C64X_DEVICE, C64X_DEVICE0, O_RDONLY, true );
     if (fd < 0)
          return -1;

     ctl = mmap( NULL, C64X_QLEN, PROT_READ, MAP_SHARED, fd, 0 );
     if (ctl == MAP_FAILED) {
          D_PERROR( "C64XDump: Mapping %lu bytes at %lu via '%s' failed!\n", C64X_QLEN, 0UL, C64X_DEVICE );
          close( fd );
          return -2;
     }

     mem = mmap( NULL, C64X_MLEN, PROT_READ, MAP_SHARED, fd, C64X_QLEN );
     if (mem == MAP_FAILED) {
          D_PERROR( "C64XDump: Mapping %lu bytes at %lu via '%s' failed!\n", C64X_MLEN, C64X_QLEN, C64X_DEVICE );
          munmap( (void*)ctl, C64X_QLEN );
          close( fd );
          return -2;
     }

     queue = mem + (0x8fe00000 - 0x8e000000);

     while (1) {
          usleep( 250000 );

          int       loadx   = 1000;
          uint32_t  counter = ctl->idlecounter;
          long long stamp   = direct_clock_get_abs_micros();
          uint32_t  ql_dsp  = ctl->QL_dsp;
          uint32_t  ql_arm  = ctl->QL_arm;
          uint32_t  qh_dsp  = ctl->QH_dsp;
          uint32_t  qh_arm  = ctl->QH_arm;
          uint32_t  task    = queue[ql_dsp & C64X_QUEUE_MASK].c64x_function;
          int       dl, dh;

          dl = ql_arm - ql_dsp;
          if (dl < 0)
               dl += C64X_QUEUE_LENGTH;

          dh = qh_arm - qh_dsp;
          if (dh < 0)
               dh += C64X_QUEUE_LENGTH;

          printf( "\e[H\e[J" );
          printf( "High Q:  arm %5d - dsp %5d = %d\n", qh_arm, qh_dsp, dh );
          printf( "Low  Q:  arm %5d - dsp %5d = %d\n", ql_arm, ql_dsp, dl );

          printf( "                      (%08x: func %d - %s)\n", 
                  task, (task >> 2) & 0x3fff, state_names[task & 3] );

          printf( "Counter: %u\n", counter );

          if (counter >= idle_last && idle_last) {
               long long int cdiff = counter - idle_last;
               long long int tdiff = stamp   - stamp_last;

               long long int  diff = cdiff * 1200000 / tdiff;

#if !IDLE_MAX
               if (diff > idle_max)
                    idle_max = diff;
#endif

               loadx = (idle_max - diff) * 1000 / idle_max;
          }

          if (idle_max)
               printf( "Load: %d.%d%%  (idle_max %d)\n", loadx / 10, loadx % 10, idle_max );

          idle_last  = counter;
          stamp_last = stamp;
     }


     munmap( (void*)mem, C64X_MLEN );
     munmap( (void*)ctl, C64X_QLEN );
     close( fd );

     return 0;
}
