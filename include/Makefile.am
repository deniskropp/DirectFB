## Makefile.am for DirectFB/include

includedir = @INCLUDEDIR@

internalincludedir = $(INTERNALINCLUDEDIR)

MAINTAINERCLEANFILES = directfb_keynames.h directfb_strings.h

EXTRA_DIST = mknames.sh

include_HEADERS = \
	directfb.h		\
	directfb_keyboard.h	\
	directfb_keynames.h	\
	directfb_strings.h	\
	directfb_version.h	\
	dfb_types.h

internalinclude_HEADERS = \
	directfb_internals.h


directfb_keynames.h: directfb_keyboard.h mknames.sh
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@

	./mknames.sh DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol directfb_keyboard.h \
	   | grep -v DIKS_ENTER >> $@

	./mknames.sh DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier directfb_keyboard.h \
	   | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


directfb_strings.h: directfb.h mknames.sh
	@echo '#ifndef __DIRECTFB_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_STRINGS_H__' >> $@

	./mknames.sh DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format directfb.h >> $@

	@echo '' >> $@
	@echo '#endif' >> $@
