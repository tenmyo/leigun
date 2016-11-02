include ../config.mk

all: sg_tunctl

ifeq ($(shell uname),Linux) 
sg_tunctl: tunctl.c
	$(CC) $(CFLAGS) tunctl.c -o sg_tunctl

install:
	install -m 0755 sg_tunctl $(bindir)	
else
sg_tunctl:

install:

endif

to_c_array: to_c_array.c
	$(CC) $(CFLAGS) to_c_array.c -o to_c_array 

# Should be compiled with target compiler

romdump: romdump.c
	$(CC) $(CFLAGS) $< -o $@ 

clean:
	rm -f *.o sg_tunctl
