#include <endian.h>
#include <unistd.h>

int main()
{
     unsigned char  byt;
     unsigned short wrd;
     
     do {
          if (read( 0, &wrd, 2 ) < 2)
               break;
          
#if __BYTE_ORDER == __BIG_ENDIAN
          swab(&wrd, &wrd, 2);          
#endif
          
          byt = (wrd & 0xf800) >> 8;
          write (1, &byt, 1);
          byt = (wrd & 0x07E0) >> 3;
          write (1, &byt, 1);
          byt = (wrd & 0x001F) << 3;
          write (1, &byt, 1);
     } while (1);

     return 0;     
}
