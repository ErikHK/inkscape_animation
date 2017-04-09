# Convenience stub makefile to call the real Makefile.



OBJEXT = o

# Explicit so that it's the default rule.
all:
	cd .. && $(MAKE) ui/all

clean %.a %.$(OBJEXT):
	cd .. && $(MAKE) ui/$@

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .a .$(OBJEXT)
