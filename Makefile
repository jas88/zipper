ifeq ($(shell uname -s),Darwin)
# macOS: Set PKG_CONFIG_PATH for Homebrew libarchive
PKG_CONFIG := PKG_CONFIG_PATH=$(shell brew --prefix libarchive)/lib/pkgconfig:$$PKG_CONFIG_PATH pkg-config
else
PKG_CONFIG := pkg-config
endif

CFLAGS	:= -O2 -Wall -Wextra -Werror $(shell $(PKG_CONFIG) --cflags libarchive 2>/dev/null) $(CFLAGS)
LDFLAGS	:= $(shell $(PKG_CONFIG) --libs libarchive 2>/dev/null) -lz

ifneq ($(shell uname -s),Darwin)
LDFLAGS	+=  -lacl	 -s -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	gcc -o $@ $< $(LDFLAGS)

zipper.exe:
	gcc -Wall -Wextra -Werror -o zipper.exe zipper.c -I/mingw64/include -L/mingw64/lib -larchive -D_FILE_OFFSET_BITS=64

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
