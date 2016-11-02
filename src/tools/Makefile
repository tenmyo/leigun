include ../config.mk

all: sg_tunctl

ifeq ($(shell uname),Linux) 
sg_tunctl: tunctl.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

install:
	install -m 0755 sg_tunctl $(bindir)	
else
sg_tunctl:

install:

endif

to_c_array: to_c_array.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

# Should be compiled with target compiler 

romdump: romdump.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o sg_tunctl
