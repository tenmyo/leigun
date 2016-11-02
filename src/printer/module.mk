PRINTER_SRC := printer/pjl.c printer/dj460interp.c printer/pcl3.c printer/pcl3gui.c printer/printengine.c printer/decompress.c sgstring.c
SRCFILES += $(PRINTER_SRC) 
PRINTER_OBJS := $(PRINTER_SRC:.c=.o)
TARGETS += pcl3gui2png
INSTALL_BINS += pcl3gui2png

pcl3gui2png: $(PRINTER_OBJS) printer/pcl3gui2png.c 
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ 

