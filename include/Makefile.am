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
	@echo '#define DirectFBKeySymbolNames(Identifier) struct DFBKeySymbolName { \' >> $@
	@echo '     DFBInputDeviceKeySymbol  symbol; \' >> $@
	@echo '     const char              *name; \' >> $@
	@echo '} Identifier[] = { \' >> $@
	@grep DIKS_ $< | grep -v DIKS_NULL | grep -v DIKS_ENTER | $(PERL) -p -e 's/.*(DIKS_)([\w_]+) .*/     \{ \1\2, \"\2\" \}, \\/' >> $@
	@echo '     { DIKS_NULL, "NULL" } \' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '#define DirectFBKeyIdentifierNames(Identifier) struct DFBKeyIdentifierName { \' >> $@
	@echo '     DFBInputDeviceKeyIdentifier  identifier; \' >> $@
	@echo '     const char                  *name; \' >> $@
	@echo '} Identifier[] = { \' >> $@
	@grep DIKI_ $< | grep -v DIKI_UNKNOWN | grep -v DIKI_NUMBER_OF_KEYS | $(PERL) -p -e 's/.*(DIKI_)([\w_]+),.*/     \{ \1\2, \"\2\" \}, \\/' >> $@
	@echo '     { DIKI_UNKNOWN, "unknown" } \' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '#endif' >> $@
