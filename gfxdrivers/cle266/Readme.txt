Important: This is alpha software. This means it is for other developers
of DirectFB, or people who are not afraid to muck around with the sources.

How to use it
-------------

Note:
This is for Linux 2.4 only, for now. v2.6 and BSD patches are welcome.

1. Configure the kernel for the VESA framebuffer.
In my testing I've used these settings on the kernel command line:
vga=789 video=vesa:ywrap

vga=789 sets 800x600 pixels, 32 bits per pixel.

2. Configure the kernel for the device file system (devfs)

3. Install the cle266vgaio module. It probes the PCI subsystem and won't
install itself unless it finds the right hardware device. Either dmesg or
lsmod will tell you if it worked.

Then, the DirectFB driver simply makes sure that /dev/cle266vgaio exists and
asks it to mmap the IO registers to user space.

This way, we avoid all the trouble of writing a whole framebuffer driver or
patching the kernel.  And, if you want to go back to software-only rendering,
simply uninstall the cle266vgaio module.

Reporting bugs
--------------

If you want to report any problem, please make an effort to figure
out what is going on. Please also provide a small code example that
reliably replicates your problem and only relies on DirectFB.

A single .c file + makefile is preferred, so that it can be built and
tested easily.

Just stating that "this or that doesn't work" is a way of giving the
developer a load of work, beyond fixing the actual problem.

Implemented features
--------------------

* Drawing and blitting in opaque and blended surfaces.
  Supported surface formats are roughly as follows:
  Source: 8-bit (indexed), 16 or 32-bit (A)RGB
  Destination: 16 or 32-bit (A)RGB
* One video overlay surface.

Unimplemented features (TODOs)
------------------------------

* Hardware revision check. The revision number is currently hardcoded.
  Yes, this is bad! If you get strange looking video, try changing the
  revision number to 0x10 in unichrome.c, driver_init_driver().

* 2.4 and 2.6 kernel patches for cle266vgaio, for people who don't want
  to use modules.

* Blitting into overlay surfaces.
* Colorkeyed stretch blits.
* Support for interlaced surfaces.
* Video surface color keying.
* DVD subsurface (subtitling) layer.
* Video alpha layer.
* Second video overlay (for picture-in-picture video)
* HQV video blitter support.

* System->Video RAM blits. (AGPGART support)
(* Drawing and blitting into 8-bit (indexed color) surfaces.)

Limitations (of the hardware)
-----------------------------

* These drawing and blitting flags are not supported:
  DSDRAW_DST_PREMULTIPLY, DSDRAW_DEMULTIPLY
  DSBLIT_SRC_PREMULTIPLY, DSBLIT_DST_PREMULTIPLY, DSBLIT_DEMULTIPLY

* You can not combine combine source and destination color keying
  when blitting opaque surfaces. Both functions work, just not at
  the same time.
  
* The hardware does not support 24-bit (3 bytes/pixel) surfaces. 

* The blitter does not support YUV->RGB pixel-format conversion.
  (Ok, maybe it does, but there is no information about it.)
  Video surfaces does the conversion automatically.

Known bugs and quirks
---------------------

* Do not use the CPU to write into VRAM surfaces, unless where
  absolutely needed (ie system -> video blits). CPU accesses
  into VRAM run at 1/4 speed.

* Colorkeyed stretched blits are not supported => will be software
  rendered => will be very slow, if the source surface is in VRAM.

* The driver could use a code review, and some more testing.
  There are certainly untested corner-cases...

* Blitting outside the screen is buggy (e.g with negative surface
  coordinates). The result is clipped, but does not look right.

* DirectFB announces the driver as "DirectFB/GraphicsDevice: MMX Software
  Rasterizer 0.1" rather than "Via UniChrome Driver".
