## Makefile.am for DirectFB/include

includedir = @INCLUDEDIR@

MAINTAINERCLEANFILES = directfb_keynames.h directfb_strings.h

EXTRA_DIST = mknames.sh

include_HEADERS = \
	directfb.h		\
	directfb_keyboard.h	\
	directfb_keynames.h	\
	directfb_strings.h	\
	directfb_version.h	\
	dfb_types.h

MKNAMES = $(top_srcdir)/include/mknames.sh

directfb_keynames.h: directfb_keyboard.h mknames.sh Makefile
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@

	$(MKNAMES) DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol directfb_keyboard.h \
	   | grep -v DIKS_ENTER >> $@

	$(MKNAMES) DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier directfb_keyboard.h \
	   | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


directfb_strings.h: directfb.h mknames.sh Makefile
	@echo '#ifndef __DIRECTFB_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_STRINGS_H__' >> $@

	$(MKNAMES) DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format directfb.h >> $@

	$(MKNAMES) DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type directfb.h \
	   | grep -v DIDTF_ALL >> $@

	@echo FIXME: Use DIMCAPS prefix for DFBImageCapabilities
	$(MKNAMES) DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability directfb.h \
	   | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY >> $@

	$(MKNAMES) DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type directfb.h \
	   | grep -v DLTF_ALL >> $@

	$(MKNAMES) DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability directfb.h \
	   | grep -v DLCAPS_ALL >> $@

	$(MKNAMES) DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode directfb.h \
	   | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE >> $@

	$(MKNAMES) DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability directfb.h \
	   | grep -v DSCCAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability directfb.h \
	   | grep -v DSECAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type directfb.h \
	   | grep -v DSET_ALL >> $@

	$(MKNAMES) DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard directfb.h \
	   | grep -v DSETV_ALL >> $@

	$(MKNAMES) DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability directfb.h \
	   | grep -v DSOCAPS_ALL >> $@

	$(MKNAMES) DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector directfb.h \
	   | grep -v DSOC_ALL >> $@

	$(MKNAMES) DFBScreenOutputSignals DSOS UNKNOWN ScreenOutputSignals signal directfb.h \
	   | grep -v DSOS_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@
