## Makefile.am for DirectFB/include

includedir = @INCLUDEDIR@

internalincludedir = $(INTERNALINCLUDEDIR)

MAINTAINERCLEANFILES = directfb_keynames.h

include_HEADERS = \
	directfb.h		\
	directfb_keyboard.h	\
	directfb_keynames.h

internalinclude_HEADERS = \
	directfb_internals.h	\
	directfb_version.h


directfb_keynames.h: directfb_keyboard.h
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@
	@echo '' >> $@
	@echo '#define DirectFBKeySymbolNames(identifier) struct DFBKeySymbolName { \' >> $@
	@echo '     DFBInputDeviceKeySymbol  symbol; \' >> $@
	@echo '     const char              *name; \' >> $@
	@echo '} identifier[] = { \' >> $@
	@grep DIKS_ $< | grep -v DIKS_NULL | grep -v DIKS_ENTER | $(PERL) -p -e 's/.*(DIKS_)([\w_]+) .*/     \{ \1\2, \"\2\" \}, \\/' >> $@
	@echo '     { DIKS_NULL, "NULL" } \' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '#endif' >> $@
