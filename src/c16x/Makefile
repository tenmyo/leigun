top_srcdir=..
include $(top_srcdir)/config.mk
INCLUDES := $(INCLUDES) -I. 

SRCFILES= c161_irq.c c161_mem.c idecode_c16x.c instructions_c16x.c c16x_cpu.c c161_serial.c
SHOBJS = $(SRCFILES:.c=.o)
SHLIBS = c16x.so 

CFLAGS += $(SHAREDCFLAGS)

.PHONY: all

all:  $(SHOBJS) $(SHLIBS)

install:
	install -m 0755 c16x.so $(libdir)



$(SHLIBS): $(SHOBJS)
	$(CC) $(SHAREDLDFLAGS) $(CFLAGS) $(INCLUDES) -DTARGET_BIG_ENDIAN=0  $(SHOBJS) -o $@

clean:
	rm -f *.o  *.so
