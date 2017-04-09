# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd ../.. && $(MAKE) ui/view/all

clean %.a %.$(OBJEXT):
	cd ../.. && $(MAKE) ui/view/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
