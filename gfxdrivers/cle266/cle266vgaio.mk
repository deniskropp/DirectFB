# The makefile for the cle266 VGA IO module.
# Build-command: make -f cle266vgaio.mk

.SILENT:

all:
	echo 2.4 kernel build: make -f cle266vgaio.mk v24
	echo 2.6 kernel build: make -f cle266vgaio.mk v26
	echo You need to have matching kernel header files in /usr/src/linux/include.
	echo The kernel driver has not been built.

v24:
	echo Compiling v2.4 kernel module.
	gcc -D__KERNEL__  -DMODULE -O2 -Wall -I/usr/src/linux/include -c cle266vgaio.c

v26:
	echo Compiling v2.6 kernel module.
	gcc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -D__KERNEL__  -DMODULE -DKBUILD_MODNAME=cle266vgaio -DKBUILD_BASENAME=cle266vgaio -I/usr/src/linux/include -c cle266vgaio.c -o cle266vgaio.o
	ld -m elf_i386 -r -o cle266vgaio.ko cle266vgaio.o

clean:
	rm -f cle266vgaio.o cle266vgaio.ko
