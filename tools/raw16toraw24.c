#include "config.h"

#include <unistd.h>
#include <stdio.h>

int main()
{
     unsigned char  byt;
     unsigned short wrd;

     do {
          fread (&wrd, 2, 1, stdin);

#ifdef WORDS_BIGENDIAN
          swab (&wrd, &wrd, 2);
#endif

          byt = (wrd & 0xf800) >> 8;
          fwrite (&byt, 1, 1, stdout);
          byt = (wrd & 0x07E0) >> 3;
          fwrite (&byt, 1, 1, stdout);
          byt = (wrd & 0x001F) << 3;
          fwrite (&byt, 1, 1, stdout);
     } while (!feof (stdin));

     return 0;
}
