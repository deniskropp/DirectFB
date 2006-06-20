## Makefile.am for DirectFB/include

includedir = @INCLUDEDIR@

EXTRA_DIST = mknames.sh

include_HEADERS = \
	directfb.h		\
	directfb_keyboard.h	\
	directfb_keynames.h	\
	directfb_strings.h	\
	directfb_util.h		\
	directfb_version.h	\
	dfb_types.h

MKNAMES = $(top_srcdir)/include/mknames.sh
INCP = $(top_srcdir)/include

distclean-local:
	rm -f directfb_keynames.h directfb_strings.h

directfb_keynames.h: directfb_keyboard.h mknames.sh Makefile
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@

	$(MKNAMES) DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol $(INCP)/directfb_keyboard.h \
	   | grep -v DIKS_ENTER >> $@

	$(MKNAMES) DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier $(INCP)/directfb_keyboard.h \
	   | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


directfb_strings.h: directfb.h mknames.sh Makefile
	@echo '#ifndef __DIRECTFB_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_STRINGS_H__' >> $@

	$(MKNAMES) DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format $(INCP)/directfb.h >> $@

	$(MKNAMES) DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type $(INCP)/directfb.h \
	   | grep -v DIDTF_ALL >> $@

	$(MKNAMES) DFBSurfaceDrawingFlags DSDRAW NOFX SurfaceDrawingFlags flag $(INCP)/directfb.h \
	   | grep -v DSDRAW_ALL >> $@

	$(MKNAMES) DFBSurfaceBlittingFlags DSBLIT NOFX SurfaceBlittingFlags flag $(INCP)/directfb.h \
	   | grep -v DSBLIT_ALL >> $@

	@echo FIXME: Use DIMCAPS prefix for DFBImageCapabilities
	$(MKNAMES) DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability $(INCP)/directfb.h \
	   | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY >> $@

	$(MKNAMES) DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type $(INCP)/directfb.h \
	   | grep -v DLTF_ALL >> $@

	$(MKNAMES) DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability $(INCP)/directfb.h \
	   | grep -v DLCAPS_ALL >> $@

	$(MKNAMES) DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode $(INCP)/directfb.h \
	   | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE >> $@

	$(MKNAMES) DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSCCAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSECAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type $(INCP)/directfb.h \
	   | grep -v DSET_ALL >> $@

	$(MKNAMES) DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard $(INCP)/directfb.h \
	   | grep -v DSETV_ALL >> $@

	$(MKNAMES) DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSOCAPS_ALL >> $@

	$(MKNAMES) DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector $(INCP)/directfb.h \
	   | grep -v DSOC_ALL >> $@

	$(MKNAMES) DFBScreenOutputSignals DSOS NONE ScreenOutputSignals signal $(INCP)/directfb.h \
	   | grep -v DSOS_ALL >> $@

	$(MKNAMES) DFBScreenOutputSlowBlankingSignals DSOSB OFF ScreenOutputSlowBlankingSignals slow_signal $(INCP)/directfb.h \
	   | grep -v DSOSB_ALL >> $@

	$(MKNAMES) DFBScreenMixerCapabilities DSMCAPS NONE ScreenMixerCapabilities capability $(INCP)/directfb.h \
	   | grep -v DSMCAPS_ALL >> $@

	$(MKNAMES) DFBScreenMixerTree DSMT UNKNOWN ScreenMixerTree tree $(INCP)/directfb.h \
	   | grep -v DSMT_ALL >> $@

	$(MKNAMES) DFBScreenEncoderTestPicture DSETP OFF ScreenEncoderTestPicture test_picture $(INCP)/directfb.h \
	   | grep -v DSETP_ALL >> $@

	$(MKNAMES) DFBScreenEncoderScanMode DSESM UNKNOWN ScreenEncoderScanMode scan_mode $(INCP)/directfb.h \
	   | grep -v DSESM_ALL >> $@

	$(MKNAMES) DFBAccelerationMask DFXL NONE AccelerationMask mask $(INCP)/directfb.h \
	   | grep -v DFXL_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@
