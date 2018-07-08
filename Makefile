CFLAGS=-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -DG_LOG_DOMAIN=\"jmidiplay\" -O2 -Wall
LIBS=-lglib-2.0 -ljack -lsmf
jmidiplay: jmidiplay.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

.PHONY: clean
clean:
	rm -f jmidiplay
