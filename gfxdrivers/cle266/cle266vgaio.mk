# The makefile for the cle266 VGA IO module.
# Build-command: make -f cle266vgaio.mk

.SILENT:

all:
	echo Usage:
	echo     make -f cle266vgaio.mk v24
	echo     make -f cle266vgaio.mk v24-static
	echo     make -f cle266vgaio.mk v26
	echo     make -f cle266vgaio.mk v26-static
	echo
	echo v24: Compile for a 2.4 kernel.
	echo v26: Compile for a 2.6 kernel.
	echo
	echo -static suffix: Module will not depend on devfs
	echo   You will need to manually create a /dev device entry if you use it.
	echo
	echo See cle266vgaio.c for more details.
	echo
	echo You need to have kernel header files matching the version you
	echo are trying to compile to, in /usr/src/linux/include.
	echo
	echo ... the kernel driver has not been built.

v24:
	echo Compiling v2.4 kernel module.
	gcc -D__KERNEL__  -DMODULE -O2 -Wall -I/usr/src/linux/include -c cle266vgaio.c

v24-static:
	echo Compiling v2.4 kernel module not depending on devfs.
	gcc -D__KERNEL__  -DMODULE -DCLE266_STATIC_DEVNUM -O2 -Wall -I/usr/src/linux/include -c cle266vgaio.c

v26:
	echo Compiling v2.6 kernel module.
	gcc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -D__KERNEL__  -DMODULE -DKBUILD_MODNAME=cle266vgaio -DKBUILD_BASENAME=cle266vgaio -I/usr/src/linux/include -c cle266vgaio.c -o cle266vgaio.o
	ld -m elf_i386 -r -o cle266vgaio.ko cle266vgaio.o

v26-static:
	echo Compiling v2.6 kernel module.
	gcc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -DCLE266_STATIC_DEVNUM -D__KERNEL__  -DMODULE -DKBUILD_MODNAME=cle266vgaio -DKBUILD_BASENAME=cle266vgaio -I/usr/src/linux/include -c cle266vgaio.c -o cle266vgaio.o
	ld -m elf_i386 -r -o cle266vgaio.ko cle266vgaio.o

clean:
	rm -f cle266vgaio.o cle266vgaio.ko
