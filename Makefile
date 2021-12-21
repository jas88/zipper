CFLAGS	:= -O2 -Wall -Wextra $(addprefix -I,$(wildcard /usr/local/opt/libarchive/include))
LDFLAGS	:= -larchive $(addprefix -L,$(wildcard /usr/local/opt/libarchive/lib))

ifneq ($(shell uname -s),Darwin)
LDFLAGS	+= -lacl -llzma -lbz2 -lz -s -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	$(CC) -o $@ $< $(LDFLAGS)

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
