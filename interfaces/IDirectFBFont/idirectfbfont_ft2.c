// #define _FILE_OFFSET_BITS 64
#include <sys/sendfile.h>

main() {
  off_t o=0;
  sendfile(1,0,&o,100);
}
