Unichrome driver readme
-----------------------

Recommended VESA framebuffer settings (use them on the kernel command line):

vga=789 video=vesa:ywrap,vram:32

vga=789 sets 800x600 pixels, 32 bits per pixel.
vesa:ywrap is makes it possible to flip the primary surface.
vram:32 forces the VESA framebuffer to use 32Mb video RAM.
Adjust 32 to match your BIOS settings, eg 64 or 128.

(Note: VIA's or DirectFB's viafb should also work.)

Reporting bugs
--------------

If you want to report any problem, please make an effort to figure
out what is going on. Please also provide a small code example that
reliably replicates your problem and only relies on DirectFB.

A single .c file + makefile is preferred, so that it can be built and
tested easily.

Just stating that "this or that doesn't work" is a way of giving the
developer a load of work, beyond fixing the actual problem.

Unimplemented features (TODOs)
------------------------------

* 2.4 and 2.6 kernel patches for ucio.

* Blitting into overlay surfaces.
* Colorkeyed stretch blits.
* Support for interlaced surfaces.
* Second video overlay (for picture-in-picture video)
* HQV video blitter support.
* System->Video RAM blits. (AGPGART support)
(* Drawing and blitting into 8-bit (indexed color) surfaces.)

Limitations (of the hardware)
-----------------------------

* These drawing and blitting flags are not supported:
  DSDRAW_DST_PREMULTIPLY, DSDRAW_DEMULTIPLY
  DSBLIT_SRC_PREMULTIPLY, DSBLIT_DST_PREMULTIPLY, DSBLIT_DEMULTIPLY

* You can not combine source and destination color keying when
  blitting opaque surfaces. Both functions work, just not at the
  same time.
  
* The hardware does not support 24-bit (3 bytes/pixel) surfaces. 

* The blitter does not support YUV->RGB pixel-format conversion.
  Video surfaces does the conversion automatically.

Known bugs and quirks
---------------------

* In underlay mode (see "implemented features", above), the video
  is fully visible where the primary layer's alpha is 255, and
  invisible (= graphics visible) where the alpha is 0.

  In other words: the video layer using the primary layer's alpha
  channel as its own alpha channel, and that is not very practical.

  Fortunately, there is an easy workaround. The following function
  XOR's the alpha channel of a surface. Use it just before flipping
  the primary surface, for example.

  void InvertSurfaceAlpha(IDirectFBSurface* surface)
  {
      int w,h;

      surface->SetColor(surface, 0, 0, 0, 0xff);
      surface->SetDrawingFlags(surface, DSDRAW_XOR);
      surface->GetSize(surface, &w, &h);
      surface->FillRectangle(surface, 0, 0, w, h);
  }

* Do not use the CPU to write into VRAM surfaces, unless where
  absolutely needed (ie system -> video blits). CPU accesses
  into VRAM run at 1/4 speed.

* Colorkeyed stretched blits are not supported => will be software
  rendered => will be very slow, if the source surface is in VRAM.

* Blitting outside the screen is buggy (e.g with negative surface
  coordinates). The result is clipped, but does not look right.
