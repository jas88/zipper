UNAME_S	:= $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
# macOS: Homebrew's libarchive is keg-only, so point pkg-config at it.
# Dynamic link.
PKG_CONFIG	:= PKG_CONFIG_PATH=$(shell brew --prefix libarchive)/lib/pkgconfig:$$PKG_CONFIG_PATH pkg-config
PC_FLAGS	:=
STATIC_LDFLAGS	:=
else ifneq ($(filter MSYS_NT% CYGWIN_NT%,$(UNAME_S)),)
# Windows: MSYS2 "MSYS" environment (zipper needs POSIX fork/poll/wait), so
# the result depends on msys-2.0.dll and libarchive's DLLs. Dynamic link.
PKG_CONFIG	:= pkg-config
PC_FLAGS	:=
STATIC_LDFLAGS	:=
else
# Linux: fully static binary. --static makes pkg-config emit libarchive's
# private dependencies (zstd, lzma, bz2, zlib, acl, ...), which a static
# link needs; the set varies with how the distro built libarchive.
PKG_CONFIG	:= pkg-config
PC_FLAGS	:= --static
STATIC_LDFLAGS	:= -s -static
endif

WARNFLAGS	:= -O2 -Wall -Wextra -Werror

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

# pkg-config runs inside the recipes so that a failure (e.g. libarchive.pc
# not found) stops the build with pkg-config's own error, and so targets
# that don't need it (clean) never invoke it. set -x echoes the
# fully expanded command.
zipper zipper.exe:	$(OBJS)
	@libs="$$($(PKG_CONFIG) $(PC_FLAGS) --libs libarchive)" && set -x && \
	$(CC) -o $@ $^ $$libs $(STATIC_LDFLAGS)

%.o:	%.c
	@cflags="$$($(PKG_CONFIG) $(PC_FLAGS) --cflags libarchive)" && set -x && \
	$(CC) -c -o $@ $< $(WARNFLAGS) $$cflags $(CFLAGS)

clean:
	$(RM) $(BINS) zipper.exe $(OBJS)

.PHONY:	all clean
