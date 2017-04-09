# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd ../.. && $(MAKE) ui/cache/all

clean %.a %.$(OBJEXT):
	cd ../.. && $(MAKE) ui/cache/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
