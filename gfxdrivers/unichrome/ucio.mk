# The makefile for the Unichrome IO module.
# Build-command: make -f ucio.mk

.SILENT:

all:
	echo Usage:
	echo     make -f ucio.mk v24-devfs
	echo     make -f ucio.mk v24
	echo     make -f ucio.mk v26-devfs
	echo     make -f ucio.mk v26
	echo
	echo v24: Compile for a 2.4 kernel.
	echo v26: Compile for a 2.6 kernel.
	echo
	echo -devfs suffix: Module will use devfs.
	echo   You will need to manually create a /dev device entry
	echo   if you do not use it.
	echo
	echo See ucio.c for more details.
	echo
	echo You need to have kernel header files matching the version you
	echo are trying to compile to, in /usr/src/linux/include.
	echo
	echo ... the kernel driver has not been built.

v24-devfs:
	echo Compiling v2.4 kernel module with devfs support.
	gcc -D__KERNEL__  -DMODULE -O2 -Wall -I/usr/src/linux/include -c ucio.c

v24:
	echo Compiling v2.4 kernel module not depending on devfs.
	gcc -D__KERNEL__  -DMODULE -DUC_STATIC_DEVNUM -O2 -Wall -I/usr/src/linux/include -c ucio.c

v26-devfs:
	echo Compiling v2.6 kernel module with devfs support.
	gcc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -D__KERNEL__  -DMODULE -DKBUILD_MODNAME=ucio -DKBUILD_BASENAME=ucio -I/usr/src/linux/include -c ucio.c -o ucio.o
	ld -m elf_i386 -r -o ucio.ko ucio.o

v26:
	echo Compiling v2.6 kernel module.
	gcc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -DUC_STATIC_DEVNUM -D__KERNEL__  -DMODULE -DKBUILD_MODNAME=ucio -DKBUILD_BASENAME=ucio -I/usr/src/linux/include -c ucio.c -o ucio.o
	ld -m elf_i386 -r -o ucio.ko ucio.o

clean:
	rm -f ucio.o ucio.ko
