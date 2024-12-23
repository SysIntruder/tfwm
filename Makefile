.POSIX:
ALL_LDFLAGS = -lxcb -lxcb-keysyms $(LDFLAGS)
ALL_CFLAGS = $(CPPFLAGS) $(CFLAGS)
ALL_WARNING = $(ALL_CFLAGS) -Wall -Wextra -pedantic
PREFIX = /usr/local
LDLIBS = -lm
BIN_DIR = $(PREFIX)/bin

objects = tfwm.o

install: tfwm
	mkdir -p $(BIN_DIR)
	cp -f tfwm $(BIN_DIR)
	chmod 755 $(BIN_DIR)/tfwm

tfwm: $(objects)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $^ -o tfwm $(LDLIBS)

$(objects): %.o: %.c
	$(CC) -c $^ -o $@

clean:
	rm -rf tfwm *.o

uninstall:
	rm -f $(BINDIR)/tfwm

.PHONY: tfwm install uninstall clean
