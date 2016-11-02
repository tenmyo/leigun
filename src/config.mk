CROSS=
CC=$(CROSS)gcc
CPP=$(CROSS)g++
RANLIB=$(CROSS)ranlib
AR=$(CROSS)ar

prefix=/usr/local
bindir=$(prefix)/bin
libdir=$(prefix)/lib/softgun/

#CFLAGS=-Wall -pg -O0 -DPROFILE 
CFLAGS=-Wall -O9 -g -fomit-frame-pointer -fno-strict-overflow -Wstrict-overflow=3 -Wno-unused-but-set-variable 
DEFS=-D_GNU_SOURCE
INCLUDES:=-I$(top_srcdir) -I.

# ubuntu requires -nostdlib in compiler call 
# to omit stack protection symbol in object file
SHAREDCFLAGS=-fPIC -nostdlib  -D_SHARED_
SHAREDLDFLAGS=-nostdlib -shared
CYGWIN=$(findstring CYGWIN,$(shell uname))
ifeq ($(shell uname),Linux)
LDFLAGS=-lpthread -lrt -lm -lz -lasound 
else
 ifeq ($(shell uname),FreeBSD)
 LDFLAGS=-lpthread -lm -lz -lSDL
 else
  ifeq ($(CYGWIN),CYGWIN)
   LDFLAGS=-lpthread -lrt -lm -lz
  else
   $(error "Unknown architecture $(shell uname)")
  endif
 endif
endif

.SUFFIXES: .c .o .be.o
.PHONY: clean all

%.be.o: %.c
	$(CC) $(CFLAGS) $(DEFS)  $(INCLUDES) -DTARGET_BIG_ENDIAN=1 -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -DTARGET_BIG_ENDIAN=0 -o $@ -c $<


# Position independent version
%.po: %.c
	$(CC) $(SHAREDCFLAGS) $(CFLAGS) $(DEFS) $(INCLUDES) -DTARGET_BIG_ENDIAN=0 -o $@ -c $<

%.be.po: %.c
	$(CC) $(SHAREDCFLAGS) $(CFLAGS) $(DEFS) $(INCLUDES) -DTARGET_BIG_ENDIAN=1 -o $@ -c $<
