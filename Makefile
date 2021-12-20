CFLAGS	:= -O2 -Wall -Wextra $(addprefix -I,$(wildcard /usr/local/opt/libarchive/include))
LDFLAGS	:=

ifneq ($(shell uname -s),"Darwin")
LDFLAGS	+= -static
endif

BINS	:= zipper
OBJS	:= zipper.o

all:	$(BINS)

zipper:	$(OBJS)
	$(CC) -o $@ $<

%.o:	%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)

.PHONY:	all clean
