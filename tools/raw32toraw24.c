#include <endian.h>
#include <unistd.h>

#include <asm/types.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#include <asm/byteorder.h>
#endif

int main()
{
     __u8  byt;
     __u32 pixel32;
     
     do {
          if (read( 0, &pixel32, 4 ) < 4)
               break;          

#if __BYTE_ORDER == __BIG_ENDIAN
          __swab32( pixel32 );
#endif
          
          byt = (pixel32 & 0xff0000) >> 16;
          write (1, &byt, 1);
          byt = (pixel32 & 0x00ff00) >> 8;
          write (1, &byt, 1);
          byt = (pixel32 & 0x0000ff);
          write (1, &byt, 1);
     } while (1);
     
     return 0;
}
