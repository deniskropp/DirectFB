%.o: .libs/%.a %.la
	rm -f $<.tmp/*.o
	if test -d $<.tmp; then rmdir $<.tmp; fi
	mkdir $<.tmp
	(cd $<.tmp && $(AR) x ../../$<)
	$(LD) -o $@ -r $<.tmp/*.o
	rm -f $<.tmp/*.o && rmdir $<.tmp

.PHONY: $(LTLIBRARIES:%.la=.libs/%.a)
