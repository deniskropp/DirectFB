#include <endian.h>
#include <unistd.h>
#include <stdio.h>

#include <asm/types.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#include <asm/byteorder.h>
#endif

int main()
{
     __u8  byt;
     __u32 pixel32;
     
     do {
          fread( &pixel32, 4, 1, stdin );
          
#if __BYTE_ORDER == __BIG_ENDIAN
          __swab32( pixel32 );
#endif
          
          byt = (pixel32 & 0xff0000) >> 16;
          fwrite (&byt, 1, 1, stdout);
          byt = (pixel32 & 0x00ff00) >> 8;
          fwrite (&byt, 1, 1, stdout);
          byt = (pixel32 & 0x0000ff);
          fwrite (&byt, 1, 1, stdout);
     } while (!feof(stdin));
     
     return 0;
}
