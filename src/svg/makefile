# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd .. && $(MAKE) svg/all

clean %.a %.$(OBJEXT):
	cd .. && $(MAKE) svg/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
