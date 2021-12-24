CFLAGS	:= -O2 -Wall -Wextra -Werror $(addprefix -I,$(wildcard /usr/local/opt/libarchive/include))
LDFLAGS	:= -larchive $(addprefix -L,$(wildcard /usr/local/opt/libarchive/lib))

ifneq ($(shell uname -s),Darwin)
LDFLAGS	+= -lacl -llzma -lbz2 -lz -s -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	$(CC) -o $@ $< $(LDFLAGS)

zipper.exe:
	gcc -Wall -Wextra -Werror -o zipper.exe zipper.c -I/mingw64/include -L/mingw64/lib -larchive -D_FILE_OFFSET_BITS=64

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
