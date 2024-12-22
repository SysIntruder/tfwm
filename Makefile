.POSIX:
ALL_WARNING = -Wall -Wextra -pedantic
ALL_LDFLAGS = -lxcb -lxcb-keysyms $(LDFLAGS)
ALL_CFLAGS = $(CPPFLAGS) $(CFLAGS) -std=c17 $(ALL_WARNING)
PREFIX = /usr/local
LDLIBS = -lm
BINDIR = $(PREFIX)/bin

all: tfwm
install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f tfwm $(DESTDIR)$(BINDIR)
	chmod 755 $(DESTDIR)$(BINDIR)/tfwm
tfwm: tfwm.o
	$(CC) $(ALL_LDFLAGS) -o tfwm tfwm.o $(LDLIBS)
tfwm.o: tfwm.c tfwm.h config.h
clean:
	rm -f tfwm *.o
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/tfwm
.PHONY: all install uninstall clean
