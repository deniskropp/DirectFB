## Makefile.am for DirectFB/include

includedir = @INCLUDEDIR@

DISTCLEANFILES = directfb_keynames.h directfb_strings.h directfb_water_strings.h

include_HEADERS = \
	directfb++.h		\
	directfb.h		\
	directfb_build.h	\
	directfb_graphics.h	\
	directfb_keyboard.h	\
	directfb_keynames.h	\
	directfb_strings.h	\
	directfb_water_strings.h\
	directfb_util.h		\
	directfb_version.h	\
	directfb_water.h	\
	directfb_windows.h	\
	directfbgl.h		\
	directfbgl2.h		\
	dfb_types.h		\
	dfiff.h			\
	dgiff.h

MKNAMES = $(top_srcdir)/tools/mknames.sh
INCP = $(srcdir)

distclean-local:
	rm -f $(DISTCLEANFILES)

directfb_keynames.h: directfb_keyboard.h $(MKNAMES) Makefile
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@

	@$(MKNAMES) DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol $(INCP)/directfb_keyboard.h \
	   | grep -v DIKS_ENTER >> $@

	@$(MKNAMES) DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier $(INCP)/directfb_keyboard.h \
	   | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


directfb_strings.h: directfb.h $(MKNAMES) Makefile
	@echo '#ifndef __DIRECTFB_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_STRINGS_H__' >> $@
	@echo '#include <directfb.h>' >> $@

	@$(MKNAMES) DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format $(INCP)/directfb.h >> $@

	@$(MKNAMES) DFBSurfacePorterDuffRule DSPD NONE PorterDuffRule rule $(INCP)/directfb.h >> $@

	@$(MKNAMES) DFBSurfaceCapabilities DSCAPS NONE SurfaceCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSCAPS_FLIPPING | grep -v DSCAPS_ALL >> $@

	@$(MKNAMES) DFBSurfaceColorSpace DSCS UNKNOWN ColorSpace colorspace $(INCP)/directfb.h >> $@

	@$(MKNAMES) DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type $(INCP)/directfb.h \
	   | grep -v DIDTF_ALL >> $@

	@$(MKNAMES) DFBSurfaceDrawingFlags DSDRAW NOFX SurfaceDrawingFlags flag $(INCP)/directfb.h \
	   | grep -v DSDRAW_ALL >> $@

	@$(MKNAMES) DFBSurfaceBlittingFlags DSBLIT NOFX SurfaceBlittingFlags flag $(INCP)/directfb.h \
	   | grep -v DSBLIT_ALL >> $@

	@$(MKNAMES) DFBSurfaceFlipFlags DSFLIP NONE SurfaceFlipFlags flag $(INCP)/directfb.h \
	   | grep -v DSFLIP_ALL >> $@

	@$(MKNAMES) DFBSurfaceBlendFunction DSBF UNKNOWN SurfaceBlendFunction function $(INCP)/directfb.h \
	   >> $@

	@echo FIXME: Use DIMCAPS prefix for DFBImageCapabilities
	@$(MKNAMES) DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability $(INCP)/directfb.h \
	   | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY >> $@

	@$(MKNAMES) DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type $(INCP)/directfb.h \
	   | grep -v DLTF_ALL >> $@

	@$(MKNAMES) DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability $(INCP)/directfb.h \
	   | grep -v DLCAPS_ALL >> $@

	@$(MKNAMES) DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode $(INCP)/directfb.h \
	   | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE >> $@

	@$(MKNAMES) DFBWindowCapabilities DWCAPS NONE WindowCapabilities capability $(INCP)/directfb.h \
	   | grep -v DWCAPS_ALL >> $@

	@$(MKNAMES) DFBDisplayLayerOptions DLOP NONE DisplayLayerOptions option $(INCP)/directfb.h \
	   | grep -v DLOP_ALL >> $@

	@$(MKNAMES) DFBWindowOptions DWOP NONE WindowOptions option $(INCP)/directfb.h \
	   | grep -v DWOP_ALL >> $@

	@$(MKNAMES) DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSCCAPS_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSECAPS_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type $(INCP)/directfb.h \
	   | grep -v DSET_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard $(INCP)/directfb.h \
	   | grep -v DSETV_ALL >> $@

	@$(MKNAMES) DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSOCAPS_ALL >> $@

	@$(MKNAMES) DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector $(INCP)/directfb.h \
	   | grep -v DSOC_ALL >> $@

	@$(MKNAMES) DFBScreenOutputSignals DSOS NONE ScreenOutputSignals signal $(INCP)/directfb.h \
	   | grep -v DSOS_ALL >> $@

	@$(MKNAMES) DFBScreenOutputSlowBlankingSignals DSOSB OFF ScreenOutputSlowBlankingSignals slow_signal $(INCP)/directfb.h \
	   | grep -v DSOSB_ALL >> $@

	@$(MKNAMES) DFBScreenOutputResolution DSOR UNKNOWN ScreenOutputResolution resolution $(INCP)/directfb.h \
	   | grep -v DSOR_ALL >> $@

	@$(MKNAMES) DFBScreenMixerCapabilities DSMCAPS NONE ScreenMixerCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSMCAPS_ALL >> $@

	@$(MKNAMES) DFBScreenMixerTree DSMT UNKNOWN ScreenMixerTree tree $(INCP)/directfb.h \
	   | grep -v DSMT_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderTestPicture DSETP OFF ScreenEncoderTestPicture test_picture $(INCP)/directfb.h \
	   | grep -v DSETP_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderScanMode DSESM UNKNOWN ScreenEncoderScanMode scan_mode $(INCP)/directfb.h \
	   | grep -v DSESM_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderConfigFlags DSECONF UNKNOWN ScreenEncoderConfigFlags config_flags $(INCP)/directfb.h \
	   | grep -v DSECONF_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderFrequency DSEF UNKNOWN ScreenEncoderFrequency frequency $(INCP)/directfb.h \
	   | grep -v DSEF_ALL >> $@

	@$(MKNAMES) DFBScreenEncoderPictureFraming DSEPF UNKNOWN ScreenEncoderPictureFraming framing $(INCP)/directfb.h \
	   | grep -v DSEPF_ALL >> $@

	@$(MKNAMES) DFBAccelerationMask DFXL NONE AccelerationMask mask $(INCP)/directfb.h \
	   | grep -v DFXL_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@



directfb_water_strings.h: directfb.h $(MKNAMES) Makefile
	@echo '#ifndef __DIRECTFB_WATER_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_WATER_STRINGS_H__' >> $@
	@echo '#include <directfb.h>' >> $@
	@echo '#include <directfb_water.h>' >> $@

	@$(MKNAMES) WaterElementType WET UNKNOWN WaterElementType type $(INCP)/directfb_water.h \
	   | grep -v WET_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@
