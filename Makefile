CFLAGS	:= -O2 -Wall -Wextra -Werror $(addprefix -I,$(wildcard /usr/local/opt/libarchive/include)) $(CFLAGS)
LDFLAGS	:= -larchive $(addprefix -L,$(wildcard /usr/local/opt/libarchive/lib)) $(pkg-config --static --libs bzip2) $(pkg-config --static --libs libzstd) $(pkg-config --static --libs libdeflate) $(pkg-config --static --libs liblzma)

CFLAGS += $(pkg-config --cflags bzip2) $(pkg-config --cflags libzstd) $(pkg-config --cflags libdeflate) $(pkg-config --cflags libzstd)

ifneq ($(shell uname -s),Darwin)
LDFLAGS	+=  -lacl	 -s -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	gcc -o $@ $< -llzma -llibz.a $(LDFLAGS)

zipper.exe:
	gcc -Wall -Wextra -Werror -o zipper.exe zipper.c -I/mingw64/include -L/mingw64/lib -larchive -D_FILE_OFFSET_BITS=64

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
