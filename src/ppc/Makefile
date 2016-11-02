top_srcdir=..
include $(top_srcdir)/config.mk
INCLUDES := $(INCLUDES) -I.

SRCFILES= idecode_ppc.c  instructions_ppc.c  mmu_ppc.c  cpu_ppc.c mpc8xx_mem.c

OBJS_BE=$(SRCFILES:.c=.be.o)
SHLIBS_BE = ppc_be.so

CFLAGS += $(SHAREDCFLAGS)

all: $(OBJS_BE) $(SHLIBS_BE)

install: $(SHLIBS_BE)
	install -m 0755 ppc_be.so $(libdir)


$(SHLIBS_BE): $(OBJS_BE)
	$(CC) $(SHAREDLDFLAGS) $(CFLAGS) $(INCLUDES) $(OBJS_BE) -o $@


clean:
	rm -f *.o *.so 
