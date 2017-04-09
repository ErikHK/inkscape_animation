# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd ../.. && $(MAKE) live_effects/parameter/all

clean %.a %.$(OBJEXT):
	cd ../.. && $(MAKE) live_effects/parameter/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
