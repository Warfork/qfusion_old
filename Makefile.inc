# Qfusion source include makefile

VERSION_MAJOR=0
VERSION_SUB=0
VERSION_MINOR=9
VERSION=$(VERSION_MAJOR).$(VERSION_SUB).$(VERSION_MINOR)

CC=gcc

CFLAGS_COMMON=-MD -I./ -DLINUX_VERSION="\"$(QF_VERSION)\""
CFLAGS_RELEASE=-O2 -fno-strict-aliasing -ffast-math -fexpensive-optimizations
CFLAGS_DEBUG=-ggdb
CFLAGS_MODULE=-fPIC

DO_CC=echo "  > Compiling $<" && \
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_EXTRA) -o $@ -c $<

DO_CC_MODULE=$(DO_CC) $(CFLAGS_MODULE)


LDFLAGS_COMMON=-L/usr/local/lib -ldl -lm
LDFLAGS_MODULE=-shared


# this nice line comes from the linux kernel makefile
ARCH:=$(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

BUILD_DEBUG_DIR=debug$(ARCH)
BUILD_RELEASE_DIR=release$(ARCH)
