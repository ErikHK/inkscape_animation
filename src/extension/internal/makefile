# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd ../.. && $(MAKE) extension/internal/all

clean %.a %.$(OBJEXT):
	cd ../.. && $(MAKE) extension/internal/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
