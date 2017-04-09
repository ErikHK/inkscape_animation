# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd ../.. && $(MAKE) ui/dialog/all

clean %.a %.$(OBJEXT):
	cd ../.. && $(MAKE) ui/dialog/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
