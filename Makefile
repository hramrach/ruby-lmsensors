RUBY ?= ruby

TOPTARGETS := all clean test check install

SUBDIRS := $(wildcard */.)

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

ext/.: ext/Makefile

ext/Makefile:
	cd ext ; $(RUBY) extconf.rb
