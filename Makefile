.POSIX:
ALL_LDFLAGS = -lxcb -lxcb-keysyms -lxcb-cursor $(LDFLAGS)
ALL_CFLAGS = $(CPPFLAGS) $(CFLAGS) -s
ALL_WARNING = $(ALL_CFLAGS) -Wall -Wextra -pedantic
PREFIX = /usr/local
LDLIBS = -lm
BIN_DIR = $(PREFIX)/bin

c = tfwm.c

install: tfwm
	mkdir -p $(BIN_DIR)
	cp -f tfwm $(BIN_DIR)
	chmod 755 $(BIN_DIR)/tfwm

tfwm: $(c)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $^ -o tfwm $(LDLIBS)

clean:
	rm -rf tfwm *.o

uninstall:
	rm -f $(BINDIR)/tfwm

.PHONY: install tfwm clean uninstall

# o = tfwm.o

# tfwm: $(o)
# 	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $^ -o tfwm $(LDLIBS)

# $(o): %.o: %.c
# 	$(CC) -c $^ -o $@
