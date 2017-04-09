# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd .. && $(MAKE) libuemf/all

clean %.a %.$(OBJEXT):
	cd .. && $(MAKE) libuemf/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
