CFLAGS=-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -DG_LOG_DOMAIN=\"jmidiplay\" -O2 -Wall
LIBS=-lglib-2.0 -ljack -lsmf
all: jmidiplay jmidirec
jmidiplay: jmidiplay.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
jmidirec: jmidirec.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
clean:
	rm -f jmidiplay jmidirec
.PHONY: clean all
