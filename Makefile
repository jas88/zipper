BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /usr/local)
LIBARCHIVE_PREFIX := $(shell brew --prefix libarchive 2>/dev/null || echo $(BREW_PREFIX)/opt/libarchive)

CFLAGS	:= -O2 -Wall -Wextra -Werror -I$(LIBARCHIVE_PREFIX)/include $(CFLAGS)
LDFLAGS	:= -larchive -L$(LIBARCHIVE_PREFIX)/lib $(shell pkg-config --static --libs bzip2 2>/dev/null) $(shell pkg-config --static --libs libzstd 2>/dev/null) $(shell pkg-config --static --libs libdeflate 2>/dev/null) $(shell pkg-config --static --libs liblzma 2>/dev/null)

CFLAGS += $(shell pkg-config --cflags bzip2 2>/dev/null) $(shell pkg-config --cflags libzstd 2>/dev/null) $(shell pkg-config --cflags libdeflate 2>/dev/null) $(shell pkg-config --cflags liblzma 2>/dev/null)

ifneq ($(shell uname -s),Darwin)
LDFLAGS	+=  -lacl	 -s -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	gcc -o $@ $< -lz $(LDFLAGS)

zipper.exe:
	gcc -Wall -Wextra -Werror -o zipper.exe zipper.c -I/mingw64/include -L/mingw64/lib -larchive -D_FILE_OFFSET_BITS=64

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
