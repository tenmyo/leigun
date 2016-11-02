SRCFILES += c16x/c161_irq.c c16x/c161_mem.c c16x/idecode_c16x.c c16x/instructions_c16x.c c16x/c16x_cpu.c c16x/c161_serial.c

#C16XOBJS := $(C16XSRC:.c=.o)
#TARGETS += c16x.so 

#c16x.so: $(C16XOBJS)
#        $(CC) $(SHAREDLDFLAGS) $(CFLAGS) $(INCLUDES) -DTARGET_BIG_ENDIAN=0  $(C16XOBJS) -o $@
