#include "config.h"

#include <unistd.h>
#include <stdio.h>

#include "dfb_types.h"

int main()
{
     __u8  byt;
     __u32 pixel32;

     do {
          fread (&pixel32, 4, 1, stdin);

#ifdef WORDS_BIGENDIAN
          pixel32 = (pixel32 << 16) | (pixel32 >> 16);
#endif

          byt = (pixel32 & 0xff0000) >> 16;
          fwrite (&byt, 1, 1, stdout);
          byt = (pixel32 & 0x00ff00) >> 8;
          fwrite (&byt, 1, 1, stdout);
          byt = (pixel32 & 0x0000ff);
          fwrite (&byt, 1, 1, stdout);
     } while (!feof (stdin));

     return 0;
}
