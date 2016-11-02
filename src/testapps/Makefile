CC=gcc
CFLAGS=-O9 -D_GNU_SOURCE -Wall

sig_psel_sema: sig_psel_sema.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

clean:
	rm -f sig_psel_sema
